#!/usr/bin/env python3
"""Optional OpenAI-compatible token-length capture and seeded CSV v1 workload generation.

No raw prompt, completion, or credential is persisted. Standard library only.
"""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import math
import os
from pathlib import Path
import random
import re
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

CAPTURE_SCHEMA = "os-llm-workload-capture/v1"
META_SCHEMA = "os-llm-workload-scenario/v1"
MAX_REQUESTS = 256
MAX_PROMPT_CHARS = 4096
MAX_TOKENS = 4096
MAX_BODY = 1024 * 1024
UINT32_MAX = 2**32 - 1
FIXTURE = Path(__file__).with_name("synthetic_fixture.json")


class CaptureError(ValueError):
    """Invalid input or unsupported endpoint response (no private data in errors)."""


def integer(value, name, lo=0, hi=UINT32_MAX):
    if type(value) is not int or not lo <= value <= hi:
        raise CaptureError(f"invalid {name}")
    return value


def category(value):
    if not isinstance(value, str) or not 1 <= len(value) <= 80 or not value.strip() or any(ord(c) < 32 for c in value):
        raise CaptureError("invalid task_category")
    return value


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise CaptureError("duplicate JSON field")
        result[key] = value
    return result


def parse_json(data):
    try:
        return json.loads(data, object_pairs_hook=unique_object,
                          parse_constant=lambda _: (_ for _ in ()).throw(CaptureError("non-finite JSON number")))
    except (ValueError, UnicodeDecodeError) as exc:
        raise CaptureError("invalid JSON") from exc


def load_prompts(path, limit):
    file = Path(path)
    if file.stat().st_size > 4 * 1024 * 1024:
        raise CaptureError("prompt file exceeds size limit")
    rows, ids = [], set()
    with file.open(encoding="utf-8") as stream:
        for line_no, line in enumerate(stream, 1):
            if not line.strip():
                continue
            if len(rows) >= limit:
                break  # explicit upper bound; no accidental unlimited inference
            try:
                row = parse_json(line)
                if not isinstance(row, dict) or set(row) != {"request_id", "prompt", "task_category", "max_new_tokens", "priority"}:
                    raise CaptureError("invalid prompt fields")
                rid = integer(row["request_id"], "request_id", 1)
                if rid in ids:
                    raise CaptureError("duplicate request_id")
                prompt = row["prompt"]
                if not isinstance(prompt, str) or not 1 <= len(prompt) <= MAX_PROMPT_CHARS or not prompt.strip():
                    raise CaptureError("invalid prompt length")
                category(row["task_category"])
                integer(row["max_new_tokens"], "max_new_tokens", 1, MAX_TOKENS)
                integer(row["priority"], "priority", 0, 5)
            except CaptureError as exc:
                raise CaptureError(f"prompt line {line_no}: {exc}") from exc
            ids.add(rid)
            rows.append(row)
    if not rows:
        raise CaptureError("no prompts")
    return rows


def endpoint_kind(url, allow_remote):
    parsed = urlsplit(url)
    if (parsed.scheme not in ("http", "https") or not parsed.hostname or
            parsed.username is not None or parsed.password is not None or
            parsed.query or parsed.fragment or parsed.path != "/v1/chat/completions"):
        raise CaptureError("endpoint must be an HTTP(S) /v1/chat/completions URL without credentials/query")
    loopback = parsed.hostname.lower() in ("localhost", "127.0.0.1", "::1")
    if not loopback and not allow_remote:
        raise CaptureError("remote endpoint requires --allow-remote")
    if not loopback and parsed.scheme != "https":
        raise CaptureError("remote endpoint requires HTTPS")
    try:
        port = parsed.port
    except ValueError as exc:
        raise CaptureError("invalid endpoint port") from exc
    if port is not None and not 1 <= port <= 65535:
        raise CaptureError("invalid endpoint port")
    return "loopback" if loopback else "remote"


