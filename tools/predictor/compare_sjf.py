#!/usr/bin/env python3
"""Compare FCFS, SJF-EMA, SJF-AI and oracle on one virtual workload.

SJF-AI is the existing C --policy sjf fed with *offline* model predictions;
no Python dependency is added to the C runtime. Scenario targets are loaded
only after all predictions have been calculated from separate metadata.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from predictor import (MODEL_SCHEMA, PREDICTION_SCHEMA, check_model,
                       estimate, read_rows)

SCHEMA = "os-llm-sjf-comparison/v1"
MAX_REQUESTS = 256
UINT32_MAX = 2**32 - 1
UINT64_MAX = 2**64 - 1
HEADER = re.compile(r"# os-llm-scenario v1 seed=([0-9]+) distribution=(uniform|burst|staggered) count=([0-9]+)\s*$")
FEATURES = {"request_id", "prompt_tokens", "max_new_tokens", "task_category"}
POLICIES = (("fcfs", "fcfs", "FCFS"),
            ("ema", "ema", "SJF-EMA"),
            ("ai", "sjf", "SJF-estimated"),
            ("oracle", "oracle", "SJF-oracle"))


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_predictions(model_path, feature_path):
    model = json.loads(Path(model_path).read_text(encoding="utf-8"))
    check_model(model)
    with open(feature_path, newline="", encoding="utf-8-sig") as stream:
        fields = csv.DictReader(stream).fieldnames or []
    if len(fields) != len(set(fields)) or set(fields) != FEATURES:
        raise ValueError("features CSV must contain exactly request_id,prompt_tokens,max_new_tokens,task_category (no output target)")
    rows = read_rows(feature_path, training=False)
    if len(rows) > MAX_REQUESTS:
        raise ValueError("too many feature rows")
    return {row["request_id"]: estimate(row, model) for row in rows}


def bounded_int(value, label, minimum=0, maximum=UINT32_MAX):
    value = value.strip()
    if not value.isascii() or not value.isdecimal():
        raise ValueError(f"invalid {label}")
    number = int(value)
    if not minimum <= number <= maximum:
        raise ValueError(f"{label} out of range")
    return number


def signed_priority(value):
    value = value.strip()
    if not re.fullmatch(r"[+-]?[0-9]+", value):
        raise ValueError("invalid priority")
    number = int(value)
    if not -(2**31) <= number <= 2**31 - 1:
        raise ValueError("priority out of range")
    return number


def load_scenario(path):
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    result, seen, preserved = [], set(), []
    declared = None
    for number, line in enumerate(lines, start=1):
        if line.startswith("# os-llm-scenario"):
            match = HEADER.fullmatch(line)
            if match is None or declared is not None or result or number != 1:
                raise ValueError(f"invalid v1 scenario header on line {number}")
            declared = bounded_int(match.group(3), "count", 1, MAX_REQUESTS)
            preserved.append(line)
            continue
        if not line.strip() or line.startswith("#"):
            preserved.append(line)
            continue
        values = next(csv.reader([line], skipinitialspace=True))
        if len(values) != 5:
            raise ValueError(f"scenario line {number}: expected five CSV fields")
        rid = bounded_int(values[0], "request ID", 1)
        arrival = bounded_int(values[1], "arrival tick", maximum=UINT64_MAX)
        total = bounded_int(values[2], "total tokens", 1)
        predicted = bounded_int(values[3], "existing prediction", 1)
        priority = signed_priority(values[4])
        if rid in seen or len(result) == MAX_REQUESTS:
            raise ValueError(f"scenario line {number}: duplicate ID or too many requests")
        seen.add(rid)
        result.append((rid, arrival, total, predicted, priority))
        preserved.append(None)
    if not result or (declared is not None and len(result) != declared):
        raise ValueError("empty scenario or mismatched declared count")
    return result, preserved


def scenario_with_predictions(rows, preserved, predictions):
    ids = {r[0] for r in rows}
    if ids != set(predictions):
        raise ValueError(f"scenario/features ID mismatch: missing={sorted(ids - set(predictions))}, extra={sorted(set(predictions) - ids)}")
    output, iterator = [], iter(rows)
    for line in preserved:
        if line is not None:
            output.append(line)
        else:
            rid, arrival, total, _old, priority = next(iterator)
            output.append(f"{rid},{arrival},{total},{predictions[rid]},{priority}")
    return "\n".join(output) + "\n"


def run_cli(cli, scenario, directory, workers, alpha, initial, timeout):
    runs = {}
    for label, policy, expected in POLICIES:
        export = directory / f"{label}.json"
        command = [str(cli), "--scenario", str(scenario), "--policy", policy,
                   "--workers", str(workers), "--export-json", str(export)]
        if policy == "ema":
            command += ["--ema-alpha", str(alpha), "--ema-initial", str(initial)]
        result = subprocess.run(command, capture_output=True, text=True,
                                check=False, timeout=timeout)
        if result.returncode:
            raise ValueError(f"CLI failed for {label} (exit {result.returncode}): {result.stderr.strip()[:500]}")
        data = json.loads(export.read_text(encoding="utf-8"))
        if (data.get("schema") != "os-llm-run" or data.get("finished") is not True or
                data.get("config", {}).get("policy") != expected or
                not isinstance(data.get("metrics"), dict)):
            raise ValueError(f"invalid or incomplete CLI export for {label}")
        runs[label] = data
    return runs


def compare(args):
    # Strict ordering is deliberate: no scenario output target is available
    # to the model or feature preparation at inference time.
    predictions = load_predictions(args.model, args.features)
    rows, preserved = load_scenario(args.scenario)
    injected = scenario_with_predictions(rows, preserved, predictions)
    out = Path(args.output_dir)
    if out.exists() and any(out.iterdir()):
        raise ValueError("output directory must be empty (refusing to overwrite results)")
    if not Path(args.cli).is_file():
        raise ValueError("simulator CLI executable not found")
    with tempfile.TemporaryDirectory(prefix="osllm-sjf-") as temporary:
        work = Path(temporary)
        scenario = work / "sjf-ai.csv"
        scenario.write_text(injected, encoding="utf-8")
        runs = run_cli(args.cli, scenario, work, args.workers,
                       args.ema_alpha, args.ema_initial, args.timeout)
        ids = {r[0] for r in rows}
        for label, _policy, _expected in POLICIES:
            data = runs[label]
            inputs = data.get("input", [])
            if (len(inputs) != len(rows) or {r.get("id") for r in inputs} != ids or
                    any((r.get("arrival_tick"), r.get("total_tokens"),
                          r.get("predicted_tokens"), r.get("priority")) !=
                        (arrival, total, predictions[rid], priority)
                        for rid, arrival, total, _old, priority in rows
                        for r in inputs if r.get("id") == rid) or
                    data["metrics"].get("completed") != len(rows)):
                raise ValueError(f"CLI export does not match common workload for {label}")
        errors = [predictions[rid] - total for rid, _arrival, total, _old, _priority in rows]
        report = {
            "schema": SCHEMA, "engine": "virtual", "source": "provided_scenario",
            "provenance": {"model_sha256": digest(args.model),
                           "features_sha256": digest(args.features),
                           "scenario_sha256": digest(args.scenario),
                           "model_schema": MODEL_SCHEMA},
            "config": {"workers": args.workers, "ema_alpha": args.ema_alpha,
                       "ema_initial": args.ema_initial},
            "prediction_error_on_scenario": {
                "mae_tokens": sum(abs(e) for e in errors) / len(errors),
                "rmse_tokens": math.sqrt(sum(e * e for e in errors) / len(errors))},
            "note": "Scenario-target evaluation only; no claim of held-out training independence or real LLM data.",
            "runs": {label: {"engine_policy": runs[label]["config"]["policy"],
                             "metrics": runs[label]["metrics"],
                             "trace_fingerprint": runs[label]["trace_fingerprint"]}
                     for label, _policy, _expected in POLICIES},
        }
        prediction_file = work / "predictions.json"
        prediction_file.write_text(json.dumps({"schema": PREDICTION_SCHEMA,
            "model_schema": MODEL_SCHEMA,
            "predictions": [{"request_id": rid, "predicted_tokens": predictions[rid]}
                            for rid, *_ in rows]}, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        (work / "comparison.json").write_text(json.dumps(report, indent=2, sort_keys=True,
            allow_nan=False) + "\n", encoding="utf-8")
        out.mkdir(parents=True, exist_ok=True)
        for filename in ("sjf-ai.csv", "predictions.json", "comparison.json",
                         "fcfs.json", "ema.json", "ai.json", "oracle.json"):
            shutil.copyfile(work / filename, out / filename)
    print(f"Compared {len(rows)} requests across FCFS/EMA/AI/oracle; outputs: {out}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("scenario", "features", "model", "cli", "output-dir"):
        parser.add_argument("--" + key, required=True)
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--ema-alpha", type=float, default=0.5)
    parser.add_argument("--ema-initial", type=int, default=6)
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args(argv)
    if (not 1 <= args.workers <= 4096 or not 0 <= args.ema_alpha <= 1 or
            not 1 <= args.ema_initial <= UINT32_MAX or not 1 <= args.timeout <= 600):
        parser.error("invalid workers, EMA parameters or timeout")
    try:
        compare(args)
    except (OSError, ValueError, TypeError, AttributeError, json.JSONDecodeError,
            subprocess.TimeoutExpired) as exc:
        parser.exit(2, f"error: {exc}\n")


if __name__ == "__main__":
    main()
