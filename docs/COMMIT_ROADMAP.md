# Roadmap de implementação por commits — OS-llm-project

> Planejamento revisável de 25/10/2026, baseado na `main` a partir do commit `140ff8a` e na especificação [FUNCTIONAL_SPEC.md](FUNCTIONAL_SPEC.md). Este documento **não cria commits futuros automaticamente**; tarefas agendadas somente tentam executar o próximo trabalho quando houver acesso e testes.  
> **Regra:** cada commit contém uma mudança coesa, testes relevantes e documentação do comportamento real. Não usar timestamps retroativos, commits vazios, force push ou publicar integrações quebradas.

## Primeira metade: 14 commits

A estimativa abaixo é **tempo de desenvolvimento + revisão + testes por entrega**, não um atraso obrigatório para publicar. Tarefas complexas têm faixas maiores porque exigir 20–40 minutos reduziria a qualidade.

| # | Estado | Mensagem sugerida | Entrega verificável | Esforço previsto |
|---|---|---|---|---|
| 01 | **Feito**: `e707c3c` | `fix(core): enforce fresh PCB invariants at gateway admission` | Invariantes de PCB novo centralizados no core; gateway não aceita campos preenchidos indevidamente; teste de regressão | 20–40 min |
| 02 | **Feito** (visões por requisição) | `feat(memory): expose per-request resident page mappings` | Consulta e snapshot de mapeamentos lógico→frame por requisição; eviction/liberação refletidas automaticamente, sem duplicar estado | 40–70 min |
| 03 | **Feito**: `c14495c` (I/O virtual) | `feat(runtime): model page-fault blocking and latency` | Estado BLOCKED, custo virtual, liberação do worker, evento de wakeup, progresso e snapshots consistentes | 60–110 min |
| 04 | **Feito**: `a4592ec` (pressão e fragmentação) | `feat(memory): handle capacity and fragmentation` | Pressão de memória, política de espera/evicção explicitada, fragmentação e casos extremos | 35–65 min |
| 05 | **Feito**: `f9747ae` (trace e replay) | `feat(events): provide deterministic traces` | Ordem estável de eventos, rastreabilidade e replay verificável sem reconstrução falsa de estados | 25–45 min |
| 06 | **Feito**: `cc60a67` (workload v1) | `feat(workload): generate seeded scenarios` | Gerador com seed, múltiplas distribuições, formato versionado, validação e fixtures | 30–60 min |
| 07 | **Feito**: `c628607` (métricas) | `feat(metrics): add percentiles and utilization` | p50/p95, utilização, espera máxima, tempo bloqueado, preempções e dados para Gantt | 30–60 min |
| 08 | **Feito**: `0a7a210` (exportação virtual) | `feat(results): export reproducible runs` | JSON/CSV com versão, entrada, configuração, seed, eventos e medidas | 25–50 min |
| 09 | **Feito**: `1523d7f` (ciclo de vida pthread isolado) | `feat(runtime): introduce pthread worker pool` | Threads reais, ciclo de vida, shutdown cooperativo, joins e testes independentes | 90–150 min |
| 10 | **Feito**: `a390a53` (fila POSIX independente) | `feat(sync): protect worker queues and resources` | Fila compartilhada, mutex, semáforos, cancelamento/cleanup e stress | 45–80 min |
| 11 | **Feito**: `1e122d2` (adaptador POSIX) | `feat(runtime): connect pthreads to scheduling` | Coordenador único para escalonador, cache e métricas; fila e pthreads confirmam tarefas de token com ACK exclusivo; traces idênticos ao virtual | 90–150 min |
| 12 | **Feito**: `ac6432f` | `feat(concurrency): isolate race demonstration` | Subprocesso fork/exec, watchdog 3s, safe/unsafe com lost update determinístico sem data race indefinida em C | 45–75 min |
| 13 | **Feito**: `ba86156` | `feat(deadlock): detect resource wait cycles` | Grafo didático de recursos exclusivos e detecção de ciclos com testemunha determinística | 60–100 min |
| 14 | **Feito (modelo)**: `021fd78` | `feat(deadlock): prevent and recover safely` | Aquisição não bloqueante em ordem global, recuperação por liberação da vítima, 1000 ciclos de liveness e watchdog CTest | 60–100 min |

Os commits 08–10 passaram pela matriz GCC Debug, Clang Debug e GCC ASan/UBSan na origem (**38/38** em cada configuração); os logs antigos não foram migrados. O commit 09 inicia threads reais e as encerra cooperativamente; o commit 10 cria fila protegida por mutex e semáforos. **A integração chegou no commit 11**, com um adaptador separado: o coordenador é dono exclusivo dos PCBs, scheduler, cache KV e métricas; pthreads processam confirmações de tarefas de token. Não há inferência real nem acesso paralelo ao cache.

Os commits 11–14 foram publicados em PRs separados na origem e aprovados em CI (39/39, 41/41, 42/42 e 43/43 testes, respectivamente). PRs e execuções de Actions da origem não foram migrados; consulte a [CI deste repositório](https://github.com/lboclin/Sistemas-Operacionais/actions). **Desvio de escopo:** o deadlock do commit 14 é um grafo didático, não um travamento real de mutexes pthread; o watchdog de liveness é o TIMEOUT do CTest. A integração de deadlock real entre recursos do runtime/memória e a interface visual permanecem pendentes.

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

## O restante da aplicação (15–28, em andamento)

**Commit 15 (F12 parcial, feito: `46736ae`):** transporte POSIX por pipe com framing e demo gateway/worker em processos reais. Dois testes novos: framing e round-trip. A integração IPC com o runtime principal permanece pendente. A suíte de 45 testes passou na matriz GCC, Clang e ASan/UBSan antes da migração; o PR da origem não foi migrado. Consulte a [CI deste repositório](https://github.com/lboclin/Sistemas-Operacionais/actions).

Próximos: integração IPC, SIGUSR1, SJF-EMA, treino e integração do preditor, coleta com LLM, experimentos, relatórios, decisão da GUI e telas de escalonamento/memória/concorrência, testes E2E e entrega final. Linguagem e interface ainda são decisões abertas.