def completion(url, model, prompt, cap, timeout, temperature, seed, api_key):
    payload = {"model": model, "messages": [{"role": "user", "content": prompt}],
               "max_tokens": cap, "temperature": temperature, "stream": False}
    if seed is not None:
        payload["seed"] = seed
    headers = {"Content-Type": "application/json"}
    if api_key:
        headers["Authorization"] = "Bearer " + api_key
    request = Request(url, data=json.dumps(payload).encode("utf-8"), headers=headers, method="POST")
    start = time.perf_counter_ns()
    try:
        with urlopen(request, timeout=timeout) as response:
            raw = response.read(MAX_BODY + 1)
    except HTTPError as exc:
        raise CaptureError(f"endpoint returned HTTP {exc.code}") from exc
    except (URLError, TimeoutError, OSError) as exc:
        raise CaptureError("endpoint connection failed or timed out") from exc
    latency_ms = round((time.perf_counter_ns() - start) / 1_000_000, 3)
    if len(raw) > MAX_BODY:
        raise CaptureError("endpoint response exceeds size limit")
    data = parse_json(raw)
    if not isinstance(data, dict) or not isinstance(data.get("usage"), dict):
        raise CaptureError("endpoint must return token usage")
    usage = data["usage"]
    prompt_tokens = integer(usage.get("prompt_tokens"), "usage.prompt_tokens", 0)
    output_tokens = integer(usage.get("completion_tokens"), "usage.completion_tokens", 1, cap)
    choices = data.get("choices")
    if not isinstance(choices, list) or len(choices) != 1 or not isinstance(choices[0], dict):
        raise CaptureError("invalid completion choices")
    message = choices[0].get("message")
    if not isinstance(message, dict) or not isinstance(message.get("content"), str):
        raise CaptureError("invalid completion message")
    finish = choices[0].get("finish_reason")
    if not isinstance(finish, str) or not 1 <= len(finish) <= 40:
        raise CaptureError("invalid finish_reason")
    reported = data.get("model", model)
    if not isinstance(reported, str) or not 1 <= len(reported) <= 120 or any(ord(c) < 32 for c in reported):
        raise CaptureError("invalid response model")
    return {"prompt_tokens": prompt_tokens, "output_tokens": output_tokens,
            "output_chars": len(message["content"]), "latency_ms": latency_ms,
            "finish_reason": finish, "reported_model": reported}


def write_json_atomic(path, value):
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        raise CaptureError("output already exists")
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=target.parent,
                                     prefix=".osllm-", delete=False) as temp:
        name = temp.name
        try:
            json.dump(value, temp, indent=2, sort_keys=True, allow_nan=False)
            temp.write("\n")
        except BaseException:
            temp.close()
            os.unlink(name)
            raise
    try:
        if target.exists():
            raise CaptureError("output already exists")
        os.link(name, target)  # atomic create-if-absent; never overwrite a capture
    finally:
        if os.path.exists(name):
            os.unlink(name)


def collect(args):
    kind = endpoint_kind(args.endpoint, args.allow_remote)
    if not isinstance(args.model, str) or not 1 <= len(args.model) <= 120 or any(ord(c) < 32 for c in args.model):
        raise CaptureError("invalid model")
    integer(args.max_prompts, "max_prompts", 1, MAX_REQUESTS)
    if not 0.1 <= args.timeout <= 120 or not math.isfinite(args.timeout):
        raise CaptureError("timeout must be between 0.1 and 120 seconds")
    if not 0 <= args.temperature <= 2 or not math.isfinite(args.temperature):
        raise CaptureError("temperature out of range")
    if args.seed is not None:
        integer(args.seed, "request_seed", 0, UINT32_MAX)
    key = None
    if args.api_key_env:
        if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", args.api_key_env):
            raise CaptureError("invalid API key environment variable name")
        key = os.environ.get(args.api_key_env)
        if not key:
            raise CaptureError("API key environment variable not set")
    prompts = load_prompts(args.prompts, args.max_prompts)
    meta = [{"request_id": r["request_id"], "prompt_chars": len(r["prompt"]),
             "task_category": r["task_category"], "priority": r["priority"],
             "max_new_tokens": r["max_new_tokens"]} for r in prompts]
    digest = hashlib.sha256(json.dumps(meta, sort_keys=True).encode("utf-8")).hexdigest()
    records = []
    for r in prompts:
        observed = completion(args.endpoint, args.model, r["prompt"], r["max_new_tokens"],
                              args.timeout, args.temperature, args.seed, key)
        records.append({"request_id": r["request_id"], "task_category": r["task_category"],
                        "priority": r["priority"], "max_new_tokens": r["max_new_tokens"],
                        "prompt_chars": len(r["prompt"]), **observed})
    document = {"schema": CAPTURE_SCHEMA, "source": "observed_llm",
                "provenance": {"provider": "openai-compatible", "endpoint_kind": kind,
                               "requested_model": args.model, "captured_at_utc": datetime.now(timezone.utc).isoformat(),
                               "temperature": args.temperature, "request_seed": args.seed,
                               "timeout_seconds": args.timeout, "prompt_metadata_sha256": digest,
                               "note": "No raw prompt/completion text stored; latency is wall time."},
                "records": records}
    write_json_atomic(args.output, document)
    print(f"Captured {len(records)} request metadata rows to {args.output}")


