#!/usr/bin/env python3
"""H24: Validate raw H23 measurements and render reproducible reports."""
import argparse
import csv
import hashlib
import html
import json
import math
import os
from pathlib import Path
import re
import tempfile

SCHEMA = "os-llm-experiments/v1"
VERSION = "os-llm-report/v1"
SCHED = ("completed", "average_wait_ticks", "p50_wait_ticks", "p95_wait_ticks",
         "average_response_ticks", "p95_response_ticks",
         "average_turnaround_ticks", "p95_turnaround_ticks",
         "throughput_per_tick", "worker_utilization", "max_wait_ticks", "starvation_count")
MEMORY = ("completed", "page_hits", "page_faults", "page_evictions",
          "peak_resident_frames", "total_blocked_ticks")
NAME = re.compile(r"[a-z][a-z0-9_]*\Z")


def safe_json(path):
    def no_constant(value):
        raise ValueError("non-finite JSON: " + value)
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON property")
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique,
                      parse_constant=no_constant)


def workload(data):
    rows, ids = [], set()
    if not isinstance(data, list) or not data:
        raise ValueError("empty or malformed workload")
    for row in data:
        if not isinstance(row, dict):
            raise ValueError("invalid input record")
        record = tuple(row.get(field) for field in
                       ("id", "arrival_tick", "total_tokens", "priority"))
        if any(type(v) is not int for v in record) or record[0] in ids or (
            record[0] <= 0 or record[1] < 0 or record[2] <= 0):
            raise ValueError("invalid workload record")
        ids.add(record[0])
        rows.append(record)
    return tuple(sorted(rows))


def validate(root):
    source = safe_json(root / "experiments.json")
    if not isinstance(source, dict) or source.get("schema") != SCHEMA:
        raise ValueError("unsupported experiment schema")
    provenance = source.get("provenance")
    if (not isinstance(provenance, dict) or
        provenance.get("source_claim") not in
        ("synthetic_fixture", "observed_llm", "unverified") or
        provenance.get("source_verified") is not False):
        raise ValueError("source provenance missing or not verifiable")
    if not isinstance(source.get("config"), dict):
        raise ValueError("configuration missing")
    campaigns = source.get("campaigns")
    if not isinstance(campaigns, dict) or set(campaigns) != {"E1", "E2", "E3", "E4"}:
        raise ValueError("missing experiment campaigns")
    rows = {"E2": [], "E3": []}
    labs = {"E1": [], "E4": []}
    reference = None
    for group in ("E1", "E2", "E3", "E4"):
        runs = campaigns[group].get("runs")
        if not isinstance(runs, list):
            raise ValueError("invalid run collection")
        names = set()
        for run in runs:
            if not isinstance(run, dict):
                raise ValueError("invalid run")
            label, status = run.get("name"), run.get("status")
            if not isinstance(label, str) or not NAME.fullmatch(label) or label in names:
                raise ValueError("duplicate/unsafe run label")
            names.add(label)
            if status == "skipped":
                reason = run.get("reason")
                if not isinstance(reason, str) or not reason.strip():
                    raise ValueError("missing skip reason")
                if group in labs:
                    labs[group].append({"name": label, "status": status, "details": reason})
                continue
            if status != "completed":
                raise ValueError("unknown run status")
            if group in labs:
                observation = run.get("observed_output")
                if not isinstance(observation, str) or not observation.strip():
                    raise ValueError("missing laboratory observation")
                labs[group].append({"name": label, "status": status, "details": observation})
                continue
            expected_path = "runs/" + label + ".json"
            if run.get("artifact") != expected_path:
                raise ValueError("invalid artifact path")
            raw = safe_json(root / expected_path)
            if not isinstance(raw, dict) or raw.get("schema") != "os-llm-run" or raw.get("finished") is not True:
                raise ValueError("invalid or incomplete raw run")
            if (raw.get("metrics") != run.get("metrics") or
                raw.get("config") != run.get("config") or
                raw.get("trace_fingerprint") != run.get("trace_fingerprint")):
                raise ValueError("manifest disagrees with raw measurements")
            request_set = workload(raw.get("input"))
            if reference is None:
                reference = request_set
            elif reference != request_set:
                raise ValueError("comparisons use different workloads")
            metrics = raw["metrics"]
            if not isinstance(metrics, dict) or metrics.get("completed") != len(request_set):
                raise ValueError("uncompleted or missing measurements")
            columns = SCHED if group == "E2" else MEMORY
            item = {"name": label, "policy": raw["config"].get("policy")}
            for key in columns:
                v = metrics.get(key)
                if type(v) not in (int, float) or not math.isfinite(v) or v < 0:
                    raise ValueError("invalid/missing metric " + key)
                item[key] = v
            rows[group].append(item)
    return source, rows, labs


def format_number(value):
    return ("%.4f" % value).rstrip("0").rstrip(".") if type(value) is float else str(value)


