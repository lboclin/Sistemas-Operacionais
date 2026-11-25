# Reports from experimental runs

Run the experiments with the compiled binaries, then render the report:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel
    python3 tools/experiments/run.py --cli ./build/os_llm_cli --race-lab ./build/os_llm_race_lab --deadlock-lab ./build/os_llm_deadlock_lab --scenario scenarios/seed42-v2.csv --source synthetic_fixture --output-dir results/experiment-01
    python3 tools/analysis/report.py --experiments results/experiment-01 --output-dir results/report-01

To combine several campaigns (subdirectories `NAME_sN` are seeds of group `NAME`):

    python3 tools/analysis/report.py --aggregate results/chk3/raw --output-dir results/chk3/aggregate

Outputs: report.json, report.md, scheduling.csv, memory.csv and optional SVG charts. All policy measurements are re-checked against unmodified engine exports before publication. Different workload sets, changed metrics, unsafe artifact paths and non-finite numeric values are rejected. Reports are never overwritten. Missing campaigns do not receive fabricated measurements.

Important: Source is a user-supplied claim; ticks are simulated (1 tick = 1 Jev input token, not real latency); OPT is offline-only. Lab outputs are descriptive observations, not general benchmark performance.

Tests: python3 -m unittest discover -s tools/analysis -p test_report.py -v. The CTest test analysis_report_py also runs a compiled run-and-report smoke when binaries are present.
