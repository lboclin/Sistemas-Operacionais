"""J30: acceptance tests using the real compiled C engine, not a mock.

The environment variable OSLLM_CLI is set by CTest; direct standalone runs
without it skip these tests rather than reporting fabricated validation.
"""
import http.client
import json
import os
from pathlib import Path
import random
import sys
from threading import Thread
import unittest

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / "apps" / "visual"))
import server


def scenario(count=3):
    return {
        "requests": [
            {"id": str(i + 1), "arrival_tick": str(i % 3),
             "total_tokens": str(2 + i % 5),
             "priority": str((i % 3) - 1)}
            for i in range(count)
        ],
        "policy": "fcfs", "workers": "2", "quantum": "2"
    }


@unittest.skipUnless(os.environ.get("OSLLM_CLI"), "requires compiled OSLLM_CLI")
class RealEngineAcceptance(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cli = Path(os.environ["OSLLM_CLI"])
        if not cls.cli.is_file():
            raise AssertionError("OSLLM_CLI must be a compiled executable")

    def check_artifact(self, result, payload):
        self.assertEqual(result["schema"], "os-llm-run")
        self.assertEqual(result["version"], 2)
        self.assertIs(result["finished"], True)
        self.assertEqual(result["metrics"]["completed"], len(payload["requests"]))
        self.assertEqual(len(result["requests"]), len(payload["requests"]))
        expected = {int(r["id"]): int(r["total_tokens"]) for r in payload["requests"]}
        self.assertEqual({r["id"] for r in result["requests"]}, set(expected))
        for r in result["requests"]:
            self.assertEqual(r["state"], "TERMINATED")
            self.assertEqual(r["processed_tokens"], expected[r["id"]])
            self.assertGreaterEqual(r["first_run_tick"], r["arrival_tick"])
            self.assertGreaterEqual(r["finished_tick"], r["first_run_tick"])
        events = result["events"]
        self.assertTrue(events)
        self.assertEqual([e["sequence"] for e in events], list(range(len(events))))
        self.assertEqual(sorted(e["tick"] for e in events), [e["tick"] for e in events])
        for e in events:
            if e["worker_index"] is not None:
                self.assertLess(e["worker_index"], int(payload["workers"]))

    def test_all_scheduling_policies_with_one_and_two_workers(self):
        for policy in ("fcfs", "sjf", "rr", "priority"):
            for workers in ("1", "2"):
                with self.subTest(policy=policy, workers=workers):
                    payload = scenario(5)
                    payload["policy"], payload["workers"] = policy, workers
                    self.check_artifact(server.simulate(self.cli, payload), payload)

    def test_memory_fifo_lru_with_and_without_fault_latency(self):
        for policy in ("fifo", "lru"):
            for latency in ("0", "2"):
                with self.subTest(policy=policy, latency=latency):
                    payload = scenario(3)
                    payload["memory"] = {
                        "frames": "2", "page_tokens": "2",
                        "policy": policy, "fault_latency": latency}
                    result = server.simulate(self.cli, payload)
                    self.check_artifact(result, payload)
                    self.assertGreater(result["metrics"]["page_faults"], 0)
                    self.assertIn("page_fault", {e["type"] for e in result["events"]})

    def test_arrivals_above_unsigned_32bit_boundary(self):
        payload = scenario(2)
        for i, row in enumerate(payload["requests"]):
            row["arrival_tick"] = str(2**32 + i)
        self.check_artifact(server.simulate(self.cli, payload), payload)

    def test_deterministic_seeded_workloads_across_policies(self):
        rng = random.Random(29026)
        choices = ("fcfs", "sjf", "rr", "priority")
        for iteration in range(18):
            payload = scenario(rng.randint(1, 8))
            payload["policy"] = choices[iteration % len(choices)]
            payload["workers"] = str(rng.randint(1, 3))
            for row in payload["requests"]:
                row["arrival_tick"] = str(rng.randrange(10))
                row["total_tokens"] = str(rng.randint(1, 8))
                row["priority"] = str(rng.randint(-3, 3))
            if iteration % 3 == 0:
                payload["memory"] = {
                    "frames": str(rng.randint(1, 3)), "page_tokens": "2",
                    "policy": ("fifo", "lru")[iteration % 2],
                    "fault_latency": str(iteration % 2)}
            run = server.simulate(self.cli, payload)
            with self.subTest(iteration=iteration):
                self.check_artifact(run, payload)
                if iteration % 6 == 0:
                    repeat = server.simulate(self.cli, payload)
                    self.assertEqual(run["trace_fingerprint"], repeat["trace_fingerprint"])
                    self.assertEqual(run["metrics"], repeat["metrics"])

    def test_maximum_supported_request_count(self):
        payload = scenario(256)
        self.check_artifact(server.simulate(self.cli, payload), payload)

    def test_http_round_trip_uses_compiled_engine(self):
        server.Handler.cli = self.cli
        api = server.ThreadingHTTPServer(("127.0.0.1", 0), server.Handler)
        worker = Thread(target=api.serve_forever, daemon=True)
        worker.start()
        try:
            payload = scenario()
            raw = json.dumps(payload).encode("utf-8")
            conn = http.client.HTTPConnection("127.0.0.1", api.server_port, timeout=10)
            conn.request("POST", "/api/run", body=raw,
                         headers={"Host": f"127.0.0.1:{api.server_port}",
                                  "Content-Type": "application/json"})
            response = conn.getresponse()
            body = response.read()
            self.assertEqual(response.status, 200, body)
            self.check_artifact(json.loads(body), payload)
            self.assertEqual(response.getheader("X-Content-Type-Options"), "nosniff")
            conn.close()
        finally:
            api.shutdown()
            api.server_close()
            worker.join(timeout=4)


class InputHardeningAudit(unittest.TestCase):
    def test_many_invalid_numbers_and_forged_fields(self):
        edits = (
            ("id", "0"), ("id", True), ("arrival_tick", "-1"),
            ("arrival_tick", "18446744073709551615"),
            ("total_tokens", "0"), ("predicted_tokens", "0"),  # old field: forged
            ("priority", "2147483648"), ("priority", "-2147483649"),
            ("total_tokens", "1e5"), ("arrival_tick", " 1"),
            ("id", "01"), ("id", "1;touch /tmp/side_effect"))
        for field, bad in edits:
            payload = scenario()
            payload["requests"][0][field] = bad
            with self.subTest(field=field, bad=bad):
                with self.assertRaises(server.InputError):
                    server.validate(payload)

    def test_rejects_untrusted_lab_commands(self):
        for request in (
            {"lab": "race", "mode": "unsafe", "acknowledge": False},
            {"lab": "deadlock", "mode": "deadlock", "acknowledge": "true"},
            {"lab": "deadlock", "mode": "shell", "acknowledge": True},
            {"lab": "race", "mode": "safe", "acknowledge": True, "path": "/bin/sh"}):
            with self.subTest(request=request):
                with self.assertRaises(server.InputError):
                    server.validate_lab(request)


if __name__ == "__main__":
    unittest.main()
