# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 3df79291d2e6cb987d0f62d67ed6e8baabb550c386b40a7e137fa103c955e526
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 60.915 | 113 | 60.915 | 125 | 77.19 | 0.1223 | 0.9954 | 0 |
| sjf | 38.075 | 147 | 38.075 | 758 | 54.35 | 0.1223 | 0.9954 | 6 |
| hrrn | 49.43 | 120 | 49.43 | 194 | 65.705 | 0.1225 | 0.9966 | 0 |
| rr_q1 | 90.815 | 227 | 4.92 | 266 | 107.09 | 0.1224 | 0.996 | 0 |
| rr_q3 | 87.88 | 224 | 14.345 | 260 | 104.155 | 0.1223 | 0.9954 | 0 |
| priority_aging | 62.325 | 114 | 62.325 | 126 | 78.6 | 0.1222 | 0.9942 | 0 |
| priority_no_aging | 58.83 | 277 | 58.83 | 669 | 75.105 | 0.1225 | 0.9966 | 8 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1809 | 2896 | 2722 | 2 | 7250 | 466.28 |
| memory_lru_f2 | 2371 | 1770 | 1569 | 2 | 4430 | 317.54 |
| memory_fifo_f4 | 2475 | 1740 | 1539 | 4 | 4800 | 338.51 |
| memory_lru_f4 | 2468 | 1673 | 1469 | 4 | 4430 | 317.54 |
| memory_fifo_f8 | 3196 | 947 | 607 | 8 | 4440 | 318.205 |
| memory_lru_f8 | 3147 | 994 | 628 | 8 | 4430 | 317.54 |
| memory_fifo_f16 | 3241 | 900 | 176 | 16 | 4430 | 317.54 |
| memory_lru_f16 | 3230 | 911 | 185 | 16 | 4430 | 317.54 |

## E1 e E4 — Laboratórios
- E1 safe: completed: mode=safe expected=2 observed=2 lost=0
- E1 unsafe: completed: mode=unsafe expected=2 observed=1 lost=1
- E4 ordered: completed: mode=ordered completed=2 watchdog=none deadlock=no
- E4 deadlock: completed: mode=deadlock acquired=2 watchdog=SIGKILL deadlock=observed

## Limitações
- Um cenário não demonstra superioridade universal de uma política.
- OPT é somente offline, não foi comparado como política online.
- Testes de corrida e deadlock são laboratórios, não inferência LLM.
- Configurações ausentes não recebem resultados fictícios.

## Arquivos
- scheduling.csv, memory.csv, report.json
- scheduling_wait.svg
- memory_faults.svg
