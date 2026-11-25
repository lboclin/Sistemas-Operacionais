# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 2946239d8af5dc4a3e8cd4cf4defc076d7f73eade80f2f6ef139a1a6411bd38e
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 304.485 | 607 | 304.485 | 651 | 321.35 | 0.0897 | 0.7563 | 94 |
| sjf | 194.74 | 641 | 194.74 | 779 | 211.605 | 0.0894 | 0.7539 | 56 |
| hrrn | 211.655 | 601 | 211.655 | 645 | 228.52 | 0.0897 | 0.7566 | 57 |
| rr_q1 | 425.325 | 710 | 19.355 | 742 | 442.19 | 0.0897 | 0.7566 | 134 |
| rr_q3 | 420.305 | 704 | 56.575 | 729 | 437.17 | 0.0897 | 0.7566 | 133 |
| priority_aging | 304.49 | 602 | 304.49 | 651 | 321.355 | 0.0895 | 0.7546 | 94 |
| priority_no_aging | 301.85 | 682 | 301.85 | 713 | 318.715 | 0.0897 | 0.7563 | 95 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1856 | 3041 | 2868 | 2 | 7620 | 528.43 |
| memory_lru_f2 | 2465 | 1822 | 1620 | 2 | 4570 | 446.38 |
| memory_fifo_f4 | 2591 | 1760 | 1558 | 4 | 4890 | 455.36 |
| memory_lru_f4 | 2558 | 1729 | 1522 | 4 | 4570 | 446.38 |
| memory_fifo_f8 | 3325 | 964 | 622 | 8 | 4580 | 446.79 |
| memory_lru_f8 | 3281 | 1006 | 640 | 8 | 4570 | 446.38 |
| memory_fifo_f16 | 3364 | 923 | 199 | 16 | 4570 | 446.38 |
| memory_lru_f16 | 3353 | 934 | 208 | 16 | 4570 | 446.38 |

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
