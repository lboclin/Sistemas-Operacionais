# H23 — Experimentos automatizados

Runner: python3 tools/experiments/run.py --cli ./build/os_llm_cli --scenario scenarios/seed42-v2.csv --race-lab ./build/os_llm_race_lab --deadlock-lab ./build/os_llm_deadlock_lab --source synthetic_fixture --output-dir results/h23-example

E1: corrida lógica safe/unsafe (não é data race C indefinida).
E2: FCFS, SJF (pelo tamanho da entrada, conhecido na chegada), Round Robin (q1/q3) e prioridade com aging padrão. Não há CLI para desativar aging, então essa execução fica skipped.
E3: FIFO/LRU online variando frames; OPT offline não é executável como política online, então fica skipped.
E4: laboratório real de mutexes pthread em subprocesso protegido por watchdog; não representa detecção de deadlock no runtime principal.

Resultados: experiments.json (os-llm-experiments/v1) e runs/*.json, mantendo os dumps integrais da CLI e verificando a mesma carga em cada execução. Diretório de saída só pode ser criado uma vez.
Fonte informada no parâmetro --source é declaração do operador (source_verified=false): não comprova coleta real. Ticks virtuais não são milissegundos reais; não reportar desempenho de modelos sem medir.

Testes locais sem binários: python3 -m unittest discover -s tools/experiments -p test_runner.py -v
CTest com binários: experiments_runner_py, incluindo smoke real se estiverem compilados.
Análise em tabelas e gráficos fica para o commit H24; esta entrega apenas mede e preserva resultados.
