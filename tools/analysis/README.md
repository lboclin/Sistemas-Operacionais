# H24 — Reports from H23 experimental runs

Generate an H23 experiment run using the compiled binaries:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel
    python3 tools/experiments/run.py --cli ./build/os_llm_cli --race-lab ./build/os_llm_race_lab --deadlock-lab ./build/os_llm_deadlock_lab --scenario scenarios/seed42-v1.csv --source synthetic_fixture --output-dir results/experiment-01
    python3 tools/analysis/report.py --experiments results/experiment-01 --output-dir results/report-01

Outputs: report.json, report.md, scheduling.csv, memory.csv and optional SVG charts. All policy measurements are re-checked against unmodified engine exports before publication. Different workload sets, changed metrics, unsafe artifact paths and non-finite numeric values are rejected. Reports are never overwritten. Missing campaigns do not receive fabricated measurements.

Important: Source is a user-supplied claim; ticks are simulated (not LLM latency); OPT is offline-only and oracle SJF is a theoretical baseline. Lab outputs are descriptive observations, not general benchmark performance.

Tests: python3 -m unittest discover -s tools/analysis -p test_report.py -v. The CTest test analysis_report_py also runs a compiled H23-to-H24 smoke when binaries are present.