def markdown_table(rows, keys):
    if not rows:
        return "_Nenhuma medição verificada._\n"
    output = ["| " + " | ".join(keys) + " |", "| " + " | ".join("---" for _ in keys) + " |"]
    for row in rows:
        output.append("| " + " | ".join(format_number(row[key]) for key in keys) + " |")
    return "\n".join(output) + "\n"


def draw_bars(rows, metric, title):
    if not rows:
        return None
    max_value = max((r[metric] for r in rows), default=0) or 1
    height = 110 + 38 * len(rows)
    result = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 900 {height}" width="900" height="{height}">',
              '<rect width="100%" height="100%" fill="white"/>',
              f'<text x="18" y="30" font-size="19" font-family="sans-serif">{html.escape(title)}</text>',
              '<text x="18" y="50" font-size="12" font-family="sans-serif">Simulação virtual; não são tempos reais de inferência</text>']
    for index, row in enumerate(rows):
        y, value = 80 + index * 38, row[metric]
        width = int(420 * value / max_value)
        result += [f'<text x="18" y="{y + 15}" font-size="13" font-family="monospace">{html.escape(row["name"])}</text>',
                   f'<rect x="248" y="{y}" rx="3" width="{width}" height="21" fill="#4472a5"/>',
                   f'<text x="{260 + width}" y="{y + 15}" font-size="13" font-family="monospace">{html.escape(format_number(value))}</text>']
    return "\n".join(result + ["</svg>", ""])


def export(output, original, rows, labs, digest):
    for key, name, metrics in (("E2", "scheduling", SCHED), ("E3", "memory", MEMORY)):
        with (output / (name + ".csv")).open("w", newline="", encoding="utf-8") as file:
            writer = csv.DictWriter(file, fieldnames=("name", "policy") + metrics)
            writer.writeheader()
            writer.writerows(rows[key])
    charts = []
    for key, filename, metric, title in (
        ("E2", "scheduling_wait.svg", "average_wait_ticks", "Espera média por política (ticks)"),
        ("E3", "memory_faults.svg", "page_faults", "Faltas de página por configuração")
    ):
        svg = draw_bars(rows[key], metric, title)
        if svg is not None:
            (output / filename).write_text(svg, encoding="utf-8")
            charts.append(filename)
    report = {"schema": VERSION, "experiments_sha256": digest,
              "provenance": original["provenance"], "configuration": original["config"],
              "scheduling": rows["E2"], "memory": rows["E3"], "laboratories": labs,
              "charts": charts, "time_basis": "virtual ticks, not wall-clock LLM inference"}
    (output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True,
                                    allow_nan=False) + "\n", encoding="utf-8")
    notes = ["# Relatório de experimentos OS-LLM", "",
             "## Procedência e método",
             "Origem declarada: " + original["provenance"]["source_claim"] +
             " (não verificada independentemente).",
             "Manifest de entrada SHA-256: " + digest,
             "As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.",
             "Métricas de espera, resposta, retorno e throughput usam ticks virtuais.",
             "Não representam a latência de um LLM real. SJF-oráculo conhece o alvo antecipadamente.",
             "", "## E2 — Escalonamento",
             markdown_table(rows["E2"], ("name", "average_wait_ticks",
                 "p95_wait_ticks", "average_response_ticks",
                 "average_turnaround_ticks", "throughput_per_tick")),
             "## E3 — Memória",
             markdown_table(rows["E3"], ("name", "page_hits",
                 "page_faults", "page_evictions", "peak_resident_frames")),
             "## E1 e E4 — Laboratórios"]
    for group in ("E1", "E4"):
        for item in labs[group]:
            notes.append(f'- {group} {item["name"]}: {item["status"]}: {item["details"]}')
    notes.extend(["", "## Limitações",
                  "- Um cenário não demonstra superioridade universal de uma política.",
                  "- OPT é somente offline, não foi comparado como política online.",
                  "- Testes de corrida e deadlock são laboratórios, não inferência LLM.",
                  "- Previsão SJF-IA requer modelo treinado separadamente.",
                  "- Configurações ausentes não recebem resultados fictícios.",
                  "", "## Arquivos", "- scheduling.csv, memory.csv, report.json"])
    notes.extend("- " + chart for chart in charts)
    (output / "report.md").write_text("\n".join(notes) + "\n", encoding="utf-8")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--experiments", required=True, type=Path)
    p.add_argument("--output-dir", required=True, type=Path)
    args = p.parse_args(argv)
    root, out = args.experiments.resolve(), args.output_dir.resolve()
    if out.exists():
        p.error("output directory already exists (will not overwrite)")
    try:
        original, rows, labs = validate(root)
        digest = hashlib.sha256((root / "experiments.json").read_bytes()).hexdigest()
        out.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".osllm-h24-", dir=out.parent) as path:
            export(Path(path), original, rows, labs, digest)
            if out.exists():
                raise ValueError("output directory appeared while rendering")
            os.rename(path, out)
    except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError) as exc:
        p.exit(2, "error: " + str(exc) + "\n")
    print("Report created:", out)


if __name__ == "__main__":
    main()
