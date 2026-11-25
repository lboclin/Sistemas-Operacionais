#!/usr/bin/env python3
"""HRRN against an exact oracle, and SJF starvation on scenarios/starvation.csv."""
from fractions import Fraction
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

CLI = os.environ.get("OSLLM_CLI")
SCENARIO = Path(__file__).resolve().parents[2] / "scenarios" / "starvation.csv"
LONG_ID = 5
THRESHOLD = 320


def execute(scenario, policy, *flags):
    with tempfile.TemporaryDirectory() as folder:
        export = Path(folder) / "run.json"
        subprocess.run([CLI, "--scenario", str(scenario), "--policy", policy, "--workers", "1",
                        "--starvation-threshold", str(THRESHOLD), "--export-json", str(export), *flags],
                       check=True, capture_output=True, timeout=20)
        return json.loads(export.read_text(encoding="utf-8"))


def hrrn_oracle(rows):
    """First-run tick per ID with exact rational ratios, one worker, no preemption."""
    order = {r[0]: i for i, r in enumerate(rows)}  # input order breaks same-tick arrivals
    pending, tick, first = list(rows), 0, {}
    while pending:
        ready = [r for r in pending if r[1] <= tick]
        if not ready:
            tick = min(r[1] for r in pending)
            continue
        chosen = min(ready, key=lambda r: (-Fraction(tick - r[1] + r[2], r[2]), r[1], order[r[0]]))
        first[chosen[0]] = tick
        tick += chosen[2]
        pending.remove(chosen)
    return first


@unittest.skipUnless(CLI, "compiled os_llm_cli is required")
class StarvationScenarioTests(unittest.TestCase):
    def long_wait(self, policy, *flags):
        data = execute(SCENARIO, policy, *flags)
        self.assertEqual(data["metrics"]["completed"], 200)
        wait = next(r["waiting_ticks"] for r in data["requests"] if r["id"] == LONG_ID)
        return wait, data["metrics"]

    def test_sjf_starves_the_long_request_until_the_flow_ends(self):
        wait, metrics = self.long_wait("sjf")
        self.assertEqual(wait, 796)  # runs after the 199 short ones
        self.assertEqual(metrics["starvation_count"], 1)

    def test_hrrn_bounds_the_wait_of_the_long_request(self):
        wait, metrics = self.long_wait("hrrn")
        self.assertEqual(wait, 120)
        self.assertEqual(metrics["starvation_count"], 0)
        self.assertEqual(metrics["preemptions"], 0)

    def test_aging_prevents_starvation_under_priority(self):
        without, starved = self.long_wait("priority", "--aging-interval", "0")
        aged, healthy = self.long_wait("priority", "--aging-interval", "2")
        self.assertEqual((without, starved["starvation_count"]), (796, 1))
        self.assertLess(aged, THRESHOLD)
        self.assertEqual(healthy["starvation_count"], 0)

    def test_hrrn_matches_exact_rational_oracle(self):
        for seed in range(12):
            rng = random.Random(7300 + seed)
            rows = [(i + 1, rng.randrange(0, 60), rng.randrange(1, 20), 0) for i in range(40)]
            with tempfile.TemporaryDirectory() as folder:
                scenario = Path(folder) / "s.csv"
                scenario.write_text("".join("%d,%d,%d,%d\n" % r for r in rows), encoding="utf-8")
                data = execute(scenario, "hrrn")
            self.assertEqual(data["config"]["policy"], "HRRN")
            self.assertEqual({r["id"]: r["first_run_tick"] for r in data["requests"]},
                             hrrn_oracle(rows), f"seed {seed}")


if __name__ == "__main__":
    unittest.main()
