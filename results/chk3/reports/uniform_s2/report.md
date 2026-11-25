# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: eecb0ef2a9aa24a682bcdcc2b184d64685c9b4d7de6c951ac161a2188132311a
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 56.835 | 109 | 56.835 | 126 | 73.39 | 0.1172 | 0.9704 | 0 |
| sjf | 34.86 | 188 | 34.86 | 546 | 51.415 | 0.117 | 0.9687 | 5 |
| hrrn | 44.76 | 125 | 44.76 | 140 | 61.315 | 0.1175 | 0.9727 | 0 |
| rr_q1 | 78.45 | 215 | 4.125 | 272 | 95.005 | 0.1179 | 0.9755 | 0 |
| rr_q3 | 77.085 | 205 | 12.14 | 256 | 93.64 | 0.1178 | 0.975 | 0 |
| priority_aging | 57.06 | 114 | 57.06 | 122 | 73.615 | 0.1175 | 0.9727 | 0 |
| priority_no_aging | 63.23 | 349 | 63.23 | 468 | 79.785 | 0.1173 | 0.971 | 11 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1867 | 2896 | 2729 | 2 | 7260 | 442.265 |
| memory_lru_f2 | 2415 | 1796 | 1595 | 2 | 4500 | 298.265 |
| memory_fifo_f4 | 2537 | 1750 | 1553 | 4 | 4880 | 321.165 |
| memory_lru_f4 | 2525 | 1686 | 1483 | 4 | 4500 | 298.265 |
| memory_fifo_f8 | 3262 | 951 | 609 | 8 | 4510 | 298.95 |
| memory_lru_f8 | 3221 | 990 | 629 | 8 | 4500 | 298.265 |
| memory_fifo_f16 | 3304 | 907 | 174 | 16 | 4500 | 298.265 |
| memory_lru_f16 | 3293 | 918 | 177 | 16 | 4500 | 298.265 |

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
