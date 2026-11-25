# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 9a3f0fdad5ca7a2df09b2c3a48c5c8f0d8acc8c95c9fa3a02a1e5a7bacdeb457
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 288.905 | 575 | 288.905 | 592 | 304.69 | 0.0919 | 0.7251 | 85 |
| sjf | 181.73 | 587 | 181.73 | 683 | 197.515 | 0.0915 | 0.7221 | 58 |
| hrrn | 197.19 | 546 | 197.19 | 635 | 212.975 | 0.0915 | 0.7224 | 51 |
| rr_q1 | 399.86 | 656 | 20.045 | 699 | 415.645 | 0.092 | 0.7257 | 132 |
| rr_q3 | 393.6 | 652 | 59.01 | 685 | 409.385 | 0.0919 | 0.7254 | 126 |
| priority_aging | 288.705 | 576 | 288.705 | 595 | 304.49 | 0.0916 | 0.7228 | 85 |
| priority_no_aging | 282.135 | 644 | 282.135 | 680 | 297.92 | 0.0915 | 0.7224 | 85 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1768 | 2788 | 2611 | 2 | 6995 | 498.885 |
| memory_lru_f2 | 2298 | 1725 | 1524 | 2 | 4330 | 424.32 |
| memory_fifo_f4 | 2408 | 1665 | 1460 | 4 | 4580 | 432.59 |
| memory_lru_f4 | 2395 | 1628 | 1422 | 4 | 4330 | 424.32 |
| memory_fifo_f8 | 3121 | 903 | 553 | 8 | 4335 | 424.485 |
| memory_lru_f8 | 3059 | 964 | 580 | 8 | 4330 | 424.32 |
| memory_fifo_f16 | 3149 | 874 | 139 | 16 | 4330 | 424.32 |
| memory_lru_f16 | 3143 | 880 | 143 | 16 | 4330 | 424.32 |

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
