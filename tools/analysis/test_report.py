"""H24 schema validation, immutability, and optional compiled end-to-end smoke."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "report.py"
METRICS = dict(completed=2, average_wait_ticks=3.0, p50_wait_ticks=3.0,
               p95_wait_ticks=5.0, average_response_ticks=3.0,
               p95_response_ticks=5.0, average_turnaround_ticks=6.0,
               p95_turnaround_ticks=8.0, throughput_per_tick=0.2,
               worker_utilization=0.5, max_wait_ticks=5, starvation_count=0,
               page_hits=7, page_faults=3, page_evictions=1,
               peak_resident_frames=2, total_blocked_ticks=2)
INPUT = [dict(id=1, arrival_tick=0, total_tokens=5, priority=1),
         dict(id=2, arrival_tick=1, total_tokens=3, priority=2)]


class ReportTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.raw = self.base / "raw"
        (self.raw / "runs").mkdir(parents=True)
        self.out = self.base / "report"
        self.manifest = {"schema": "os-llm-experiments/v1",
            "config": {"workers": 2}, "provenance": {
                "source_claim": "synthetic_fixture", "source_verified": False},
            "campaigns": {"E1": {"runs": []}, "E2": {"runs": []},
                          "E3": {"runs": []}, "E4": {"runs": []}}}
        self.add_run("E2", "fcfs", "FCFS")
        self.add_run("E2", "sjf", "SJF")
        self.add_run("E3", "memory_fifo_f2", "FCFS")
        self.save()

    def add_run(self, group, name, policy):
        raw = {"schema": "os-llm-run", "finished": True,
               "config": {"policy": policy}, "input": copy.deepcopy(INPUT),
               "metrics": copy.deepcopy(METRICS), "trace_fingerprint": "abc"}
        (self.raw / "runs" / (name + ".json")).write_text(json.dumps(raw))
        self.manifest["campaigns"][group]["runs"].append({
            "name": name, "status": "completed", "artifact": "runs/" + name + ".json",
            "config": copy.deepcopy(raw["config"]), "metrics": copy.deepcopy(raw["metrics"]),
            "trace_fingerprint": "abc"})

    def save(self):
        (self.raw / "experiments.json").write_text(json.dumps(self.manifest))

    def invoke(self):
        return subprocess.run([sys.executable, str(SCRIPT),
            "--experiments", str(self.raw), "--output-dir", str(self.out)],
            capture_output=True, text=True, check=False)

    def test_verified_measurements_and_no_overwrite(self):
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads((self.out / "report.json").read_text())
        self.assertEqual(report["schema"], "os-llm-report/v1")
        self.assertFalse(report["provenance"]["source_verified"])
        self.assertEqual(len(report["scheduling"]), 2)
        self.assertEqual(report["memory"][0]["page_faults"], 3)
        self.assertTrue((self.out / "scheduling_wait.svg").exists())
        self.assertFalse(self.invoke().returncode == 0)

    def test_changed_workload_rejected(self):
        path = self.raw / "runs" / "sjf.json"
        value = json.loads(path.read_text())
        value["input"][0]["total_tokens"] = 99
        path.write_text(json.dumps(value))
        self.assertNotEqual(self.invoke().returncode, 0)
        self.assertFalse(self.out.exists())

    def test_tampered_metric_rejected(self):
        self.manifest["campaigns"]["E2"]["runs"][0]["metrics"]["page_faults"] = 999
        self.save()
        self.assertNotEqual(self.invoke().returncode, 0)
        self.assertFalse(self.out.exists())

    def test_missing_metric_rejected(self):
        path = self.raw / "runs" / "fcfs.json"
        data = json.loads(path.read_text())
        data["metrics"].pop("p95_wait_ticks")
        path.write_text(json.dumps(data))
        self.manifest["campaigns"]["E2"]["runs"][0]["metrics"] = data["metrics"]
        self.save()
        self.assertNotEqual(self.invoke().returncode, 0)

    def test_skipped_only_has_no_fabricated_charts(self):
        self.manifest["campaigns"]["E2"]["runs"] = [{"name": "priority_no_aging", "status": "skipped", "reason": "not exposed"}]
        self.manifest["campaigns"]["E3"]["runs"] = []
        self.save()
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads((self.out / "report.json").read_text())
        self.assertEqual(report["charts"], [])
        self.assertEqual(report["scheduling"], [])
        self.assertFalse((self.out / "memory_faults.svg").exists())

    def test_path_traversal_rejected(self):
        self.manifest["campaigns"]["E2"]["runs"][0]["artifact"] = "../../other.json"
        self.save()
        self.assertNotEqual(self.invoke().returncode, 0)

    @unittest.skipUnless(os.environ.get("OSLLM_CLI") and os.environ.get("OSLLM_RACE") and
                         os.environ.get("OSLLM_DEADLOCK"), "compiled binaries required")
    def test_h23_to_h24_real_binary_smoke(self):
        runner = HERE.parent / "experiments" / "run.py"
        (self.base / "scenario.csv").write_text("1,0,5,0\n2,1,3,1\n")
        measured = subprocess.run([sys.executable, str(runner),
            "--cli", os.environ["OSLLM_CLI"], "--scenario", str(self.base / "scenario.csv"),
            "--output-dir", str(self.base / "measured"),
            "--race-lab", os.environ["OSLLM_RACE"], "--deadlock-lab", os.environ["OSLLM_DEADLOCK"],
            "--frames", "1", "3"], capture_output=True, text=True, check=False, timeout=90)
        self.assertEqual(measured.returncode, 0, measured.stderr)
        self.raw = self.base / "measured"
        self.assertEqual(self.invoke().returncode, 0)
        report = json.loads((self.out / "report.json").read_text())
        self.assertEqual(len(report["scheduling"]), 5)  # FCFS, SJF, RR q1/q3, priority
        self.assertEqual(len(report["memory"]), 4)


if __name__ == "__main__":
    unittest.main()
