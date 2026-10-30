"""Run with python3 -m unittest discover -s tools/predictor -p 'test_*.py'."""
import csv
import json
import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "predictor.py"
FIXTURE = HERE / "synthetic_fixture.csv"


class PredictorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)
        self.model = self.dir / "model.json"
        self.report = self.dir / "report.json"

    def call(self, *args, ok=True):
        proc = subprocess.run([sys.executable, str(SCRIPT), *map(str, args)],
                              capture_output=True, text=True, check=False)
        if ok:
            self.assertEqual(proc.returncode, 0, proc.stderr)
        else:
            self.assertNotEqual(proc.returncode, 0, proc.stdout)
        return proc

    def training(self, source=FIXTURE):
        self.call("train", "--data", source, "--model", self.model, "--report", self.report, "--seed", 19)
        return json.loads(self.model.read_text()), json.loads(self.report.read_text())

    def test_deterministic_split_and_held_out_metrics(self):
        model, report = self.training()
        self.assertEqual(model["schema"], "os-llm-output-predictor/v1")
        self.assertEqual(len(report["train_ids"]), 12)
        self.assertEqual(len(report["test_ids"]), 4)
        self.assertFalse(set(report["train_ids"]) & set(report["test_ids"]))
        self.assertTrue(math.isfinite(report["held_out"]["rmse"]))
        self.assertGreaterEqual(report["held_out"]["mae"], 0)
        original_model, original_report = self.model.read_bytes(), self.report.read_bytes()
        self.training()
        self.assertEqual(original_model, self.model.read_bytes())
        self.assertEqual(original_report, self.report.read_bytes())
        with FIXTURE.open(newline="") as f:
            rows = {int(r["request_id"]): r for r in csv.DictReader(f)}
        means = model["means"]
        self.assertAlmostEqual(means["prompt_tokens"], sum(int(rows[i]["prompt_tokens"]) for i in report["train_ids"]) / 12)

    def test_prediction_ignores_target_and_bounds(self):
        self.training()
        with FIXTURE.open(newline="") as f:
            rows = list(csv.DictReader(f))
        inp = self.dir / "input.csv"
        with inp.open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=rows[0].keys())
            w.writeheader()
            w.writerows(rows)
        out = self.dir / "pred.json"
        self.call("predict", "--data", inp, "--model", self.model, "--output", out)
        original = json.loads(out.read_text())
        self.assertEqual(original["schema"], "os-llm-token-predictions/v1")
        self.assertEqual(len(original["predictions"]), 16)
        for row, prediction in zip(rows, original["predictions"]):
            self.assertEqual(int(row["request_id"]), prediction["request_id"])
            self.assertTrue(1 <= prediction["predicted_tokens"] <= int(row["max_new_tokens"]))
            row["output_tokens"] = "not-even-a-number"  # not a feature
        with inp.open("w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=rows[0].keys())
            w.writeheader()
            w.writerows(rows)
        self.call("predict", "--data", inp, "--model", self.model, "--output", out)
        self.assertEqual(original, json.loads(out.read_text()))

    def test_invalid_data_and_model(self):
        self.training()
        bad = self.dir / "bad.csv"
        bad.write_text("request_id,prompt_tokens,max_new_tokens,task_category,output_tokens\n1,5,10,qa,5\n1,6,10,qa,6\n")
        self.call("train", "--data", bad, "--model", self.model, "--report", self.report, ok=False)
        bad.write_text("request_id,prompt_tokens,max_new_tokens,task_category,output_tokens\n1,5,10,qa,11\n")
        self.call("train", "--data", bad, "--model", self.model, "--report", self.report, ok=False)
        bad.write_text("request_id,prompt_tokens,max_new_tokens,task_category\n2,abc,10,qa\n")
        self.call("predict", "--data", bad, "--model", self.model, "--output", self.dir / "out.json", ok=False)
        model = json.loads(self.model.read_text())
        model["weights"][0] = float("nan")
        self.model.write_text(json.dumps(model))
        self.call("predict", "--data", FIXTURE, "--model", self.model, "--output", self.dir / "out.json", ok=False)

    def test_unseen_category_and_small_dataset(self):
        self.training()
        inp = self.dir / "unseen.csv"
        inp.write_text("request_id,prompt_tokens,max_new_tokens,task_category\n99,40,32,unseen\n")
        out = self.dir / "out.json"
        self.call("predict", "--data", inp, "--model", self.model, "--output", out)
        self.assertTrue(1 <= json.loads(out.read_text())["predictions"][0]["predicted_tokens"] <= 32)
        self.call("train", "--data", inp, "--model", self.model, "--report", self.report, ok=False)


if __name__ == "__main__":
    unittest.main()
