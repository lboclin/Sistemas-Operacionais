"""Run reproducibility and failure tests without requiring actual LLM/GPU."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


class ExperimentRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.scenario = self.home / "scenario.csv"
        self.scenario.write_text("1,0,5,0\n2,1,3,1\n", encoding="utf-8")
        self.cli = self.home / "mock.py"
        self.cli.write_text('''#!/usr/bin/env python3
import json,sys
from pathlib import Path
a=sys.argv
def arg(k): return a[a.index(k)+1]
items=[]
for l in Path(arg("--scenario")).read_text().splitlines():
 if not l or l.startswith("#"): continue
 x=list(map(int,l.split(",")))
 items.append(dict(zip(("id","arrival_tick","total_tokens","priority"),x)))
policy={"fcfs":"FCFS","sjf":"SJF","rr":"Round-Robin","priority":"Priority-aging"}[arg("--policy")]
Path(arg("--export-json")).write_text(json.dumps({"schema":"os-llm-run","finished":True,"input":items,"config":{"policy":policy},"metrics":{"completed":len(items),"average_wait_ticks":0.0,"page_faults":0},"trace_fingerprint":"abc"}))
''', encoding="utf-8")
        self.cli.chmod(0o755)

    def run_script(self, extra=None):
        command = [sys.executable, str(HERE / "run.py"), "--cli", str(self.cli),
                   "--scenario", str(self.scenario), "--output-dir", str(self.home / "out"),
                   "--frames", "1", "3"] + (extra or [])
        return subprocess.run(command, capture_output=True, text=True, timeout=30, check=False)

    def test_campaigns_and_no_overwrite(self):
        run = self.run_script()
        self.assertEqual(run.returncode, 0, run.stderr)
        result = json.loads((self.home / "out" / "experiments.json").read_text())
        self.assertEqual(result["schema"], "os-llm-experiments/v1")
        self.assertEqual(set(result["campaigns"]), {"E1", "E2", "E3", "E4"})
        self.assertEqual(len(result["campaigns"]["E3"]["runs"]), 5)
        self.assertEqual(result["campaigns"]["E2"]["runs"][-1]["status"], "skipped")
        self.assertFalse(self.run_script().returncode == 0)

    def test_duplicate_ids_fail_without_output(self):
        self.scenario.write_text("1,0,5,0\n1,1,3,1\n")
        self.assertNotEqual(self.run_script().returncode, 0)
        self.assertFalse((self.home / "out").exists())

    @unittest.skipUnless(os.environ.get("OSLLM_CLI") and os.environ.get("OSLLM_RACE")
                         and os.environ.get("OSLLM_DEADLOCK"), "requires compiled binaries")
    def test_full_binary_smoke(self):
        argv = [sys.executable, str(HERE / "run.py"),
                "--cli", os.environ["OSLLM_CLI"], "--scenario", str(self.scenario),
                "--output-dir", str(self.home / "actual"),
                "--race-lab", os.environ["OSLLM_RACE"],
                "--deadlock-lab", os.environ["OSLLM_DEADLOCK"], "--frames", "1", "3"]
        run = subprocess.run(argv, capture_output=True, text=True, timeout=90, check=False)
        self.assertEqual(run.returncode, 0, run.stderr)
        report = json.loads((self.home / "actual" / "experiments.json").read_text())
        self.assertEqual(report["campaigns"]["E1"]["runs"][0]["status"], "completed")
        self.assertEqual(report["campaigns"]["E4"]["runs"][1]["status"], "completed")


if __name__ == "__main__":
    unittest.main()
