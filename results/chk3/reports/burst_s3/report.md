# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: 359d5cd735979a015ca96eb81fba7662ca8c3a2e11fdf91d3590b7afd4464f22
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 317.585 | 674 | 317.585 | 752 | 334.325 | 0.0941 | 0.7874 | 97 |
| sjf | 205.565 | 723 | 205.565 | 888 | 222.305 | 0.0939 | 0.7855 | 57 |
| hrrn | 220.11 | 644 | 220.11 | 810 | 236.85 | 0.0938 | 0.7848 | 55 |
| rr_q1 | 446.715 | 807 | 20.15 | 854 | 463.455 | 0.0941 | 0.7878 | 143 |
| rr_q3 | 438.615 | 795 | 59.27 | 847 | 455.355 | 0.0941 | 0.7874 | 140 |
| priority_aging | 317.645 | 682 | 317.645 | 744 | 334.385 | 0.0941 | 0.7874 | 99 |
| priority_no_aging | 319.73 | 773 | 319.73 | 802 | 336.47 | 0.0939 | 0.7855 | 94 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1897 | 2908 | 2740 | 2 | 7285 | 537.105 |
| memory_lru_f2 | 2442 | 1818 | 1617 | 2 | 4560 | 463.285 |
| memory_fifo_f4 | 2574 | 1748 | 1547 | 4 | 4870 | 472.64 |
| memory_lru_f4 | 2548 | 1712 | 1507 | 4 | 4560 | 463.285 |
| memory_fifo_f8 | 3302 | 962 | 621 | 8 | 4580 | 463.66 |
| memory_lru_f8 | 3264 | 996 | 621 | 8 | 4560 | 463.285 |
| memory_fifo_f16 | 3346 | 914 | 167 | 16 | 4560 | 463.285 |
| memory_lru_f16 | 3340 | 920 | 172 | 16 | 4560 | 463.285 |

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
