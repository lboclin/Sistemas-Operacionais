# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 5ab3ad7663d9763a2f56a805895220ec8f9fd3602cc84bbdf1bbb454877dd611
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 50.9 | 52 | 50.9 | 52 | 55.08 | 0.2392 | 1 | 0 |
| sjf | 15.8 | 12 | 15.8 | 796 | 19.98 | 0.2392 | 1 | 1 |
| hrrn | 46.22 | 52 | 46.22 | 120 | 50.4 | 0.2392 | 1 | 0 |
| rr_q1 | 68.26 | 84 | 15.855 | 532 | 72.44 | 0.2392 | 1 | 1 |
| rr_q3 | 73.79 | 85 | 34.82 | 377 | 77.97 | 0.2392 | 1 | 1 |
| priority_aging | 50.9 | 52 | 50.9 | 52 | 55.08 | 0.2392 | 1 | 0 |
| priority_no_aging | 15.8 | 12 | 15.8 | 796 | 19.98 | 0.2392 | 1 | 1 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 674 | 371 | 171 | 2 | 1045 | 165.81 |
| memory_lru_f2 | 674 | 371 | 171 | 2 | 1045 | 165.81 |
| memory_fifo_f4 | 750 | 295 | 93 | 4 | 1045 | 165.81 |
| memory_lru_f4 | 750 | 295 | 93 | 4 | 1045 | 165.81 |
| memory_fifo_f8 | 836 | 209 | 4 | 8 | 1045 | 165.81 |
| memory_lru_f8 | 836 | 209 | 4 | 8 | 1045 | 165.81 |
| memory_fifo_f16 | 836 | 209 | 0 | 12 | 1045 | 165.81 |
| memory_lru_f16 | 836 | 209 | 0 | 12 | 1045 | 165.81 |

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
