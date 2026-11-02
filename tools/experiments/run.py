#!/usr/bin/env python3
"""Execute bounded OS-LLM experimental campaigns using existing binaries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile

SCHEMA = "os-llm-experiments/v1"


def execute(command, timeout):
    result = subprocess.run(command, capture_output=True, text=True, check=False, timeout=timeout)
    if result.returncode != 0:
        raise ValueError(f"command failed: {Path(command[0]).name} exit={result.returncode}: {result.stderr[:250]}")
    return result.stdout.strip()


def workload(scenario):
    items = []
    for line in Path(scenario).read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        fields = [field.strip() for field in line.split(",")]
        if len(fields) != 5:
            raise ValueError("scenario row requires 5 fields")
        request_id, arrival, total, predicted, priority = map(int, fields)
        if not 1 <= request_id <= 2**32 - 1 or not 0 <= arrival <= 2**64 - 1 or not 1 <= total <= 2**32 - 1 or not 1 <= predicted <= 2**32 - 1:
            raise ValueError("scenario field out of range")
        items.append((request_id, arrival, total, priority))
    if not 1 <= len(items) <= 256 or len({item[0] for item in items}) != len(items):
        raise ValueError("scenario must have 1..256 unique requests")
    return sorted(items)


def run_cli(args, work, expected, name, flags):
    artifact = work / "runs" / (name + ".json")
    command = [str(args.cli.resolve()), "--scenario", str(args.scenario.resolve()),
               "--export-json", str(artifact), "--workers", str(args.workers)] + flags
    execute(command, args.timeout)
    data = json.loads(artifact.read_text(encoding="utf-8"))
    inputs = data.get("input")
    if (data.get("schema") != "os-llm-run" or data.get("finished") is not True or
        not isinstance(inputs, list) or
        sorted((r["id"], r["arrival_tick"], r["total_tokens"], r["priority"]) for r in inputs) != expected):
        raise ValueError(name + ": incomplete or non-comparable CLI export")
    metrics = data.get("metrics")
    if not isinstance(metrics, dict) or metrics.get("completed") != len(expected):
        raise ValueError(name + ": incomplete measurements")
    return {"name": name, "status": "completed", "config": data["config"],
            "metrics": metrics, "trace_fingerprint": data.get("trace_fingerprint"),
            "artifact": "runs/" + name + ".json"}


def lab(executable, mode, pattern, timeout):
    if executable is None:
        return {"name": mode, "status": "skipped", "reason": "laboratory binary not provided"}
    output = execute([str(executable.resolve()), mode], timeout)
    if re.search(pattern, output) is None:
        raise ValueError(mode + ": unexpected lab observation: " + output[:180])
    return {"name": mode, "status": "completed", "observed_output": output}


def collect(args, work):
    expected = workload(args.scenario)
    (work / "runs").mkdir()
    data = {"schema": SCHEMA, "provenance": {
        "scenario_sha256": hashlib.sha256(args.scenario.read_bytes()).hexdigest(),
        "source_claim": args.source, "source_verified": False,
        "environment": {"os": platform.system(), "machine": platform.machine(),
                        "python": platform.python_version()},
        "model": "virtual token-tick scheduler; pthread labs separately"},
        "config": {"workers": args.workers, "frames": args.frames,
                   "tokens_per_page": args.page_tokens, "timeout_seconds": args.timeout},
        "campaigns": {}}
    data["campaigns"]["E1"] = {"name": "logical lost updates", "runs": [
        lab(args.race_lab, "safe", r"mode=safe expected=2 observed=2 lost=0", args.timeout),
        lab(args.race_lab, "unsafe", r"mode=unsafe expected=2 observed=1 lost=1", args.timeout)]}
    scheduling = [
        ("fcfs", ["--policy", "fcfs"]), ("sjf_estimated", ["--policy", "sjf"]),
        ("sjf_ema", ["--policy", "ema", "--ema-alpha", "0.5", "--ema-initial", "6"]),
        ("sjf_oracle", ["--policy", "oracle"]),
        ("rr_q1", ["--policy", "rr", "--quantum", "1"]),
        ("rr_q3", ["--policy", "rr", "--quantum", "3"]),
        ("priority_aging", ["--policy", "priority"])]
    e2 = [run_cli(args, work, expected, name, flags) for name, flags in scheduling]
    e2 += [{"name": "sjf_ai", "status": "skipped",
            "reason": "requires independently trained model; see tools/predictor/compare_sjf.py"},
           {"name": "priority_no_aging", "status": "skipped",
            "reason": "CLI does not yet expose aging_interval"}]
    data["campaigns"]["E2"] = {"name": "virtual scheduling", "runs": e2}
    e3 = []
    for count in args.frames:
        for policy in ("fifo", "lru"):
            name = f"memory_{policy}_f{count}"
            e3.append(run_cli(args, work, expected, name,
                              ["--policy", "fcfs", "--frames", str(count),
                               "--page-tokens", str(args.page_tokens),
                               "--page-policy", policy, "--page-fault-latency", "1"]))
    e3.append({"name": "opt_offline", "status": "skipped",
               "reason": "OPT is an offline replay benchmark, not an online CLI policy"})
    data["campaigns"]["E3"] = {"name": "virtual paging", "runs": e3}
    data["campaigns"]["E4"] = {"name": "isolated pthread deadlock", "runs": [
        lab(args.deadlock_lab, "ordered",
            r"mode=ordered completed=2 watchdog=none deadlock=no", args.timeout),
        lab(args.deadlock_lab, "deadlock",
            r"mode=deadlock acquired=2 watchdog=SIGKILL deadlock=observed", args.timeout)]}
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, type=Path)
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--race-lab", type=Path)
    parser.add_argument("--deadlock-lab", type=Path)
    parser.add_argument("--source", choices=("synthetic_fixture", "observed_llm", "unverified"), default="unverified")
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--frames", type=int, nargs="+", default=[1, 2, 4])
    parser.add_argument("--page-tokens", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args(argv)
    if (not 1 <= args.workers <= 32 or not 1 <= args.page_tokens <= 4096 or
        not 1 <= args.timeout <= 120 or not args.frames or
        len(args.frames) != len(set(args.frames)) or any(not 1 <= x <= 4096 for x in args.frames)):
        parser.error("invalid experiment configuration")
    output = args.output_dir.resolve()
    if output.exists():
        parser.error("output directory already exists; will not overwrite")
    if not args.cli.is_file():
        parser.error("CLI binary not found")
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(prefix=".osllm-experiments-", dir=output.parent) as scratch:
            tmp = Path(scratch)
            data = collect(args, tmp)
            (tmp / "experiments.json").write_text(json.dumps(data, sort_keys=True, indent=2,
                                         allow_nan=False) + "\n", encoding="utf-8")
            if output.exists():
                raise ValueError("output directory appeared during execution")
            os.rename(tmp, output)
    except (ValueError, OSError, KeyError, TypeError, json.JSONDecodeError,
            subprocess.TimeoutExpired) as exc:
        parser.exit(2, f"error: {exc}\n")
    print(f"Experiments written to {output}")


if __name__ == "__main__":
    main()
