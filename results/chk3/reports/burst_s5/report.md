# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 7e60e87ca4769856c3ac3e0b9a7f66fa5ee6d1b612e0ca4bc199802bacc4c51c
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 301.765 | 582 | 301.765 | 627 | 317.69 | 0.0892 | 0.7106 | 91 |
| sjf | 180.305 | 623 | 180.305 | 762 | 196.23 | 0.0892 | 0.7106 | 56 |
| hrrn | 195.77 | 585 | 195.77 | 697 | 211.695 | 0.0894 | 0.7119 | 49 |
| rr_q1 | 398.205 | 691 | 20.39 | 736 | 414.13 | 0.0896 | 0.7132 | 126 |
| rr_q3 | 393.205 | 689 | 59.815 | 734 | 409.13 | 0.0896 | 0.7132 | 127 |
| priority_aging | 301.615 | 581 | 301.615 | 622 | 317.54 | 0.0892 | 0.7106 | 93 |
| priority_no_aging | 282.615 | 687 | 282.615 | 704 | 298.54 | 0.0895 | 0.7128 | 79 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1757 | 2864 | 2694 | 2 | 7180 | 517.48 |
| memory_lru_f2 | 2318 | 1736 | 1536 | 2 | 4345 | 438.475 |
| memory_fifo_f4 | 2432 | 1688 | 1486 | 4 | 4675 | 448.63 |
| memory_lru_f4 | 2413 | 1641 | 1436 | 4 | 4345 | 438.475 |
| memory_fifo_f8 | 3146 | 912 | 572 | 8 | 4365 | 439.14 |
| memory_lru_f8 | 3075 | 979 | 608 | 8 | 4345 | 438.475 |
| memory_fifo_f16 | 3183 | 871 | 155 | 16 | 4345 | 438.475 |
| memory_lru_f16 | 3173 | 881 | 160 | 16 | 4345 | 438.475 |

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