def load_capture(path):
    if Path(path).stat().st_size > 4 * 1024 * 1024:
        raise CaptureError("capture file exceeds size limit")
    data = parse_json(Path(path).read_text(encoding="utf-8"))
    if not isinstance(data, dict) or data.get("schema") != CAPTURE_SCHEMA or data.get("source") not in ("observed_llm", "synthetic_fixture"):
        raise CaptureError("unsupported capture schema/source")
    provenance = data.get("provenance")
    if not isinstance(provenance, dict) or provenance.get("provider") != ("openai-compatible" if data["source"] == "observed_llm" else "none"):
        raise CaptureError("invalid capture provenance")
    rows = data.get("records")
    if not isinstance(rows, list) or not 1 <= len(rows) <= MAX_REQUESTS:
        raise CaptureError("invalid capture records")
    ids = set()
    for r in rows:
        if not isinstance(r, dict) or set(r) != {"request_id", "task_category", "priority", "max_new_tokens", "prompt_chars", "prompt_tokens", "output_tokens", "output_chars", "latency_ms", "finish_reason", "reported_model"}:
            raise CaptureError("invalid capture record fields")
        rid = integer(r["request_id"], "request_id", 1)
        if rid in ids:
            raise CaptureError("duplicate capture request_id")
        ids.add(rid)
        category(r["task_category"])
        integer(r["priority"], "priority", 0, 5)
        integer(r["max_new_tokens"], "max_new_tokens", 1, MAX_TOKENS)
        integer(r["prompt_chars"], "prompt_chars", 1, MAX_PROMPT_CHARS)
        integer(r["prompt_tokens"], "prompt_tokens")
        integer(r["output_tokens"], "output_tokens", 1, r["max_new_tokens"])
        integer(r["output_chars"], "output_chars")
        if type(r["latency_ms"]) not in (int, float) or not math.isfinite(r["latency_ms"]) or r["latency_ms"] < 0:
            raise CaptureError("invalid latency_ms")
        if not isinstance(r["finish_reason"], str) or not 1 <= len(r["finish_reason"]) <= 40:
            raise CaptureError("invalid finish_reason")
        if not isinstance(r["reported_model"], str) or not 1 <= len(r["reported_model"]) <= 120 or any(ord(c) < 32 for c in r["reported_model"]):
            raise CaptureError("invalid reported_model")
    return data


