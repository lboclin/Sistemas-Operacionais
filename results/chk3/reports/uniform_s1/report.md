# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 049992425ac78e2ea62a1d019ec142158a06212b517697212bae4014b99aaba2
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 80.97 | 122 | 80.97 | 133 | 97.18 | 0.1201 | 0.9736 | 0 |
| sjf | 45.135 | 177 | 45.135 | 804 | 61.345 | 0.1197 | 0.9701 | 8 |
| hrrn | 61.64 | 132 | 61.64 | 173 | 77.85 | 0.1198 | 0.9707 | 0 |
| rr_q1 | 113.465 | 250 | 6.52 | 302 | 129.675 | 0.12 | 0.9724 | 0 |
| rr_q3 | 110.125 | 233 | 18.78 | 278 | 126.335 | 0.12 | 0.9724 | 0 |
| priority_aging | 80.865 | 125 | 80.865 | 134 | 97.075 | 0.1201 | 0.9736 | 0 |
| priority_no_aging | 76.015 | 438 | 76.015 | 831 | 92.225 | 0.1201 | 0.9736 | 15 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1827 | 2839 | 2674 | 2 | 7120 | 486.76 |
| memory_lru_f2 | 2370 | 1751 | 1550 | 2 | 4395 | 335.95 |
| memory_fifo_f4 | 2481 | 1712 | 1510 | 4 | 4755 | 355.3 |
| memory_lru_f4 | 2487 | 1634 | 1431 | 4 | 4395 | 335.95 |
| memory_fifo_f8 | 3180 | 944 | 604 | 8 | 4410 | 336.87 |
| memory_lru_f8 | 3128 | 993 | 620 | 8 | 4395 | 335.95 |
| memory_fifo_f16 | 3232 | 889 | 176 | 16 | 4395 | 335.95 |
| memory_lru_f16 | 3218 | 903 | 187 | 16 | 4395 | 335.95 |

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
