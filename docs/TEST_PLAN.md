# Estratégia de testes (planejamento)

> Testes não implementados nem executados. Framework e comandos dependem da stack futura.

| Tipo | Cobertura |
|---|---|
| Unidade | máquina de estados, FCFS/SJF/RR, aging, FIFO/LRU/OPT, cálculos de métricas |
| Integração | gateway→scheduler→runtime→memória→eventos; invariantes |
| Concorrência | exclusão mútua, cleanup, timeout, race observável, deadlock controlado |
| Sistema | execução completa de cenários, exportação, repetibilidade |
| Interface | criação de requisição até resultado, após escolher framework |

**Cenários de risco:** nenhum PCB em dois workers simultaneamente; nunca alocar além da memória configurada; despertar pedidos bloqueados quando recurso volta; tratamento de `SIGUSR1` fora do handler; garantir que preditor não enxergue a resposta real; corrida e deadlock protegidos por timeout/watchdog.

Suítes não devem depender de corrida acontecer necessariamente. Testes devem ter critérios de aceite e condições de execução verificáveis.

Ver [especificação funcional](FUNCTIONAL_SPEC.md).
