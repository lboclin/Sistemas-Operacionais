# CHK3: resumo dos experimentos

Os valores são ticks virtuais (1 tick = 1 token de entrada), não latência real. Tabelas completas em [aggregate/aggregate.md](aggregate/aggregate.md) e gráficos em `aggregate/*.svg`. Para refazer tudo, remova `raw/`, `reports/` e `aggregate/` e rode `tools/experiments/chk3.sh`.

## Configuração

| Item | Valor |
| --- | --- |
| Cargas | 200 requisições, entrada de 1 a 32 tokens, prioridade de 0 a 5, `--arrival-span 1600`, seeds 1–5, `uniform` e `burst` |
| Starvation | `scenarios/starvation.csv`: 1 pedido de 40 tokens em t=0, 4 curtos (4 tokens) em t=0 e mais um a cada 4 ticks (199 curtos no total) |
| Workers | 2 (cargas geradas) e 1 (starvation) |
| `--starvation-threshold` | 320 em todas as políticas e cargas (10 × o maior pedido gerado) |
| Aging | intervalo 2 (com) e 0 (sem) |
| E3 | FCFS, 4 tokens por página, latência de falta de 5 ticks, 2, 4, 8 e 16 frames |

Médias, p95 e máximo são médias das 5 seeds. "Pior máx" é o maior valor entre as seeds e `starv` é a soma das 1000 requisições (200 no cenário de starvation).

## E2: escalonamento

### uniform

| Política | Espera média | p95 | Máx | Pior máx | Turnaround | Throughput | Utilização | starv |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FCFS | 56,3 | 105,4 | 121,0 | 159 | 72,6 | 0,1187 | 96,8% | 0 |
| SJF | 34,0 | 147,6 | 645,0 | 961 | 50,3 | 0,1186 | 96,7% | 23 |
| HRRN | 44,6 | 110,6 | 154,8 | 194 | 60,9 | 0,1187 | 96,8% | 0 |
| RR q=1 | 80,8 | 206,6 | 250,4 | 302 | 97,1 | 0,1190 | 97,0% | 0 |
| RR q=3 | 78,7 | 197,2 | 237,4 | 278 | 95,0 | 0,1190 | 97,0% | 0 |
| Prioridade com aging | 56,6 | 107,8 | 121,2 | 159 | 72,9 | 0,1188 | 96,8% | 0 |
| Prioridade sem aging | 59,1 | 329,6 | 598,2 | 831 | 75,4 | 0,1189 | 96,9% | 50 |

### burst

| Política | Espera média | p95 | Máx | Pior máx | Turnaround | Throughput | Utilização | starv |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FCFS | 308,9 | 625,0 | 676,6 | 761 | 325,4 | 0,0898 | 73,9% | 467 |
| SJF | 195,4 | 659,6 | 804,6 | 911 | 211,9 | 0,0895 | 73,8% | 290 |
| HRRN | 211,3 | 614,4 | 718,6 | 810 | 227,8 | 0,0896 | 73,8% | 277 |
| RR q=1 | 427,8 | 740,2 | 782,4 | 881 | 444,3 | 0,0899 | 74,0% | 674 |
| RR q=3 | 421,4 | 732,8 | 775,4 | 882 | 437,9 | 0,0898 | 74,0% | 664 |
| Prioridade com aging | 309,1 | 624,8 | 676,8 | 772 | 325,6 | 0,0896 | 73,8% | 473 |
| Prioridade sem aging | 307,6 | 720,6 | 747,2 | 837 | 324,1 | 0,0897 | 73,9% | 460 |

### Cenário de starvation (1 worker)

O throughput (0,2392) e a utilização (100%) são iguais em todas. No SJF, no HRRN e na prioridade sem aging, a espera máxima é a do pedido longo. Na prioridade com aging e no FCFS, o pedido longo espera 16 ticks e o máximo (52) é de um curto.

| Política | Espera média | p95 | Máx | Turnaround | starv |
| --- | --- | --- | --- | --- | --- |
| FCFS | 50,9 | 52 | 52 | 55,1 | 0 |
| SJF | 15,8 | 12 | 796 | 20,0 | 1 |
| HRRN | 46,2 | 52 | 120 | 50,4 | 0 |
| RR q=1 | 68,3 | 84 | 532 | 72,4 | 1 |
| RR q=3 | 73,8 | 85 | 377 | 78,0 | 1 |
| Prioridade com aging | 50,9 | 52 | 52 | 55,1 | 0 |
| Prioridade sem aging | 15,8 | 12 | 796 | 20,0 | 1 |

