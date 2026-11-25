# Relatório de experimentos OS-LLM

## Procedência e método
Origem declarada: synthetic_fixture (não verificada independentemente).
Manifest de entrada SHA-256: b5a86b77a56fc85819350b035521d41136ad084e01060ecbd8355fa25af91b23
As execuções comparáveis usam a mesma carga, verificada nos JSONs brutos.
Métricas de espera, resposta, retorno e throughput usam ticks virtuais.
Não representam a latência de um LLM real. SJF e HRRN usam o tamanho da entrada, conhecido na chegada.

## E2 — Escalonamento
| name | average_wait_ticks | p95_wait_ticks | average_response_ticks | max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 331.6 | 687 | 331.6 | 761 | 348.715 | 0.0839 | 0.7179 | 100 |
| sjf | 214.855 | 724 | 214.855 | 911 | 231.97 | 0.0836 | 0.7158 | 63 |
| hrrn | 231.98 | 696 | 231.98 | 806 | 249.095 | 0.0836 | 0.7155 | 65 |
| rr_q1 | 469.05 | 837 | 21.655 | 881 | 486.165 | 0.0839 | 0.7182 | 139 |
| rr_q3 | 461.235 | 824 | 63.85 | 882 | 478.35 | 0.0839 | 0.7182 | 138 |
| priority_aging | 333.145 | 683 | 333.145 | 772 | 350.26 | 0.0836 | 0.7152 | 102 |
| priority_no_aging | 351.91 | 817 | 351.91 | 837 | 369.025 | 0.0839 | 0.7176 | 107 |

## E3 — Memória
| name | page_hits | page_faults | page_evictions | peak_resident_frames | total_blocked_ticks | average_turnaround_ticks |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 1920 | 3025 | 2850 | 2 | 7610 | 569.465 |
| memory_lru_f2 | 2499 | 1855 | 1654 | 2 | 4655 | 482.095 |
| memory_fifo_f4 | 2617 | 1803 | 1601 | 4 | 4985 | 492.47 |
| memory_lru_f4 | 2608 | 1746 | 1541 | 4 | 4655 | 482.095 |
| memory_fifo_f8 | 3380 | 975 | 628 | 8 | 4660 | 482.175 |
| memory_lru_f8 | 3311 | 1043 | 654 | 8 | 4655 | 482.095 |
| memory_fifo_f16 | 3421 | 933 | 193 | 16 | 4655 | 482.095 |
| memory_lru_f16 | 3403 | 951 | 205 | 16 | 4655 | 482.095 |

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
