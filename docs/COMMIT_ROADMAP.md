# Roadmap de implementação por commits — OS-llm-project

> Planejamento revisável de 25/10/2026, baseado na `main` a partir do commit `140ff8a` e na especificação [FUNCTIONAL_SPEC.md](FUNCTIONAL_SPEC.md). Este documento **não cria commits futuros automaticamente**; tarefas agendadas somente tentam executar o próximo trabalho quando houver acesso e testes.  
> **Regra:** cada commit contém uma mudança coesa, testes relevantes e documentação do comportamento real. Não usar timestamps retroativos, commits vazios, force push ou publicar integrações quebradas.

## Primeira metade: 14 commits

A estimativa abaixo é **tempo de desenvolvimento + revisão + testes por entrega**, não um atraso obrigatório para publicar. Tarefas complexas têm faixas maiores porque exigir 20–40 minutos reduziria a qualidade.

| # | Estado | Mensagem sugerida | Entrega verificável | Esforço previsto |
|---|---|---|---|---|
| 01 | **Feito**: `e707c3c` | `fix(core): enforce fresh PCB invariants at gateway admission` | Invariantes de PCB novo centralizados no core; gateway não aceita campos preenchidos indevidamente; teste de regressão | 20–40 min |
| 02 | Pendente | `feat(memory): track per-request KV pages` | Tabelas independentes de páginas, mapeamento lógico→frame, alocação e liberação, testes de isolamento | 40–70 min |
| 03 | Pendente | `feat(runtime): model page-fault blocking and latency` | Estado BLOCKED, custo virtual, liberação do worker, evento de wakeup, progresso e snapshots consistentes | 60–110 min |
| 04 | Pendente | `feat(memory): handle capacity and fragmentation` | Pressão de memória, política de espera/evicção explicitada, fragmentação e casos extremos | 35–65 min |
| 05 | Pendente | `feat(events): provide deterministic traces` | Ordem estável de eventos, rastreabilidade e replay verificável sem reconstrução falsa de estados | 25–45 min |
| 06 | Pendente | `feat(workload): generate seeded scenarios` | Gerador com seed, múltiplas distribuições, formato versionado, validação e fixtures | 30–60 min |
| 07 | Pendente | `feat(metrics): add percentiles and utilization` | p50/p95, utilização, espera máxima, tempo bloqueado, preempções e dados para Gantt | 30–60 min |
| 08 | Pendente | `feat(results): export reproducible runs` | JSON/CSV com versão, entrada, configuração, seed, eventos e medidas | 25–50 min |
| 09 | Pendente | `feat(runtime): introduce pthread worker pool` | Threads reais, ciclo de vida, shutdown cooperativo, joins e testes independentes | 90–150 min |
| 10 | Pendente | `feat(sync): protect worker queues and resources` | Fila compartilhada, mutex, semáforos, cancelamento/cleanup e stress | 45–80 min |
| 11 | Pendente | `feat(runtime): connect pthreads to scheduling` | Integração de fila, política, workers, memória e contadores; invariantes sob carga | 90–150 min |
| 12 | Pendente | `feat(concurrency): isolate race demonstration` | Laboratório de corrida protegido/inseguro em processo isolado, timeout e evidência medida | 45–75 min |
| 13 | Pendente | `feat(deadlock): detect resource wait cycles` | Grafo de alocação/espera, detecção de ciclo, cenários controlados | 60–100 min |
| 14 | Pendente | `feat(deadlock): prevent and recover safely` | Ordem de aquisição, recuperação em laboratório, watchdog e testes de liveness | 60–100 min |

## Sessões de trabalho planejadas

| Bloco | Commits alvo | Objetivo | Estimativa do bloco |
|---|---|---|---|
| A | 01 (feito) | Endurecer contratos básicos e validação de entrada | 20–40 min |
| B | 02–04 | Memória e integração com BLOCKED/page fault | ~2h15–4h05 |
| C | 05–07 | Eventos, workloads e métricas | ~1h25–2h45 |
| D | 08–10 | Exportação e primeira base pthread/sync | ~2h40–4h40 |
| E | 11–14 | Integração POSIX, corrida, deadlock/detecção/prevenção | ~4h45–7h55 |

**Intervalos:** dentro de um bloco, 20–40 min pode ser razoável para tarefas pequenas; tarefas de maior risco só são concluídas quando validadas. Planejar pausas de aproximadamente **6 horas entre o término de um bloco e o início do próximo**. Horários exatos dependem do tempo efetivo de cada bloco e da disponibilidade de execução. Tarefas agendadas em horário fixo são **tentativas de continuar**, não garantia de quantidade de commits nem substituto da validação.

Se houver falha de teste, incompatibilidade de contrato, autorização ausente ou mudanças de colegas, parar/replanejar sem forçar escrita. Nunca recriar ou sobrescrever commits alheios.

## O restante da aplicação (15–28, ainda não agendado)

Conforme o [plano funcional](FUNCTIONAL_SPEC.md): IPC POSIX, SIGUSR1, SJF-EMA, treino offline e integração do preditor, geração/coleta com LLM, quatro experimentos, relatórios, decisão e adaptação da interface visual web/desktop, telas de escalonamento/memória/concorrência, testes ponta a ponta, CI e entrega final. Reestimar depois do commit 14 porque as escolhas de linguagem e interface ainda estão abertas.
