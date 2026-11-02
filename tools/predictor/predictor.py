#!/usr/bin/env python3
"""Offline, dependency-free output-token regression; no simulator integration."""
import argparse
import csv
import json
import math
import random
from pathlib import Path

MODEL_SCHEMA = "os-llm-output-predictor/v1"
PREDICTION_SCHEMA = "os-llm-token-predictions/v1"
NUMERIC = ("prompt_tokens", "max_new_tokens")
REQUIRED = ("request_id", *NUMERIC, "task_category")


def positive_int(value, name, allow_zero=False):
    try:
        n = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} must be an integer") from exc
    if str(n) != str(value).strip() or n < (0 if allow_zero else 1) or n > 2**31 - 1:
        raise ValueError(f"{name} must be a bounded {'nonnegative' if allow_zero else 'positive'} integer")
    return n


def read_rows(path, training):
    with open(path, newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        fields = reader.fieldnames or []
        needed = set(REQUIRED) | ({"output_tokens"} if training else set())
        if len(fields) != len(set(fields)) or not needed.issubset(fields):
            raise ValueError(f"CSV requires unique columns: {', '.join(sorted(needed))}")
        rows, seen = [], set()
        for line, raw in enumerate(reader, start=2):
            if None in raw or any(value is None for value in raw.values()):
                raise ValueError(f"line {line}: malformed CSV")
            rid = positive_int(raw["request_id"], "request_id")
            if rid in seen:
                raise ValueError(f"line {line}: duplicate request_id {rid}")
            seen.add(rid)
            category = raw["task_category"].strip()
            if not category or len(category) > 80:
                raise ValueError(f"line {line}: invalid task_category")
            row = {"request_id": rid, "task_category": category}
            for name in NUMERIC:
                row[name] = positive_int(raw[name], name, allow_zero=(name == "prompt_tokens"))
            if training:
                row["output_tokens"] = positive_int(raw["output_tokens"], "output_tokens")
                if row["output_tokens"] > row["max_new_tokens"]:
                    raise ValueError(f"line {line}: output_tokens exceeds max_new_tokens")
            rows.append(row)
    if not rows:
        raise ValueError("CSV has no requests")
    return rows


def vector(row, means, scales, categories):
    return [1.0] + [(row[k] - means[k]) / scales[k] for k in NUMERIC] + [float(row["task_category"] == c) for c in categories]


def solve(matrix, rhs):
    """Partial-pivot Gauss-Jordan elimination; ridge keeps design invertible."""
    n = len(rhs)
    a = [matrix[i][:] + [rhs[i]] for i in range(n)]
    for k in range(n):
        pivot = max(range(k, n), key=lambda i: abs(a[i][k]))
        if abs(a[pivot][k]) < 1e-10:
            raise ValueError("singular training system")
        a[k], a[pivot] = a[pivot], a[k]
        divisor = a[k][k]
        a[k] = [v / divisor for v in a[k]]
        for i in range(n):
            if i != k:
                factor = a[i][k]
                a[i] = [x - factor * y for x, y in zip(a[i], a[k])]
    return [a[i][-1] for i in range(n)]


def fit(train, ridge):
    categories = sorted({r["task_category"] for r in train})
    if len(categories) > 64:
        raise ValueError("too many categories (maximum 64)")
    means = {k: sum(r[k] for r in train) / len(train) for k in NUMERIC}
    scales = {k: max(math.sqrt(sum((r[k] - means[k]) ** 2 for r in train) / len(train)), 1.0) for k in NUMERIC}
    size = 1 + len(NUMERIC) + len(categories)
    gram = [[0.0] * size for _ in range(size)]
    rhs = [0.0] * size
    for row in train:
        x, y = vector(row, means, scales, categories), row["output_tokens"]
        for i in range(size):
            rhs[i] += x[i] * y
            for j in range(size):
                gram[i][j] += x[i] * x[j]
    for i in range(1, size):
        gram[i][i] += ridge
    return {"schema": MODEL_SCHEMA, "algorithm": "ridge-linear", "ridge": ridge,
            "numeric_features": list(NUMERIC), "categorical_feature": "task_category",
            "means": means, "scales": scales, "categories": categories,
            "weights": solve(gram, rhs)}


def check_model(model):
    if not isinstance(model, dict) or model.get("schema") != MODEL_SCHEMA or model.get("algorithm") != "ridge-linear":
        raise ValueError("unsupported model schema/algorithm")
    if model.get("numeric_features") != list(NUMERIC) or model.get("categorical_feature") != "task_category":
        raise ValueError("unsupported feature contract")
    cats = model.get("categories")
    if not isinstance(cats, list) or not cats or len(cats) > 64 or any(not isinstance(c, str) or not c for c in cats) or len(set(cats)) != len(cats):
        raise ValueError("invalid model categories")
    weights = model.get("weights")
    if not isinstance(weights, list) or len(weights) != 3 + len(cats) or any(type(w) not in (int, float) or not math.isfinite(w) for w in weights):
        raise ValueError("invalid model weights")
    for key in NUMERIC:
        for name in ("means", "scales"):
            val = model.get(name, {}).get(key)
            if type(val) not in (int, float) or not math.isfinite(val) or (name == "scales" and val <= 0):
                raise ValueError("invalid model normalization")


def estimate(row, model):
    raw = sum(w * x for w, x in zip(model["weights"], vector(row, model["means"], model["scales"], model["categories"])))
    if not math.isfinite(raw):
        raise ValueError("non-finite model prediction")
    return min(row["max_new_tokens"], max(1, math.floor(raw + 0.5)))


def metrics(rows, predictions):
    errors = [abs(r["output_tokens"] - p) for r, p in zip(rows, predictions)]
    return {"mae": sum(errors) / len(errors), "rmse": math.sqrt(sum(e * e for e in errors) / len(errors))}


def save_json(path, payload):
    Path(path).write_text(json.dumps(payload, ensure_ascii=False, sort_keys=True, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def train(args):
    rows = read_rows(args.data, training=True)
    if len(rows) < 4:
        raise ValueError("training requires at least four distinct requests")
    if not (0 < args.test_fraction < 1) or not math.isfinite(args.ridge) or args.ridge <= 0:
        raise ValueError("test_fraction must be in (0,1) and ridge must be finite and positive")
    rng = random.Random(args.seed)
    rng.shuffle(rows)
    n_test = max(1, min(len(rows) - 2, round(len(rows) * args.test_fraction)))
    test, train_rows = rows[:n_test], rows[n_test:]
    model = fit(train_rows, args.ridge)
    check_model(model)
    model["training"] = {"seed": args.seed, "train_count": len(train_rows), "test_count": len(test), "test_fraction": args.test_fraction}
    predictions = [estimate(row, model) for row in test]
    baseline_mean = sum(row["output_tokens"] for row in train_rows) / len(train_rows)
    baseline = [min(row["max_new_tokens"], max(1, math.floor(baseline_mean + 0.5))) for row in test]
    report = {"schema": "os-llm-predictor-evaluation/v1", "synthetic_or_real": "provided_dataset_unverified",
              "seed": args.seed, "train_ids": sorted(r["request_id"] for r in train_rows),
              "test_ids": sorted(r["request_id"] for r in test),
              "test_count": len(test), "ridge": args.ridge,
              "held_out": metrics(test, predictions), "train_mean_baseline": metrics(test, baseline)}
    save_json(args.model, model)
    save_json(args.report, report)
    print(f"train={len(train_rows)} test={len(test)} MAE={report['held_out']['mae']:.3f} RMSE={report['held_out']['rmse']:.3f}")


def predict(args):
    model = json.loads(Path(args.model).read_text(encoding="utf-8"))
    check_model(model)
    rows = read_rows(args.data, training=False)
    result = {"schema": PREDICTION_SCHEMA, "model_schema": MODEL_SCHEMA,
              "predictions": [{"request_id": row["request_id"], "predicted_tokens": estimate(row, model)} for row in rows]}
    save_json(args.output, result)
    print(f"predictions={len(rows)}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    tr = commands.add_parser("train", help="fit ridge model and evaluate on held-out rows")
    tr.add_argument("--data", required=True)
    tr.add_argument("--model", required=True)
    tr.add_argument("--report", required=True)
    tr.add_argument("--seed", type=int, default=42)
    tr.add_argument("--test-fraction", type=float, default=0.25)
    tr.add_argument("--ridge", type=float, default=1.0)
    pr = commands.add_parser("predict", help="predict from pre-generation metadata")
    pr.add_argument("--data", required=True)
    pr.add_argument("--model", required=True)
    pr.add_argument("--output", required=True)
    args = parser.parse_args(argv)
    try:
        (train if args.command == "train" else predict)(args)
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        parser.exit(2, f"error: {exc}\n")


if __name__ == "__main__":
    main()