## E3: FIFO e LRU (média das 5 cargas uniform)

| Frames | Faltas FIFO | Faltas LRU | Turnaround FIFO | Turnaround LRU | Bloqueio FIFO | Bloqueio LRU |
| --- | --- | --- | --- | --- | --- | --- |
| 2 | 2875,6 | 1770,0 | 438,1 | 294,5 | 7206 | 4437 |
| 4 | 1728,4 | 1667,2 | 314,7 | 294,5 | 4792 | 4437 |
| 8 | 939,8 | 984,2 | 295,1 | 294,5 | 4447 | 4437 |
| 16 | 895,6 | 905,8 | 294,5 | 294,5 | 4437 | 4437 |

Na carga burst o padrão é o mesmo (ver `aggregate.md`). No cenário de starvation, FIFO e LRU empatam em todas as capacidades.

## E1 e E4: laboratórios (11 execuções de cada)

| Laboratório | Modo | Resultado |
| --- | --- | --- |
| E1 corrida | safe | 2 de 2 incrementos, 0 perdidos (11/11) |
| E1 corrida | unsafe | 1 de 2 incrementos, 1 perdido (11/11) |
| E4 deadlock | ordered | concluído, sem deadlock (11/11) |
| E4 deadlock | deadlock | deadlock observado e encerrado pelo watchdog (11/11) |

## Conclusões

1. **HRRN reduz a espera média sem a starvation do SJF.** Na uniform, a média cai 21% frente ao FCFS (56,3 para 44,6), com 0 casos de starvation contra 23 do SJF; a pior espera é 194 ticks, contra 961. O HRRN teve média menor que o FCFS e máximo menor que o SJF nas 5 seeds de cada distribuição. O SJF ainda tem a menor média (34,0 na uniform, 195,4 na burst).
2. **SJF e prioridade sem aging sofrem starvation; HRRN e aging não.** No cenário de starvation, o pedido longo espera 796 ticks no SJF e na prioridade sem aging, 120 no HRRN e 16 com aging. Na uniform, o aging reduz o pior caso de 831 para 159 ticks e zera os 50 casos de starvation.
3. **Aging com intervalo 2 praticamente anula a prioridade.** As prioridades vão de 0 a 5, e 10 ticks de espera cobrem essa diferença. A média com aging (56,6) é quase a do FCFS (56,3).
4. **RR troca espera por resposta.** Na uniform, a resposta média cai de 56,3 (FCFS) para 4,4 (q=1) e 12,8 (q=3), mas a espera média sobe 43% e 40%. Throughput e utilização quase não mudam entre políticas (0,1186 a 0,1190 na uniform): elas mudam quem espera, não o trabalho total.
5. **LRU é melhor quando a memória é escassa; com mais frames, empata.** Com 2 frames, LRU tem 38% menos faltas e 33% menos turnaround que FIFO. Com 8 frames, FIFO tem um pouco menos faltas (939,8 contra 984,2), mas o turnaround é igual (295,1 e 294,5). Em LRU, aumentar frames não reduziu o turnaround.

## Limitações (para citar se perguntarem)

- `waiting_ticks` é a espera acumulada na fila. No RR, `starvation_count` marca quem ficou muito tempo na fila, não quem deixou de executar.
- Com threshold 320, na burst quase metade das requisições passa do limite até no FCFS (467 de 1000), porque as chegadas se concentram em duas ondas. Compare as políticas ali pela média e pelo p95, não pelo `starv`.
- O modelo cobra a latência uma vez por falta atendida. Se a página for removida durante o I/O, a recarga conta como falta, mas não bloqueia de novo. Por isso, com LRU o bloqueio total é constante (4437) e, na uniform seed 1 com 2 frames, há 1751 faltas para 879 páginas distintas.
- E1 e E4 forçam a intercalação (barreira e ordem de aquisição dos mutexes); mostram o fenômeno, não a sua frequência. Deadlock e race rodam em laboratórios separados, fora do runtime principal.
- As cargas são sintéticas (`--source synthetic_fixture`), com 5 seeds por distribuição; as diferenças entre políticas não foram testadas com estatística inferencial.
