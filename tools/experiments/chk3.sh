#!/usr/bin/env bash
# Regenerates results/chk3: 10 seeded workloads of 200 requests plus the starvation scenario.
# Usage: tools/experiments/chk3.sh [BUILD_DIR] [OUTPUT_DIR]
set -euo pipefail
cd "$(dirname "$0")/../.."
build=${1:-build}
out=${2:-results/chk3}

if [ -e "$out/raw" ] || [ -e "$out/reports" ] || [ -e "$out/aggregate" ]; then
    echo "error: $out already has results; remove raw/, reports/ and aggregate/ first" >&2
    exit 2
fi
mkdir -p "$out/workloads"

measure() { # NAME WORKERS
    python3 tools/experiments/run.py --cli "$build/os_llm_cli" \
        --race-lab "$build/os_llm_race_lab" --deadlock-lab "$build/os_llm_deadlock_lab" \
        --scenario "$out/workloads/$1.csv" --source synthetic_fixture --workers "$2" \
        --aging-interval 2 --starvation-threshold 320 \
        --frames 2 4 8 16 --page-tokens 4 --page-fault-latency 5 \
        --output-dir "$out/raw/$1"
    python3 tools/analysis/report.py --experiments "$out/raw/$1" --output-dir "$out/reports/$1"
}

for distribution in uniform burst; do
    for seed in 1 2 3 4 5; do
        name=${distribution}_s${seed}
        "$build/os_llm_workload" --seed "$seed" --count 200 --distribution "$distribution" \
            --arrival-span 1600 --min-tokens 1 --max-tokens 32 --output "$out/workloads/$name.csv"
        measure "$name" 2
    done
done
cp scenarios/starvation.csv "$out/workloads/starvation.csv"
measure starvation 1

python3 tools/analysis/report.py --aggregate "$out/raw" --output-dir "$out/aggregate"
