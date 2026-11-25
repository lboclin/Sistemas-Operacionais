# Agregado dos experimentos OS-LLM

Médias sobre as seeds de cada grupo, em ticks virtuais. p95 e máximo são médias dos
valores de cada seed; worst_max_wait_ticks é o pior caso e starvation_count é a soma.

## burst (5 carga(s))

Configuração: `{"aging_interval": 2, "frames": [2, 4, 8, 16], "page_fault_latency": 5, "starvation_threshold": 320, "timeout_seconds": 30, "tokens_per_page": 4, "workers": 2}`

### E2 — Escalonamento
| name | average_wait_ticks | sd_average_wait_ticks | p95_wait_ticks | max_wait_ticks | worst_max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 308.868 | 14.5665 | 625 | 676.6 | 761 | 325.354 | 0.0898 | 0.7395 | 467 |
| sjf | 195.439 | 13.3939 | 659.6 | 804.6 | 911 | 211.925 | 0.0895 | 0.7376 | 290 |
| hrrn | 211.341 | 13.7526 | 614.4 | 718.6 | 810 | 227.827 | 0.0896 | 0.7383 | 277 |
| rr_q1 | 427.831 | 27.2836 | 740.2 | 782.4 | 881 | 444.317 | 0.0899 | 0.7403 | 674 |
| rr_q3 | 421.392 | 26.2761 | 732.8 | 775.4 | 882 | 437.878 | 0.0898 | 0.7402 | 664 |
| priority_aging | 309.12 | 15.1288 | 624.8 | 676.8 | 772 | 325.606 | 0.0896 | 0.7381 | 473 |
| priority_no_aging | 307.648 | 26.1392 | 720.6 | 747.2 | 837 | 324.134 | 0.0897 | 0.7389 | 460 |

### E3 — Memória
| name | page_faults | page_hits | page_evictions | total_blocked_ticks | average_turnaround_ticks | throughput_per_tick |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 2925.2 | 1839.6 | 2752.6 | 7338 | 530.273 | 0.0767 |
| memory_lru_f2 | 1791.2 | 2404.4 | 1590.2 | 4492 | 450.911 | 0.081 |
| memory_fifo_f4 | 1732.8 | 2524.4 | 1530.4 | 4800 | 460.338 | 0.0805 |
| memory_lru_f4 | 1691.2 | 2504.4 | 1485.6 | 4492 | 450.911 | 0.081 |
| memory_fifo_f8 | 943.2 | 3254.8 | 599.2 | 4504 | 451.25 | 0.081 |
| memory_lru_f8 | 997.6 | 3198 | 620.6 | 4492 | 450.911 | 0.081 |
| memory_fifo_f16 | 903 | 3292.6 | 170.6 | 4492 | 450.911 | 0.081 |
| memory_lru_f16 | 913.2 | 3282.4 | 177.6 | 4492 | 450.911 | 0.081 |

## starvation (1 carga(s))

Configuração: `{"aging_interval": 2, "frames": [2, 4, 8, 16], "page_fault_latency": 5, "starvation_threshold": 320, "timeout_seconds": 30, "tokens_per_page": 4, "workers": 1}`

### E2 — Escalonamento
| name | average_wait_ticks | sd_average_wait_ticks | p95_wait_ticks | max_wait_ticks | worst_max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 50.9 | 0 | 52 | 52 | 52 | 55.08 | 0.2392 | 1 | 0 |
| sjf | 15.8 | 0 | 12 | 796 | 796 | 19.98 | 0.2392 | 1 | 1 |
| hrrn | 46.22 | 0 | 52 | 120 | 120 | 50.4 | 0.2392 | 1 | 0 |
| rr_q1 | 68.26 | 0 | 84 | 532 | 532 | 72.44 | 0.2392 | 1 | 1 |
| rr_q3 | 73.79 | 0 | 85 | 377 | 377 | 77.97 | 0.2392 | 1 | 1 |
| priority_aging | 50.9 | 0 | 52 | 52 | 52 | 55.08 | 0.2392 | 1 | 0 |
| priority_no_aging | 15.8 | 0 | 12 | 796 | 796 | 19.98 | 0.2392 | 1 | 1 |

### E3 — Memória
| name | page_faults | page_hits | page_evictions | total_blocked_ticks | average_turnaround_ticks | throughput_per_tick |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 371 | 674 | 171 | 1045 | 165.81 | 0.1908 |
| memory_lru_f2 | 371 | 674 | 171 | 1045 | 165.81 | 0.1908 |
| memory_fifo_f4 | 295 | 750 | 93 | 1045 | 165.81 | 0.1908 |
| memory_lru_f4 | 295 | 750 | 93 | 1045 | 165.81 | 0.1908 |
| memory_fifo_f8 | 209 | 836 | 4 | 1045 | 165.81 | 0.1908 |
| memory_lru_f8 | 209 | 836 | 4 | 1045 | 165.81 | 0.1908 |
| memory_fifo_f16 | 209 | 836 | 0 | 1045 | 165.81 | 0.1908 |
| memory_lru_f16 | 209 | 836 | 0 | 1045 | 165.81 | 0.1908 |

## uniform (5 carga(s))

Configuração: `{"aging_interval": 2, "frames": [2, 4, 8, 16], "page_fault_latency": 5, "starvation_threshold": 320, "timeout_seconds": 30, "tokens_per_page": 4, "workers": 2}`

