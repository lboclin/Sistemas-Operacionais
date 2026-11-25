# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 22223540ddb4913db57a41d9d663ed7a425d58530cb559ec96804be264b804f8
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 25.355 | 52 | 25.355 | 62 | 41.245 | 0.12 | 0.9532 | 0 |
| sjf | 16.995 | 75 | 16.995 | 156 | 32.885 | 0.1199 | 0.9526 | 0 |
| hrrn | 21.185 | 51 | 21.185 | 77 | 37.075 | 0.1203 | 0.9561 | 0 |
| rr_q1 | 36.52 | 96 | 1.75 | 120 | 52.41 | 0.1204 | 0.9567 | 0 |
| rr_q3 | 35.635 | 91 | 5.59 | 115 | 51.525 | 0.1205 | 0.9572 | 0 |
| priority_aging | 25.305 | 53 | 25.305 | 65 | 41.195 | 0.1198 | 0.9521 | 0 |
| priority_no_aging | 26.795 | 104 | 26.795 | 305 | 42.685 | 0.1207 | 0.959 | 0 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1827 | 2707 | 2530 | 2 | 6780 | 369.755 |
| memory_lru_f2 | 2317 | 1727 | 1526 | 2 | 4330 | 248.84 |
| memory_fifo_f4 | 2424 | 1683 | 1478 | 4 | 4645 | 266.765 |
| memory_lru_f4 | 2403 | 1641 | 1437 | 4 | 4330 | 248.84 |
| memory_fifo_f8 | 3138 | 907 | 564 | 8 | 4335 | 248.94 |
| memory_lru_f8 | 3082 | 962 | 585 | 8 | 4330 | 248.84 |
| memory_fifo_f16 | 3172 | 872 | 170 | 16 | 4330 | 248.84 |
| memory_lru_f16 | 3164 | 880 | 180 | 16 | 4330 | 248.84 |

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
