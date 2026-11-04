"""Offline tests: deterministic workloads and local mock OpenAI-compatible server."""
import contextlib
import csv
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest

import capture


class MockHandler(BaseHTTPRequestHandler):
    mode = "ok"
    requests = []

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length))
        type(self).requests.append(payload)
        if self.path != "/v1/chat/completions":
            self.send_error(404)
            return
        if type(self).mode == "http_error" or (type(self).mode == "second_error" and len(type(self).requests) > 1):
            self.send_error(500)
            return
        if type(self).mode == "timeout":
            time.sleep(0.3)
        if type(self).mode == "bad_json":
            body = b"{invalid"
        else:
            response = {"model": "mock-small-llm", "usage": {"prompt_tokens": 17, "completion_tokens": 7},
                        "choices": [{"message": {"content": "PRIVATE ANSWER"}, "finish_reason": "stop"}]}
            if type(self).mode == "missing_usage":
                response.pop("usage")
            if type(self).mode == "oversize_usage":
                response["usage"]["completion_tokens"] = 999999
            body = json.dumps(response).encode()
        try:
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except BrokenPipeError:
            pass

    def log_message(self, *args):
        pass


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def prompts(self, rows=None):
        if rows is None:
            rows = [{"request_id": 3, "prompt": "PRIVATE PROMPT", "task_category": "qa",
                     "max_new_tokens": 32, "priority": 2}]
        path = self.root / "prompts.jsonl"
        path.write_text("\n".join(json.dumps(r) for r in rows) + "\n", encoding="utf-8")
        return path

    @contextlib.contextmanager
    def server(self, mode="ok"):
        MockHandler.mode, MockHandler.requests = mode, []
        server = ThreadingHTTPServer(("127.0.0.1", 0), MockHandler)
        server.daemon_threads = True
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            yield f"http://127.0.0.1:{server.server_port}/v1/chat/completions"
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=2)

    def collect_args(self, endpoint, prompt_file, output=None, timeout=1.0):
        return ["collect", "--endpoint", endpoint, "--model", "mock-small-llm",
                "--prompts", str(prompt_file), "--output", str(output or self.root / "capture.json"),
                "--timeout", str(timeout)]

    def test_mock_capture_and_scenario_integration(self):
        prompts = self.prompts()
        with self.server() as url:
            self.assertEqual(capture.main(self.collect_args(url, prompts)), 0)
        path = self.root / "capture.json"
        data = capture.load_capture(path)
        self.assertEqual(data["source"], "observed_llm")
        self.assertEqual(data["records"][0]["prompt_tokens"], 17)
        self.assertEqual(data["records"][0]["output_tokens"], 7)
        self.assertEqual(data["records"][0]["output_chars"], len("PRIVATE ANSWER"))
        self.assertGreaterEqual(data["records"][0]["latency_ms"], 0)
        self.assertNotIn("PRIVATE PROMPT", path.read_text())
        self.assertNotIn("PRIVATE ANSWER", path.read_text())
        self.assertEqual(MockHandler.requests[0]["max_tokens"], 32)
        self.assertEqual(MockHandler.requests[0]["stream"], False)
        out = self.root / "generated"
        self.assertEqual(capture.main(["generate", "--capture", str(path), "--count", "3", "--seed", "8", "--output-dir", str(out)]), 0)
        scenario = out / "scenario.csv"
        lines = scenario.read_text().splitlines()
        self.assertEqual(lines[0], "# os-llm-scenario v2 seed=8 distribution=uniform count=3")
        self.assertEqual(len(lines), 4)
        for line in lines[1:]:
            fields = line.split(",")
            self.assertEqual(len(fields), 4)
            self.assertEqual(int(fields[2]), 17)  # observed input (prompt) tokens
        self.assertFalse((out / "features.csv").exists())
        self.assertEqual(json.loads((out / "scenario.meta.json").read_text())["source"], "observed_llm")
        cli = os.environ.get("OSLLM_CLI")
        if cli:
            for policy in ("fcfs", "sjf", "rr", "priority"):
                command = [cli, "--scenario", str(scenario), "--policy", policy]
                result = subprocess.run(command, capture_output=True, text=True, timeout=15)
                self.assertEqual(result.returncode, 0, (policy, result.stderr))

    def test_synthetic_deterministic_and_profiles(self):
        a, b, c = (self.root / name for name in ("a", "b", "c"))
        common = ["generate", "--count", "12", "--seed", "2026", "--distribution", "burst", "--profile", "memory-pressure"]
        self.assertEqual(capture.main(common + ["--output-dir", str(a)]), 0)
        self.assertEqual(capture.main(common + ["--output-dir", str(b)]), 0)
        self.assertEqual(capture.main(["generate", "--count", "12", "--seed", "2027", "--distribution", "burst", "--profile", "memory-pressure", "--output-dir", str(c)]), 0)
        for name in ("scenario.csv", "scenario.meta.json"):
            self.assertEqual((a / name).read_bytes(), (b / name).read_bytes())
        self.assertNotEqual((a / "scenario.csv").read_bytes(), (c / "scenario.csv").read_bytes())
        meta = json.loads((a / "scenario.meta.json").read_text())
        self.assertEqual(meta["source"], "synthetic_fixture")
        self.assertEqual(meta["input_tokens_origin"], "synthetic_fixture_values")
        self.assertEqual(meta["config"]["arrival_span_effective"], 2)
        self.assertEqual(capture.main(common + ["--output-dir", str(a)]), 2)  # never overwrite

    def test_endpoint_security(self):
        with self.assertRaises(capture.CaptureError):
            capture.endpoint_kind("https://example.org/v1/chat/completions", False)
        with self.assertRaises(capture.CaptureError):
            capture.endpoint_kind("http://example.org/v1/chat/completions", True)
        with self.assertRaises(capture.CaptureError):
            capture.endpoint_kind("http://key:secret@127.0.0.1/v1/chat/completions", False)
        self.assertEqual(capture.endpoint_kind("https://example.org/v1/chat/completions", True), "remote")
        self.assertEqual(capture.endpoint_kind("http://127.0.0.1:1234/v1/chat/completions", False), "loopback")

    def test_reject_invalid_prompts(self):
        valid = {"request_id": 1, "prompt": "text", "task_category": "qa", "max_new_tokens": 16, "priority": 0}
        invalids = [
            [valid, valid],
            [{**valid, "prompt": "x" * 4097}],
            [{**valid, "max_new_tokens": 0}],
            [{**valid, "priority": 9}],
            [{**valid, "task_category": ""}],
            [{**valid, "api_key": "secret"}],
        ]
        for i, rows in enumerate(invalids):
            with self.subTest(i=i):
                with self.assertRaises(capture.CaptureError):
                    capture.load_prompts(self.prompts(rows), 16)
        self.prompts()
        (self.root / "prompts.jsonl").write_text("not-json\n")
        with self.assertRaises(capture.CaptureError):
            capture.load_prompts(self.root / "prompts.jsonl", 16)

    def test_mock_failures_do_not_persist_partial_data(self):
        prompts = self.prompts()
        for mode in ("http_error", "bad_json", "missing_usage", "oversize_usage", "timeout"):
            with self.subTest(mode=mode):
                with self.server(mode) as url:
                    self.assertEqual(capture.main(self.collect_args(url, prompts, timeout=0.1)), 2)
                self.assertFalse((self.root / "capture.json").exists())
        rows = [json.loads(prompts.read_text().splitlines()[0]),
                {"request_id": 4, "prompt": "PRIVATE SECOND", "task_category": "code", "max_new_tokens": 32, "priority": 1}]
        prompts = self.prompts(rows)
        with self.server() as url:
            self.assertEqual(capture.main(self.collect_args(url, prompts)), 0)
        self.assertEqual(len(capture.load_capture(self.root / "capture.json")["records"]), 2)
        (self.root / "capture.json").unlink()
        with self.server("second_error") as url:
            self.assertEqual(capture.main(self.collect_args(url, prompts)), 2)
        self.assertFalse((self.root / "capture.json").exists())
        with self.server() as url:
            self.assertEqual(capture.main(self.collect_args(url, prompts)), 0)
        with self.server() as url:
            self.assertEqual(capture.main(self.collect_args(url, prompts)), 2)  # no overwrite

    def test_malformed_capture_rejected_before_output(self):
        bad = self.root / "bad.json"
        base = json.loads(capture.FIXTURE.read_text())
        for mutation in (lambda x: x.update(schema="wrong"),
                         lambda x: x["records"][0].update(output_tokens=99999),
                         lambda x: x["records"][0].update(prompt="secret"),
                         lambda x: x["records"].append(x["records"][0])):
            data = json.loads(json.dumps(base))
            mutation(data)
            bad.write_text(json.dumps(data))
            output = self.root / "invalid-out"
            self.assertEqual(capture.main(["generate", "--capture", str(bad), "--output-dir", str(output)]), 2)
            self.assertFalse(output.exists())

    def test_staggered_and_priority(self):
        output = self.root / "priority"
        self.assertEqual(capture.main(["generate", "--count", "4", "--seed", "11", "--arrival-span", "9",
                                       "--distribution", "staggered", "--profile", "priority", "--output-dir", str(output)]), 0)
        lines = (output / "scenario.csv").read_text().splitlines()[1:]
        self.assertEqual([int(line.split(",")[1]) for line in lines], [0, 3, 6, 9])
        self.assertTrue(all(0 <= int(line.split(",")[3]) <= 5 for line in lines))


if __name__ == "__main__":
    unittest.main()