### E2 — Escalonamento
| name | average_wait_ticks | sd_average_wait_ticks | p95_wait_ticks | max_wait_ticks | worst_max_wait_ticks | average_turnaround_ticks | throughput_per_tick | worker_utilization | starvation_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fcfs | 56.306 | 17.8276 | 105.4 | 121 | 159 | 72.614 | 0.1187 | 0.968 | 0 |
| sjf | 34.006 | 9.2891 | 147.6 | 645 | 961 | 50.314 | 0.1186 | 0.967 | 23 |
| hrrn | 44.566 | 13.1411 | 110.6 | 154.8 | 194 | 60.874 | 0.1187 | 0.9677 | 0 |
| rr_q1 | 80.76 | 25.0902 | 206.6 | 250.4 | 302 | 97.068 | 0.119 | 0.9702 | 0 |
| rr_q3 | 78.707 | 24.273 | 197.2 | 237.4 | 278 | 95.015 | 0.119 | 0.9702 | 0 |
| priority_aging | 56.608 | 17.8994 | 107.8 | 121.2 | 159 | 72.916 | 0.1188 | 0.9681 | 0 |
| priority_no_aging | 59.094 | 17.1995 | 329.6 | 598.2 | 831 | 75.402 | 0.1189 | 0.9693 | 50 |

### E3 — Memória
| name | page_faults | page_hits | page_evictions | total_blocked_ticks | average_turnaround_ticks | throughput_per_tick |
| --- | --- | --- | --- | --- | --- | --- |
| memory_fifo_f2 | 2875.6 | 1827.2 | 2706.8 | 7206 | 438.109 | 0.0834 |
| memory_lru_f2 | 1770 | 2379 | 1569 | 4437 | 294.512 | 0.0943 |
| memory_fifo_f4 | 1728.4 | 2491.6 | 1526.8 | 4792 | 314.7 | 0.0928 |
| memory_lru_f4 | 1667.2 | 2481.8 | 1463.6 | 4437 | 294.512 | 0.0943 |
| memory_fifo_f8 | 939.8 | 3211.2 | 596.6 | 4447 | 295.094 | 0.0942 |
| memory_lru_f8 | 984.2 | 3164.8 | 613.8 | 4437 | 294.512 | 0.0943 |
| memory_fifo_f16 | 895.6 | 3253.4 | 174.4 | 4437 | 294.512 | 0.0943 |
| memory_lru_f16 | 905.8 | 3243.2 | 180.8 | 4437 | 294.512 | 0.0943 |

## E1 e E4 — Laboratórios

- E1 safe: 11 execução(ões): mode=safe expected=2 observed=2 lost=0
- E1 unsafe: 11 execução(ões): mode=unsafe expected=2 observed=1 lost=1
- E4 deadlock: 11 execução(ões): mode=deadlock acquired=2 watchdog=SIGKILL deadlock=observed
- E4 ordered: 11 execução(ões): mode=ordered completed=2 watchdog=none deadlock=no

## Procedência

- burst_s1: experiments.json SHA-256 b5a86b77a56fc85819350b035521d41136ad084e01060ecbd8355fa25af91b23
- burst_s2: experiments.json SHA-256 9a3f0fdad5ca7a2df09b2c3a48c5c8f0d8acc8c95c9fa3a02a1e5a7bacdeb457
- burst_s3: experiments.json SHA-256 359d5cd735979a015ca96eb81fba7662ca8c3a2e11fdf91d3590b7afd4464f22
- burst_s4: experiments.json SHA-256 2946239d8af5dc4a3e8cd4cf4defc076d7f73eade80f2f6ef139a1a6411bd38e
- burst_s5: experiments.json SHA-256 7e60e87ca4769856c3ac3e0b9a7f66fa5ee6d1b612e0ca4bc199802bacc4c51c
- starvation: experiments.json SHA-256 5ab3ad7663d9763a2f56a805895220ec8f9fd3602cc84bbdf1bbb454877dd611
- uniform_s1: experiments.json SHA-256 049992425ac78e2ea62a1d019ec142158a06212b517697212bae4014b99aaba2
- uniform_s2: experiments.json SHA-256 eecb0ef2a9aa24a682bcdcc2b184d64685c9b4d7de6c951ac161a2188132311a
- uniform_s3: experiments.json SHA-256 3df79291d2e6cb987d0f62d67ed6e8baabb550c386b40a7e137fa103c955e526
- uniform_s4: experiments.json SHA-256 ed50424016c0622400aa86ba8fec6d26a29ccb24a6ec99728948891c839c7d56
- uniform_s5: experiments.json SHA-256 22223540ddb4913db57a41d9d663ed7a425d58530cb559ec96804be264b804f8

## Gráficos

- burst_wait_avg.svg
- burst_wait_p95.svg
- burst_wait_max.svg
- burst_page_faults.svg
- burst_turnaround.svg
- starvation_wait_avg.svg
- starvation_wait_p95.svg
- starvation_wait_max.svg
- starvation_page_faults.svg
- starvation_turnaround.svg
- uniform_wait_avg.svg
- uniform_wait_p95.svg
- uniform_wait_max.svg
- uniform_page_faults.svg
- uniform_turnaround.svg
