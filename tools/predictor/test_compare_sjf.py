"""Adapter tests; CTest supplies OSLLM_CLI for the real-C end-to-end case."""
import csv
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SCRIPT = HERE / "compare_sjf.py"
DEMO_SCENARIO = ROOT / "scenarios" / "sjf-ai-demo.csv"
DEMO_FEATURES = HERE / "demo_features.csv"


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.model = self.base / "model.json"
        self.model.write_text(json.dumps({
            "schema": "os-llm-output-predictor/v1", "algorithm": "ridge-linear",
            "numeric_features": ["prompt_tokens", "max_new_tokens"],
            "categorical_feature": "task_category",
            "means": {"prompt_tokens": 0, "max_new_tokens": 0},
            "scales": {"prompt_tokens": 10, "max_new_tokens": 100},
            "categories": ["code", "qa", "summary"],
            "weights": [8, 2, 3, 0, 1, 5]
        }), encoding="utf-8")
        self.features = self.base / "features.csv"
        self.features.write_bytes(DEMO_FEATURES.read_bytes())
        self.scenario = self.base / "scenario.csv"
        self.scenario.write_bytes(DEMO_SCENARIO.read_bytes())
        self.out = self.base / "out"
        self.mock = self.base / "mock_cli.py"
        self.mock.write_text('''#!/usr/bin/env python3
import csv,json,sys
from pathlib import Path
args=sys.argv[1:]
def arg(key): return args[args.index(key)+1]
rows=[]
for line in Path(arg('--scenario')).read_text().splitlines():
 if line.startswith('#') or not line.strip(): continue
 rows.append([int(x) for x in next(csv.reader([line]))])
policy=arg('--policy')
names={'fcfs':'FCFS','ema':'SJF-EMA','sjf':'SJF-estimated','oracle':'SJF-oracle'}
if policy not in names: sys.exit(2)
inputs=[dict(zip(('id','arrival_tick','total_tokens','predicted_tokens','priority'),row)) for row in rows]
export={'schema':'os-llm-run','finished':True,'config':{'policy':names[policy]},
 'input':inputs,'metrics':{'completed':len(rows),'average_wait_ticks':{'fcfs':10,'ema':9,'sjf':8,'oracle':7}[policy]},
 'trace_fingerprint':'0000000000000000'}
Path(arg('--export-json')).write_text(json.dumps(export))
''', encoding="utf-8")
        self.mock.chmod(0o755)

    def invoke(self, *, cli=None, output=None, ok=True):
        cmd = [sys.executable, str(SCRIPT), "--scenario", str(self.scenario),
               "--features", str(self.features), "--model", str(self.model),
               "--cli", str(cli or self.mock), "--output-dir", str(output or self.out)]
        proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
        self.assertEqual(proc.returncode == 0, ok, proc.stderr)
        return proc

    def test_four_policies_same_workload_and_deterministic(self):
        self.invoke()
        comparison = json.loads((self.out / "comparison.json").read_text())
        self.assertEqual(list(comparison["runs"]), ["ai", "ema", "fcfs", "oracle"])
        self.assertEqual(comparison["runs"]["ai"]["engine_policy"], "SJF-estimated")
        expected = json.loads((self.out / "predictions.json").read_text())
        self.assertEqual(expected["schema"], "os-llm-token-predictions/v1")
        inputs = [json.loads((self.out / f"{name}.json").read_text())["input"]
                  for name in ("fcfs", "ema", "ai", "oracle")]
        self.assertTrue(all(item == inputs[0] for item in inputs))
        self.assertEqual({r["id"] for r in inputs[0]}, {101, 102, 103, 104, 105})
        first = (self.out / "comparison.json").read_bytes()
        self.invoke(output=self.base / "out2")
        self.assertEqual(first, (self.base / "out2" / "comparison.json").read_bytes())
        self.invoke(ok=False)  # do not overwrite a previous run

    def test_target_tokens_cannot_change_inference(self):
        self.invoke()
        first = (self.out / "predictions.json").read_bytes()
        original = self.scenario.read_text()
        self.scenario.write_text(original.replace("101,0,18,18,1", "101,0,47,18,1"))
        self.invoke(output=self.base / "out2")
        self.assertEqual(first, (self.base / "out2" / "predictions.json").read_bytes())
        self.assertNotEqual(json.loads((self.out / "comparison.json").read_text())["prediction_error_on_scenario"],
                            json.loads((self.base / "out2" / "comparison.json").read_text())["prediction_error_on_scenario"])

    def test_missing_invalid_model_and_forbidden_target_feature(self):
        self.model.unlink()
        self.invoke(ok=False)
        self.model.write_text('{"schema":"bad"}')
        self.invoke(ok=False)
        self.model.write_text(json.dumps({"schema":"os-llm-output-predictor/v1"}))
        self.invoke(ok=False)
        self.features.write_text(DEMO_FEATURES.read_text().replace("task_category", "output_tokens"))
        self.invoke(ok=False)
        self.assertFalse(self.out.exists())

    def test_mismatched_ids_and_invalid_scenario(self):
        self.features.write_text(DEMO_FEATURES.read_text().replace("105,55,96,summary", "999,55,96,summary"))
        self.invoke(ok=False)
        self.features.write_bytes(DEMO_FEATURES.read_bytes())
        self.scenario.write_text(DEMO_SCENARIO.read_text().replace("count=5", "count=6"))
        self.invoke(ok=False)
        self.scenario.write_text(DEMO_SCENARIO.read_text().replace("105,5,22,22,1", "101,5,22,22,1"))
        self.invoke(ok=False)
        self.assertFalse(self.out.exists())

    def test_arrival_tick_full_64bit_range(self):
        text = DEMO_SCENARIO.read_text(encoding="utf-8")
        self.scenario.write_text(text.replace("101,0,18,18,1",
            "101,4294967296,18,18,1"), encoding="utf-8")
        self.invoke()
        rows = json.loads((self.out / "fcfs.json").read_text())["input"]
        self.assertEqual(rows[0]["arrival_tick"], 4294967296)
        self.scenario.write_text(text.replace("101,0,18,18,1",
            "101,18446744073709551616,18,18,1"), encoding="utf-8")
        self.invoke(output=self.base / "bad", ok=False)
        self.assertFalse((self.base / "bad").exists())

    @unittest.skipUnless(os.environ.get("OSLLM_CLI"), "requires compiled C CLI via CTest")
    def test_real_c_cli_end_to_end(self):
        # Train the real G20 model on synthetic IDs 1..16, then predict for
        # distinct demo IDs 101..105 before exercising the compiled C CLI.
        trained = subprocess.run([sys.executable, str(HERE / "predictor.py"),
            "train", "--data", str(HERE / "synthetic_fixture.csv"),
            "--model", str(self.model), "--report", str(self.base / "training.json"),
            "--seed", "42"], capture_output=True, text=True, check=False)
        self.assertEqual(trained.returncode, 0, trained.stderr)
        training = json.loads((self.base / "training.json").read_text())
        self.assertFalse(set(training["train_ids"]) & set(training["test_ids"]))
        self.assertFalse(set(training["train_ids"] + training["test_ids"]) & {101, 102, 103, 104, 105})
        self.invoke(cli=Path(os.environ["OSLLM_CLI"]))
        runs = {name: json.loads((self.out / f"{name}.json").read_text())
                for name in ("fcfs", "ema", "ai", "oracle")}
        self.assertEqual({r["metrics"]["completed"] for r in runs.values()}, {5})
        self.assertEqual({r["config"]["worker_count"] for r in runs.values()}, {1})
        self.assertEqual({tuple((x["id"], x["total_tokens"]) for x in r["input"])
                          for r in runs.values()}, {((101, 18), (102, 5), (103, 30), (104, 9), (105, 22))})
        for run in runs.values():
            self.assertTrue(all(x["state"] == "TERMINATED" for x in run["requests"]))


if __name__ == "__main__":
    unittest.main()