def arrivals(count, span, distribution, rng):
    if distribution == "staggered":
        return [(i * span // (count - 1)) if count > 1 else 0 for i in range(count)]
    if distribution == "uniform":
        return [rng.randrange(span + 1) for _ in range(count)]
    jitter = span // 8
    return [(span - rng.randrange(jitter + 1) if rng.randrange(2) else rng.randrange(jitter + 1)) for _ in range(count)]


def generate(args):
    integer(args.seed, "seed", 0, 2**64 - 1)
    integer(args.count, "count", 1, MAX_REQUESTS)
    integer(args.arrival_span, "arrival_span", 0, UINT32_MAX)
    capture_path = Path(args.capture) if args.capture else FIXTURE
    data = load_capture(capture_path)
    source_rows = data["records"]
    if args.profile in ("short", "long", "memory-pressure"):
        ordered = sorted(source_rows, key=lambda r: (r["output_tokens"], r["request_id"]))
        half = max(1, (len(ordered) + 1) // 2)
        source_rows = ordered[:half] if args.profile == "short" else ordered[-half:]
    span = min(args.arrival_span, 2) if args.profile in ("high-concurrency", "memory-pressure") else args.arrival_span
    rng = random.Random(args.seed)
    times = arrivals(args.count, span, args.distribution, rng)
    selected = [rng.choice(source_rows) for _ in range(args.count)]
    scenario = [f"# os-llm-scenario v1 seed={args.seed} distribution={args.distribution} count={args.count}"]
    features = io.StringIO(newline="")
    writer = csv.writer(features, lineterminator="\n")
    writer.writerow(["request_id", "prompt_tokens", "max_new_tokens", "task_category"])
    mapping = []
    for i, (sample, arrival) in enumerate(zip(selected, times), 1):
        priority = rng.randrange(6) if args.profile == "priority" else sample["priority"]
        # max_new_tokens is a pre-generation cap, never the observed target.
        scenario.append(f"{i},{arrival},{sample['output_tokens']},{sample['max_new_tokens']},{priority}")
        writer.writerow([i, sample["prompt_tokens"], sample["max_new_tokens"], sample["task_category"]])
        mapping.append({"request_id": i, "source_request_id": sample["request_id"],
                        "prompt_tokens": sample["prompt_tokens"], "output_tokens": sample["output_tokens"],
                        "max_new_tokens": sample["max_new_tokens"], "task_category": sample["task_category"],
                        "latency_ms_observed": sample["latency_ms"]})
    meta = {"schema": META_SCHEMA, "source": data["source"],
            "capture_sha256": hashlib.sha256(capture_path.read_bytes()).hexdigest(),
            "config": {"seed": args.seed, "count": args.count, "distribution": args.distribution,
                       "profile": args.profile, "arrival_span_effective": span,
                       "sampling": "with_replacement"},
            "output_tokens_origin": "observed_completion_tokens" if data["source"] == "observed_llm" else "synthetic_fixture_values",
            "predicted_tokens_origin": "max_new_tokens_cap_not_oracle",
            "note": "Virtual simulation uses observed/synthetic token counts; recorded latency is not a virtual tick.",
            "records": mapping}
    output = Path(args.output_dir)
    if output.exists():
        raise CaptureError("output directory already exists; refusing overwrite")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".osllm-workload-", dir=output.parent) as temporary:
        tmp = Path(temporary)
        (tmp / "scenario.csv").write_text("\n".join(scenario) + "\n", encoding="utf-8")
        (tmp / "features.csv").write_text(features.getvalue(), encoding="utf-8")
        (tmp / "scenario.meta.json").write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if output.exists():
            raise CaptureError("output directory already exists")
        # Atomic directory publication: no partial scenario if generation fails.
        os.rename(tmp, output)
    print(f"Generated {args.count} {data['source']} requests in {output}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    c = sub.add_parser("collect", help="opt-in OpenAI-compatible LLM metadata capture")
    c.add_argument("--prompts", required=True, help="JSONL with prompt and request metadata")
    c.add_argument("--endpoint", required=True, help="full /v1/chat/completions URL")
    c.add_argument("--model", required=True)
    c.add_argument("--output", required=True)
    c.add_argument("--max-prompts", type=int, default=16)
    c.add_argument("--timeout", type=float, default=15.0)
    c.add_argument("--temperature", type=float, default=0.0)
    c.add_argument("--seed", type=int)
    c.add_argument("--api-key-env", help="environment variable name, never a literal key")
    c.add_argument("--allow-remote", action="store_true", help="explicitly opt in to HTTPS remote endpoint")
    g = sub.add_parser("generate", help="seeded CSV v1 + pre-generation features + metadata")
    g.add_argument("--capture", help="capture JSON; omitted means explicitly synthetic fixture")
    g.add_argument("--output-dir", required=True)
    g.add_argument("--seed", type=int, default=42)
    g.add_argument("--count", type=int, default=16)
    g.add_argument("--arrival-span", type=int, default=20)
    g.add_argument("--distribution", choices=("uniform", "burst", "staggered"), default="uniform")
    g.add_argument("--profile", choices=("mixed", "short", "long", "memory-pressure", "high-concurrency", "priority"), default="mixed")
    args = parser.parse_args(argv)
    try:
        if args.command == "collect":
            collect(args)
        else:
            generate(args)
    except (CaptureError, OSError) as exc:
        # Never echo endpoint response body, prompts, credentials, or raw JSON.
        print(f"workload_capture: {exc if isinstance(exc, CaptureError) else 'file or endpoint I/O error'}", file=__import__("sys").stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
