# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: ed50424016c0622400aa86ba8fec6d26a29ccb24a6ec99728948891c839c7d56
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 57.455 | 131 | 57.455 | 159 | 74.065 | 0.1141 | 0.9475 | 0 |
| sjf | 34.965 | 151 | 34.965 | 961 | 51.575 | 0.1142 | 0.9481 | 4 |
| hrrn | 45.815 | 125 | 45.815 | 190 | 62.425 | 0.1135 | 0.9427 | 0 |
| rr_q1 | 84.55 | 245 | 4.49 | 292 | 101.16 | 0.1144 | 0.9502 | 0 |
| rr_q3 | 82.81 | 233 | 13.275 | 278 | 99.42 | 0.1145 | 0.9508 | 0 |
| priority_aging | 57.485 | 133 | 57.485 | 159 | 74.095 | 0.1142 | 0.9481 | 0 |
| priority_no_aging | 70.6 | 480 | 70.6 | 718 | 87.21 | 0.114 | 0.9464 | 16 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1806 | 3040 | 2879 | 2 | 7620 | 425.485 |
| memory_lru_f2 | 2422 | 1806 | 1605 | 2 | 4530 | 271.965 |
| memory_fifo_f4 | 2541 | 1757 | 1554 | 4 | 4880 | 291.76 |
| memory_lru_f4 | 2526 | 1702 | 1498 | 4 | 4530 | 271.965 |
| memory_fifo_f8 | 3280 | 950 | 599 | 8 | 4540 | 272.505 |
| memory_lru_f8 | 3246 | 982 | 607 | 8 | 4530 | 271.965 |
| memory_fifo_f16 | 3318 | 910 | 176 | 16 | 4530 | 271.965 |
| memory_lru_f16 | 3311 | 917 | 175 | 16 | 4530 | 271.965 |

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
