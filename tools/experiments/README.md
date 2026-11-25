# H23 — Experimentos automatizados

Runner: python3 tools/experiments/run.py --cli ./build/os_llm_cli --scenario scenarios/seed42-v2.csv --race-lab ./build/os_llm_race_lab --deadlock-lab ./build/os_llm_deadlock_lab --source synthetic_fixture --output-dir results/h23-example

E1: corrida lógica safe/unsafe (não é data race C indefinida).
E2: FCFS, SJF (pelo tamanho da entrada, conhecido na chegada), HRRN, Round Robin (q1/q3) e prioridade com e sem aging. `--aging-interval N` (padrão 2) define o intervalo da execução com aging; a execução sem aging usa sempre 0. `--starvation-threshold N` vale para todas as políticas.
E3: FIFO/LRU online variando frames, com `--page-fault-latency` (padrão 1); OPT offline não é executável como política online, então fica skipped.
E4: laboratório real de mutexes pthread em subprocesso protegido por watchdog; não representa detecção de deadlock no runtime principal.

Resultados: experiments.json (os-llm-experiments/v1) e runs/*.json, mantendo os dumps integrais da CLI e verificando a mesma carga em cada execução. Diretório de saída só pode ser criado uma vez.
Fonte informada no parâmetro --source é declaração do operador (source_verified=false): não comprova coleta real. Ticks virtuais não são milissegundos reais; não reportar desempenho de modelos sem medir.

Campanha do CHK3 (E1–E4 sobre 11 cargas, relatórios e agregado): `tools/experiments/chk3.sh`. Os resultados versionados ficam em `results/chk3/`; os dumps de `raw/*/runs/` não são versionados por tamanho, mas o script os recria e o `trace_fingerprint` de cada manifest permite conferir.

Testes locais sem binários: python3 -m unittest discover -s tools/experiments -p test_runner.py -v
CTest com binários: experiments_runner_py, incluindo smoke real se estiverem compilados.
Análise em tabelas e gráficos fica para o commit H24; esta entrega apenas mede e preserva resultados.
