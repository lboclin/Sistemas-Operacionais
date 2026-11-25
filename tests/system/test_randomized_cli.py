#!/usr/bin/env python3
"""Deterministic CLI differential/metamorphic stress; no external dependencies."""
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

CLI = os.environ.get("OSLLM_CLI")
POLICIES = ("fcfs", "sjf", "rr", "priority")


@unittest.skipUnless(CLI, "compiled os_llm_cli is required")
class RandomizedCoreTests(unittest.TestCase):
    def run_case(self, root, entries, policy, workers, paging=False, extra=()):
        scenario = root / "workload.csv"
        scenario.write_text("\n".join(",".join(map(str, r)) for r in entries) + "\n", encoding="utf-8")
        export = root / "result.json"
        command = [CLI, "--scenario", str(scenario), "--policy", policy, "--workers", str(workers),
                   "--export-json", str(export), *extra]
        if policy == "rr":
            command += ["--quantum", "2"]
        if paging:
            command += ["--frames", "3", "--page-tokens", "2", "--page-policy", "lru",
                        "--page-fault-latency", "1"]
        completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   check=False, timeout=12)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode(errors="replace"))
        data = json.loads(export.read_text(encoding="utf-8"))
        self.assertEqual(data["schema"], "os-llm-run")
        self.assertTrue(data["finished"])
        self.assertEqual(data["metrics"]["completed"], len(entries))
        self.assertEqual(data["config"]["worker_count"], workers)
        self.assertEqual(len(data["requests"]), len(entries))
        self.assertEqual(len(data["input"]), len(entries))
        expected = {x[0]: x for x in entries}
        self.assertEqual(set(row["id"] for row in data["input"]), set(expected))
        for row in data["input"]:
            rid = row["id"]
            self.assertEqual(tuple(row[k] for k in ("id","arrival_tick","total_tokens",
                "priority")), expected[rid])
        for row in data["requests"]:
            original = expected[row["id"]]
            self.assertEqual(row["state"], "TERMINATED")
            self.assertEqual(row["processed_tokens"], original[2])
            self.assertGreaterEqual(row["first_run_tick"], original[1])
            self.assertGreaterEqual(row["finished_tick"], row["first_run_tick"])
            self.assertGreaterEqual(row["waiting_ticks"], 0)
        events = data["events"]
        self.assertEqual([e["sequence"] for e in events], list(range(len(events))))
        self.assertEqual([e["tick"] for e in events], sorted(e["tick"] for e in events))
        self.assertEqual({e["request_id"] for e in events if e["type"] == "completed"},
                         set(expected))
        self.assertEqual(sum(e["type"] == "completed" for e in events), len(entries))
        if paging:
            self.assertTrue(data["config"]["memory_enabled"])
            self.assertLessEqual(data["metrics"]["peak_resident_frames"], 3)
            self.assertLessEqual(data["metrics"]["page_evictions"],
                                 data["metrics"]["page_faults"])
        else:
            self.assertFalse(data["config"]["memory_enabled"])
            self.assertEqual(data["metrics"]["page_faults"], 0)
        return data

    def test_seeded_policy_paging_and_replay_matrix(self):
        # 6 seeds × 4 policies × 2 memory modes = 48 full C engine runs,
        # plus 16 exact reruns testing trace and metric determinism.
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for seed in range(6):
                rng = random.Random(seed + 4101)
                entries = [(1000+i, rng.randrange(0, 8), rng.randrange(1, 9),
                            rng.randrange(0, 4))
                           for i in range(1 + seed % 7)]
                for policy in POLICIES:
                    for paging in (False, True):
                        workers = 1 + seed % 3
                        first = self.run_case(root, entries, policy, workers, paging)
                        if seed in (0, 5):
                            second = self.run_case(root, entries, policy, workers, paging)
                            self.assertEqual(first["trace_fingerprint"], second["trace_fingerprint"])
                            self.assertEqual(first["metrics"], second["metrics"])
                            self.assertEqual(first["events"], second["events"])

    def test_aging_interval_is_exported_and_zero_disables_aging(self):
        # A low-priority request competing with a continuous high-priority flow.
        entries = [(1, 0, 4, 0)] + [(i + 2, 3 * i, 3, 4) for i in range(20)]
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            waits = {}
            for interval in (None, 0, 2):
                flags = [] if interval is None else ["--aging-interval", str(interval)]
                data = self.run_case(root, entries, "priority", 1, extra=flags)
                self.assertEqual(data["config"]["aging_interval"],
                                 2 if interval is None else interval)
                waits[interval] = next(r["waiting_ticks"] for r in data["requests"] if r["id"] == 1)
            self.assertEqual(waits[None], waits[2])  # the default is unchanged
            self.assertEqual(waits[0], 60)           # without aging it runs last
            self.assertLess(waits[2], waits[0])

    def test_large_valid_arrival_and_negative_cases(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            data = self.run_case(root, [(73, 4294967311, 2, 1)], "fcfs", 1)
            self.assertEqual(data["input"][0]["arrival_tick"], 4294967311)
            # Zero tokens, negative/overflowing arrival, the old five-column
            # format (with a prediction) and duplicate IDs are all rejected.
            for value in ("1,0,0,0", "1,-1,1,0", "1,18446744073709551616,1,0",
                          "1,0,2,2,0", "1,0,1,0\n1,1,1,0"):
                (root / "bad.csv").write_text(value + "\n", encoding="utf-8")
                proc = subprocess.run([CLI, "--scenario", str(root / "bad.csv")],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                      check=False, timeout=5)
                self.assertNotEqual(proc.returncode, 0, value)


if __name__ == "__main__":
    unittest.main()
