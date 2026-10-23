# LLM OS Lab — Plano 02: especificação funcional e alternativas técnicas

> **Estado:** especificação para desenvolvimento futuro; **não existe implementação validada**.  
> **Data:** 23/10/2026  
> **Documento complementar:** [`ARCHITECTURE.md`](ARCHITECTURE.md).  
> **Objetivo:** converter o enunciado acadêmico em funcionalidades isoladas, verificáveis e possíveis de dividir entre cinco pessoas.  
> **Escolhas em aberto:** C ou C++; GUI web ou desktop e stack; prioridade/combinação dos dois mecanismos de execução; pipe ou memória compartilhada para IPC.

## 1. Princípios funcionais

A aplicação modela requisições concorrentes de geração de texto como **processos lógicos com PCB**. Cada requisição disputa **workers/slots** e **blocos de memória de KV cache simulado**. O programa permite alterar políticas e observar seus efeitos sem exigir uma GPU real durante a simulação.

**Duas dimensões distintas:**

1. **O que se simula:** requisições, algoritmos, workers, memória, métricas e eventos.
2. **Como a execução avança:** por **relógio virtual determinístico** ou por **threads reais POSIX**. Esta escolha afeta relógio, reprodutibilidade, instrumentação e controles de UI, mas não deve duplicar as regras fundamentais de domínio.

**Atenção à proposta original:** ela descreve o pool de workers como **threads**, sincronização real, condição de corrida, IPC e sinais. Portanto, desenvolver apenas o simulador determinístico **não satisfaz integralmente** esses itens. O modo determinístico seria uma capacidade didática adicional; o laboratório com POSIX permanece no escopo de conclusão, mesmo que sua ordem de implementação fique para depois.

### 1.1. Vocabulário comum

| Termo | Significado no projeto |
|---|---|
| Requisição/PCB | processo **lógico** do simulador, não processo real do kernel |
| Worker lógico | slot que recebe trabalho dentro do simulador |
| Thread real | fluxo POSIX executado pelo Linux, apenas no modo concorrente |
| Token | unidade de progresso da requisição |
| Burst | trabalho estimado/restante da requisição em tokens ou custo equivalente |
| Quantum | máximo de trabalho executado antes de devolver o PCB à fila no RR |
| Bloco de KV | unidade fixa de alocação da memória **simulada** |
| Page fault | referência a bloco lógico ausente dos frames físicos simulados |
| Tempo virtual | coordenada temporal da simulação, não duração real do computador |
| Wall time | tempo observado pelo SO, usado em experiências concorrentes |
| Run | uma execução imutável com configuração, carga e resultados rastreáveis |

## 2. Decisão futura mais importante: duas opções de execução

As alternativas abaixo não se excluem necessariamente. **A decisão futura é se o projeto implementará os dois mecanismos, em que sequência e quais características cada um exporá**, preservando os requisitos POSIX da disciplina.

### Opção A — Execução determinística com relógio virtual

**Ideia:** o programa mantém uma lista ou fila de eventos (`chegada`, `dispatch`, `token gerado`, `fim do quantum`, `page fault`, `liberação de memória`, `conclusão`). Um relógio lógico avança para o próximo evento. A mesma entrada, configuração, seed e ordenação de empates geram o mesmo trace.

**Exemplo:** três solicitações chegam em `t=0`, `t=0` e `t=10`, com 100, 30 e 60 tokens. Um worker virtual FCFS com custo de `1 ms virtual/token` executa A em `[0,100)`, B em `[100,130)` e C em `[130,190)`. Não são necessários 190 ms reais para simular.

**Como funcionaria:**

1. Gateway carrega chegadas no relógio virtual.
2. Core transforma a chegada em PCB `NEW` → `READY`.
3. Escalonador escolhe um PCB e runtime reserva um worker lógico.
4. Execução gera progresso e eventos com timestamps simulados.
5. Memória aloca ou bloqueia a operação conforme disponibilidade.
6. Métricas são reconstruídas dos eventos e snapshots.

**Vantagens:** cenário reproduzível, stepping, replay por log/snapshot, comparação justa de algoritmos, gráficos e Gantt simples, testes de regressão precisos, possibilidade de acelerar simulação.

**Limitações:** mutex e corrida só são **modelados**; não comprova uso efetivo de pthreads; tempos modelados não são desempenho real; um deadlock calculado no modelo não é um deadlock de threads reais. Para laboratório de SO, isso precisa ser indicado na UI.

**Condições necessárias:** determinismo de desempates, PRNG com seed, definição exata de ordem de eventos de mesmo timestamp, modelo de custo documentado, uso separado de `simulation_time` e `wall_time`, reprodução de runs.

**Controles UI viáveis:** iniciar, pausar, avançar 1 evento, avançar até próximo token/quantum, controlar velocidade da animação, reiniciar e reproduzir evento; retroceder somente se houver log/snapshots que sustentem replay (não prometer undo universal).

**Risco:** o projeto parecer um animador abstrato e não demonstrar o conteúdo POSIX. Mitigação: incluir laboratório de threads reais ao completar a disciplina.

### Opção B — Execução concorrente com threads POSIX reais

**Ideia:** o programa cria um pool real de workers (p.ex. `pthread`). Threads consomem requisições compartilhadas, simulam produção de tokens e coordenam acesso a recursos com mutex e semáforos. O kernel Linux decide quando cada thread roda.

**Exemplo:** quatro workers disputam uma fila com dez PCBs. Sem mutex, duas threads podem retirar ou alterar a mesma estrutura incorretamente; com sincronização adequada, a fila e a memória preservam invariantes.

**Como funcionaria:**

1. Gateway real ou componente de entrada coloca trabalho na fila protegida.
2. Worker retira PCB após sincronizar acesso.
3. Worker progride até término ou quantum máximo e devolve PCB se necessário.
4. `memory` reserva frames protegidos e comunica bloqueios/desbloqueios.
5. Gerenciador detecta conflitos de recursos e instrumenta espera.
6. Encerramento faz `join` dos workers e fecha canais/recursos corretamente.

**Vantagens:** demonstra pthreads, mutexes, semáforos, corrida, espera, sinais e recursos POSIX **de verdade**; conecta diretamente aos assuntos da disciplina.

**Limitações:** ordem e latências reais podem variar entre runs; pausar/avançar arbitrariamente uma thread real é mais complexo; não usar `pthread_cancel` indiscriminadamente; race e deadlock precisam de laboratório isolado e watchdog; não confundir latência de sleep/lock com custo de inferência de LLM.

**Controles UI viáveis:** iniciar, solicitar pausa cooperativa em ponto seguro, retomar, interromper com shutdown ordenado, snapshot consistente e acompanhamento dos workers. `step()` de evento **não é garantido** em execução concorrente real: pode ser indisponível ou definido como avanço **cooperativo** entre checkpoints.

**Risco:** resultados de comparação de algoritmos serem contaminados por escalonamento do SO e latência de sincronização. Mitigação: distinguir explicitamente métricas de tempo virtual e wall time; repetir runs e relatar máquina/compilação.

### Comparação para tomada de decisão

| Critério | A: relógio virtual | B: POSIX real |
|---|---|---|
| Threads reais | Não | Sim |
| Pausa e stepping | Natural | Limitado a checkpoints |
| FCFS / SJF / RR | Muito reproduzível | sujeito à concorrência |
| Paginação FIFO/LRU/Ótimo | Facilidade de isolar eventos | possível, com sincronização adicional |
| Race condition | somente ilustração | fenômeno real |
| Mutex e semáforos | representados no modelo | mecanismos reais |
| Deadlock | grafo de espera simulado | reprodução controlada entre threads |
| Tempo | virtual | real ou misto, conforme experimento |
| Repetibilidade de traces | alta | limitada |
| Esforço de teste | menor | maior |
| Valor para a disciplina | bom em escalonamento/memória | indispensável para threads/IPC/sync |

**Caminho de implementação possível, ainda não aprovado:** definir domínio/contratos comuns → motor determinístico para comparar algoritmos → worker pool real para demonstrar concorrência → integrar ambos à mesma interface e aos relatórios. **Alternativa de prioridade:** implementar o modo pthread primeiro para atender aos requisitos explícitos da disciplina e só depois introduzir replay determinístico. A arquitetura do Plano 01 suporta as duas sequências.

### Critérios de escolha futura

1. Prazo e rubrica da disciplina: o professor exige execução POSIX já na primeira entrega?
2. Reprodutibilidade da avaliação: a comparação de políticas precisa de resultados idênticos?
3. Capacidade desejada da interface: `step` e replay são prioridades ou apenas visibilidade de threads?
4. Custo de integração: a equipe tem tempo para dois runtimes ou fará uma implementação central com laboratório separado?
5. Possibilidade de modularidade: um único PCB/event log pode alimentar ambos?

**Decisão suspensa. Não iniciar a implementação do runtime antes de fechar os contratos comuns.**

## 3. Catálogo de funcionalidades e critérios de aceite

Os identificadores (`F01`, `F02`, ...) servem para criar issues e rastrear requisitos. A coluna "base" distingue **proposta original** de **melhoria proposta**.

| ID | Módulo | Funcionalidade | Base |
|---|---|---|---|
| F01 | Core | PCB e máquina de estados | original |
| F02 | Gateway | admissão/entrada de requisições | original |
| F03 | Scheduling | FCFS | original |
| F04 | Scheduling | SJF e versões comparativas | original |
| F05 | Scheduling | Round Robin em tokens | original |
| F06 | Scheduling | prioridade com aging | original |
| F07 | Runtime | pool de workers e quantums | original |
| F08 | Concurrency | mutexes, semáforos, race demo | original |
| F09 | Concurrency | deadlock, detecção e prevenção | original |
| F10 | Memory | KV cache paginado e tabelas | original |
| F11 | Memory | substituição FIFO, LRU e Ótimo | original |
| F12 | IPC | pipe ou memória compartilhada POSIX | original |
| F13 | Signals | SIGUSR1 e dump seguro | original |
| F14 | Prediction | estimador de comprimento de resposta | original |
| F15 | Workload | coleta com LLM pequeno + geração de cargas | original |
| F16 | Metrics | latências, throughput, waiting, faults etc. | original |
| F17 | Experiments | quatro conjuntos de experimentos | original |
| F18 | UI | laboratório gráfico interativo | melhoria solicitada |
| F19 | Runtime | relógio virtual/execução determinística | ampliação proposta |
| F20 | Quality | testes, fixtures, exportação e reprodutibilidade | viabilização técnica |

### F01 — PCB e estados de requisição

**Responsabilidade:** `src/core`. **Entrada:** solicitação validada, instante de chegada, parâmetros de job. **Saída:** ID, objeto lógico e eventos de transição.

- Campos: ID único, instante de chegada, prioridade, tokens de prompt e resposta alvo, contador gerado, tamanho previsto, estados, worker, blocos possuídos e tempos de ciclo.
- Estados originais: `NEW`, `READY`, `RUNNING`, `BLOCKED`, `TERMINATED`.
- Transições esperadas: `NEW→READY`, `READY→RUNNING`, `RUNNING→READY` (preempção), `RUNNING→BLOCKED` (recurso indisponível), `BLOCKED→READY` (liberação), `RUNNING→TERMINATED`. Transições extraordinárias devem ser discutidas separadamente.
- Invariantes: IDs não duplicados; `0 ≤ generated_tokens ≤ target_output_tokens`; requisição terminada nunca volta à fila; execução exclusiva por no máximo um worker por PCB.

**Aceite:** testes cobrem cada transição válida e rejeitam uma inválida; mudanças emitem `STATE_CHANGED` com instante e motivo; nenhum algoritmo altera o PCB por acesso externo à sua API.

### F02 — Gateway e chegada/admissão

**Responsabilidade:** `src/gateway`. Recebe requisições criadas manualmente ou por cenário, valida limites e agenda as chegadas. Deve distinguir **chegada** de **admissão**: memória/slots podem impedir admissão imediata dependendo da política escolhida.

**Casos:** chegada simultânea, entrada vazia, lotes, prioridade inválida, tokens negativos, ID duplicado e carga acima da capacidade.

**Aceite:** entrada com mesmos dados e mesma ordem/seed produz mesma sequência de admissão no modo determinístico; erro de validação não corrompe filas.

### F03 — FCFS

**Responsabilidade:** `src/scheduling`. Política não preemptiva por definição de comparação básica: ordenar por instante de entrada na fila pronta; empate por ID/`admission_sequence` documentado.

**Aceite:** com um worker e A(4), B(2), C(3) chegando juntos, Gantt esperado `A[0,4)`, `B[4,6)`, `C[6,9)` em custo unitário por token; tempos de espera calculados corretamente.

### F04 — SJF e três variantes comparativas

**Responsabilidade:** `src/scheduling` + interface `src/prediction`.

**Variantes exigidas na proposta:**

1. **SJF + média exponencial** de bursts passados (`tau_next = α·last_burst + (1−α)·tau_previous`), com inicialização e escopo de histórico documentados;
2. **SJF + preditor de IA** com tamanho previsto por características de entrada;
3. **SJF + oráculo** com duração verdadeira fornecida pelo cenário (somente baseline experimental).

**Baseline adicional:** FCFS. Quando o relatório disser "SJF ideal", esclarecer o âmbito em que é ótimo: minimiza espera média em um conjunto de tarefas todas disponíveis e bursts conhecidos, sem assumir que seja ótimo globalmente para chegadas arbitrárias e múltiplos workers.

**Escolhas em aberto:** SJF estritamente não preemptivo vs. decisão por quantum; se versão preemptiva for adicionada, identificá-la como **SRTF** e compará-la separadamente. Não usar o tamanho verdadeiro fora do oráculo (evitar vazamento).

**Aceite:** cenário de burst curto após longo discrimina políticas; preditor ausente gera fallback documentado; estimativas negativas e dados faltantes são rejeitados ou tratados por regra explícita.

### F05 — Round Robin

**Responsabilidade:** `src/scheduling` e contrato com `runtime`.

- Quantum **medido em tokens**, não milissegundos de CPU.
- Após produzir até `K` tokens, se incompleta, PCB `RUNNING→READY` e retorna à fila.
- Se faltar memória durante o quantum, vai a `BLOCKED`; somente tokens gerados contam.
- Chegadas durante a execução entram na fila, respeitando regra de desempate.

**Aceite:** A(5), B(3), `K=2`, um worker, chegada conjunta: fatias esperadas `A2, B2, A2, B1, A1` (na ausência de custo extra e bloqueio); cada requisição termina exatamente na contagem alvo.

### F06 — Prioridade com aging

**Responsabilidade:** `src/scheduling`.

- Prioridade de chegada configurável, com sentido documentado (maior valor = maior prioridade, ou inverso).
- **Aging** eleva prioridade efetiva de requisição pronta que aguarda, sem alterar prioridade original irreversivelmente.
- Considerar empate estável e opção de preempção explicitamente descrita.
- Instrumentar espera longa/starvation.

**Aceite:** requisição de baixa prioridade não fica indefinidamente atrás de novas requisições prioritárias no cenário de teste configurado; `aging=0` produz baseline sem aging.

### F07 — Pool de workers e execução

**Responsabilidade:** `src/runtime`.

- Workers configuráveis; um PCB não pode ocupar dois slots simultaneamente.
- Registrar início, duração e término de fatias de execução.
- Emitir progresso em tokens, yield por quantum e conclusão.
- Tratar `BLOCKED`/`READY` e shutdown cooperativo.
- **Runtime A:** trabalhadores virtuais avançados por eventos. **Runtime B:** pool POSIX real, bloqueio condicional e `join` no término.

**Aceite:** nenhum worker executa duas requisições ao mesmo tempo; nenhum PCB executa em dois workers; `completed + active + queued + blocked` conserva o número total admitido (com adaptação para estados novos/erros); encerramento não deixa threads ativas.

### F08 — Mutex, semáforos e condição de corrida

**Responsabilidade:** `src/concurrency`.

**Dois submodos didáticos**:

- **Seguro:** acesso à fila, contadores e alocador protegidos com primitivas POSIX apropriadas; semáforos para disponibilidade de itens/slots quando fizer sentido, mutex para exclusão mútua.
- **Demonstração insegura:** laboratório pequeno, isolado do simulador principal, executa um contador/estrutura intencionalmente sem proteção, em processo de teste com timeout. Não lançar a aplicação principal com sincronização desligada de forma global.

**Aceite:** modo seguro mantém invariantes sob carga; demonstração registra valor esperado e observado, **sem exigir que uma race apareça em toda run**. Stress e sanitizers podem reforçar evidências, com limitações documentadas.

### F09 — Deadlock entre memória e slots

**Responsabilidade:** `src/concurrency`, integrado ao `memory` e `runtime` por contratos.

- Cenário pedagógico: A mantém memória e espera worker; B mantém worker e espera memória; ambos entram em espera circular.
- Instrumentar grafo de alocação/espera; detector identifica ciclo ou mecanismo equivalente adequado aos tipos de recurso.
- **Prevenção:** ordem global de aquisição (por exemplo sempre memória antes de slot, ou vice-versa), considerando política de reserva e comportamento de bloqueio.
- **Tratamento:** em laboratório, abortar/reverter uma requisição ou liberar recursos sob regra documentada; cenário não pode travar aplicação inteira.

**Aceite:** detector encontra o deadlock esperado no fixture, a prevenção elimina a condição circular no mesmo fixture e o laboratório encerra dentro do timeout. Simulação de ciclo não prova deadlock de pthreads; diferenciar as evidências.

### F10 — Gerenciador do KV cache paginado

**Responsabilidade:** `src/memory`.

- Memória dividida em **blocos/frames de tamanho fixo**; cada requisição referencia páginas lógicas por uma tabela.
- Alocar sob demanda à medida que tokens são produzidos, usando configuração explícita de tokens por bloco (modelo simplificado).
- Alocação, liberação, compartilhamento (se futuramente houver), faults, eviction, reuso e fragmentação interna são eventos separados.
- Se não existir frame disponível, política decide bloquear, evictar ou falhar segundo regra configurada.
- Uma página removida e acessada novamente exige modelo definido de **recomposição/custo** ou armazenamento secundário fictício. Não criar cache inexistente e afirmar que o KV real se recompõe sem custo.

**Aceite:** nunca ultrapassar capacidade de frames; múltiplos PCBs têm tabelas independentes; memória de requisição terminada é liberada; contagem de fragmentação considera espaço desperdiçado no último bloco de cada conjunto alocado.

### F11 — FIFO, LRU e Ótimo (OPT)

**Responsabilidade:** `src/memory`.

- **FIFO:** retira página há mais tempo residente; não confundir com idade do PCB.
- **LRU:** retira a menos recentemente acessada; atualizar uso apenas em referências reais do modelo.
- **Ótimo (OPT):** elimina página cujo próximo acesso está mais distante no futuro; requer **trace de referências futuro conhecido**. É um oráculo para comparação offline e **não é política online realista**.
- Política global ou local por requisição é **decisão pendente**; marcar nos relatórios.

**Aceite:** sequências de referência pequenas com quantidade conhecida de faltas de página para cada política; OPT nunca consulta futuro em uma run que é rotulada "online"; ausência de trace completo desabilita OPT com mensagem explicativa.

### F12 — IPC: gateway e workers

**Responsabilidade:** `src/ipc`.

A proposta pede **pipe ou memória compartilhada POSIX** entre gateway e workers, a definir. **Atenção:** threads do mesmo processo já compartilham memória; usar `pipe` ou memória compartilhada POSIX será pedagogicamente mais claro com **ao menos dois processos reais** em uma demonstração separada, por exemplo `gateway_process` ↔ `engine_process`.

Alternativas a decidir:

- **Pipe POSIX:** canal orientado a bytes, mensagem enquadrada (comprimento + payload), fechamento/EOF e tratamento de leituras parciais.
- **Memória compartilhada POSIX:** região mapeada (`shm_open`/`mmap`) e sincronização entre processos; requer coordenação do ciclo de vida e limpeza.
- **Ambas:** se o tempo permitir e a rubrica premiar comparação, implementar como duas estratégias de transporte; não obrigatório pela proposta.

**Aceite:** troca de mensagens em cenário controlado, tratamento de fim de processo e limpeza completa; sem deadlocks na finalização.

**Não confundir:** IPC didático entre processos do motor é diferente da comunicação futura **interface gráfica ↔ aplicação** (HTTP local, FFI ou IPC local); esta última será escolhida junto da GUI.

### F13 — Depuração com SIGUSR1

**Responsabilidade:** `src/ipc` ou um pequeno componente `src/signals` caso justificado na implementação.

A proposta: ao apertar uma tecla, o programa envia `SIGUSR1` para si e imprime estado de filas e memória **fora do handler**.

- Handler assíncrono, se usado, faz no máximo operação async-signal-safe (ex.: setar um flag `volatile sig_atomic_t`) e retorna; **nunca** usa `printf`, mutex, alocação ou serialização de snapshot.
- Alternativa robusta no Linux/POSIX: bloquear SIGUSR1 nas threads relevantes e ter thread dedicada com `sigwait`/mecanismo equivalente, sinalizando ao controlador para capturar um snapshot seguro.
- A GUI futura oferece um botão para a mesma funcionalidade sem exigir tecla de console; manter o exemplo de sinal demonstrável separadamente.

**Aceite:** envio de SIGUSR1 não encerra processo, não provoca deadlock e produz dump consistente fora do contexto de sinal.

### F14 — Preditor de tamanho da resposta

**Responsabilidade:** `src/prediction` (uso) e `tools/predictor` (treino).

- Treinar offline regressão, gradient boosting ou baseline equivalente para estimar número de tokens da saída.
- Inputs possíveis: tamanho do prompt, limites de geração, categoria de tarefa e metadados não sensíveis.
- Exportar previsões versionadas/indexadas por `request_id`; o motor não exige instalar dependências de treino.
- Avaliação sem vazamento entre treino e teste: MAE/RMSE e comparação downstream de espera média, retorno e p95.
- Modo sem IA funciona com FCFS, RR, prioridade e SJF de média exponencial/oráculo para experimento.

**Aceite:** previsões válidas são carregadas; faltantes seguem fallback configurado; erro de predição é registrado; teste de qualidade informa split e seed; não alegar ganho sem medir.

### F15 — Carga de trabalho realista

**Responsabilidade:** `src/workload`, `tools/workload_capture`.

- Gerar datasets sintéticos com distribuições controladas: chegada, prompt tokens, output tokens, prioridade, burst, perfil de recursos.
- Opcionalmente coletar **distribuições** de um LLM pequeno local (ex.: `llama.cpp` ou Ollama), conforme a proposta, sem realizar inferência durante todos os benchmarks.
- Arquivos de cenário contêm seed, proveniência, configuração e identificadores; usar dados sintéticos ou prompts tratados antes de versionar.
- Conjuntos: curto, longo, rajadas, mem-pressure, alta concorrência, prioridades, casos de erro.

**Aceite:** uma carga com seed reproduz dados em ambiente compatível; dois algoritmos recebem exatamente mesma lista de requisições; dados de avaliação não são usados para treinar o preditor.

### F16 — Métricas, eventos, Gantt e relatórios

**Responsabilidade:** `src/metrics`.

Cada run precisa guardar **modo**, relógio, custo por token, política, tamanho da memória, quantidade de workers, seed, versão do código, parâmetros relevantes, timestamps e ambiente (quando wall time).

**Definições sugeridas a aprovar:**

| Métrica | Fórmula / interpretação |
|---|---|
| Espera (`waiting`) | tempo total em `READY`, soma dos intervalos; não confundir com `BLOCKED` |
| Retorno (`turnaround`) | `finish_time − arrival_time` |
| Resposta (`response`) | `first_dispatch_time − arrival_time` ou primeiro token (TTFT); **escolher e nomear**, sem misturar |
| Vazão | requisições concluídas / duração total da run no relógio especificado |
| Utilização | tempo em execução / (workers × intervalo disponível) |
| Starvation | maior espera em fila + contagem acima de limiar declarado |
| Page faults | referências a páginas ausentes |
| Fragmentação interna | capacidade alocada sem dados úteis, sob regra de blocos escolhida |
| Preempções | quantums encerrados antes do término |
| Latência p50/p95 | percentis do conjunto de requisições concluídas; explicitar método de interpolação |

**Cuidados:** zero requisições concluídas não permite throughput significativo; workflows bloqueados não devem desaparecer de estatísticas; clock de parede não é comparável com clock virtual; custo de fault e de troca de contexto deve ser configurável e explicitado.

**Aceite:** métricas são dedutíveis de logs pequenos, sem valores hardcoded; exportação anexa metadata; Gantt diferencia RUNNING, READY e BLOCKED; gráficos e tabelas usam o mesmo conjunto de runs selecionado.

### F17 — Experimentos científicos da proposta

**Responsabilidade:** `src/experiments` e `tools/analysis`.

**E1 — Corrida:** variar número de requisições/threads; executar com e sem sincronização **somente no laboratório isolado**; comparar valor esperado com valor observado e ocorrências de erro, repetindo várias vezes.

**E2 — Escalonamento:** mesma carga e parâmetros para FCFS, SJF média exponencial, SJF preditor, SJF oráculo, RR com múltiplos `K`, prioridade com/sem aging. Medir espera/retorno/resposta p50/p95, throughput e starvation. Separar custos virtuais do modo POSIX.

**E3 — Memória:** variar frames/tamanho de bloco e FIFO/LRU/OPT no **mesmo trace de referências**. Contar faults, substituições, bloqueios e fragmentação. Ótimo somente quando há trace completo.

**E4 — Deadlock:** demonstrar condição circular, detectá-la, e repetir sob prevenção por ordem global; registrar grafo/trace e ação de recuperação se implementada.

**Aceite global:** configuração e seed por run, múltiplas repetições quando há aleatoriedade, nunca comparação de políticas com cargas diferentes ou métricas misturadas, tabelas geráveis sem edição manual.

### F18 — Interface gráfica do laboratório (framework adiado)

**Responsabilidade:** `apps/visual`.

A interface será uma aplicação **visual gráfica**, não TUI. O usuário escolhe cenário/política e inspeciona execução. Forma de distribuição **web local** ou **aplicativo desktop** ainda aberta.

#### Telas e ações

| Tela | Controles | Visualização | Etapa |
|---|---|---|---|
| Dashboard | carregar/inserir requisições, escolher política, iniciar/pausar | fila, workers, KPIs básicos | MVP |
| Requisições | parâmetros de prompt/output/prioridade/chegada; validação | cards/tabela com PCB e estados | MVP |
| Escalonamento | FCFS/SJF/RR/aging, quantum, presets | Gantt interativo, preempções, espera | MVP → completa |
| Memória | capacidade, tamanho de bloco, FIFO/LRU/OPT | frames, tabela, faults, alocação | base → completa |
| Concorrência | modo seguro/demonstrativo, número workers | threads, locks, erros, grafo de espera | após lab POSIX |
| Experimentos | escolher matriz, repetições e parâmetros | status de runs, comparação lado a lado | após métricas |
| Resultados | filtros por run e métrica, exportar | gráficos, resumo, tabelas | após métricas |

#### Experiência de usuário

- Priorizar **tarefa principal**: inserir requisição → escolher política → iniciar → ver fila progredindo → inspecionar resultado.
- **Feedback visível:** por que uma requisição está bloqueada, qual worker a possui, qual algoritmo foi usado, razão de erro.
- **Controles por capacidade:** `step` pode estar disponível só no modo determinístico; nunca exibir botão ativo se o runtime não suportar.
- **Estado vazio:** mensagem e cenário de exemplo, sem dashboard preenchido com métricas falsas.
- **Legibilidade:** estados identificados por texto/ícone **e** cor; Gantt com zoom e legenda; navegação por teclado quando o toolkit permitir.
- **Segurança:** demos de race/deadlock exigem identificação clara de "laboratório controlado" e não afetam uma run normal.
- **Performance da UI:** separar eventos detalhados de snapshots periódicos; renderizar visualização agregada quando houver milhares de eventos.
- **Exportação:** run com metadados e configurações completos; interface não altera resultado exportado.

#### Critérios de aceite de interface MVP

1. Abrir uma sessão nova e identificar seu modo.
2. Criar três requisições, inclusive uma inválida e receber erro útil.
3. Escolher FCFS ou RR, iniciar uma run e ver mudanças de estado coerentes.
4. Pausar/avançar **apenas se suportado**; controles indisponíveis aparecem explicados.
5. Ver PCBs e workers sem inconsistência entre painéis (snapshot único por atualização).
6. Consultar resultados e executar novamente o mesmo cenário.

### F19 — Execução por eventos e replay (ampliação proposta)

**Responsabilidade:** `src/runtime`.

- Eventos têm ordem total dentro da run, inclusive empates de timestamp.
- O mecanismo pode avançar para próximo evento sem `sleep`, e UI controla velocidade de reprodução **somente da animação**.
- Replay exige log e estado inicial; snapshots periódicos podem otimizar reconstrução, mas não duplicam o estado autoritativo.
- **Aceite:** duas runs com configuração/seed iguais geram mesma sequência canônica de eventos; alterar a velocidade visual não altera métricas simuladas.

### F20 — Testabilidade e rastreabilidade

**Responsabilidade transversal**, com integração em `tests/`, `scenarios/` e `docs/`.

- Testes pequenos por função, testes de contrato por fronteira, cenários integrados por política e memória, testes POSIX separados.
- Falhas de concorrência deliberadas nunca impedem o CI de terminar: timeout e isolamento de processo.
- Artefatos identificam versão, entrada, configurações, seed, relógio e resultados.
- **Aceite:** uma pessoa nova consegue executar um cenário de referência, obter saída e saber se passou/fracassou sem ver código-fonte.

## 4. Roteiros funcionais completos

### Fluxo A — Execução simples

1. Usuário entra no dashboard e cria ou importa cenário.
2. Define política, parâmetros de memória, número de workers e modo da run (quando habilitado).
3. Aplicação valida e congela a configuração da run.
4. Requisições chegam, são admitidas, selecionadas, executadas, bloqueadas ou finalizadas.
5. Motor publica eventos; interface consome snapshots e desenha Gantt/memória.
6. Ao encerrar, experimentos e métricas recebem a run **imutável**.
7. Usuário pode comparar com outra política carregando o mesmo cenário.

### Fluxo B — Falta de memória e substituição

1. PCB executa geração que requer novo bloco KV.
2. `memory` verifica se há frame livre.
3. Se não houver, aplica política permitida, produz `PAGE_FAULT` e possivelmente `PAGE_EVICTED`.
4. Se substituição não liberar recurso adequado, PCB fica `BLOCKED` com motivo e condição de retomada.
5. Ao liberar memória, uma requisição bloqueada é reavaliada e volta a `READY` sem ocupar worker indevidamente.
6. O evento é observável na UI; custos são adicionados conforme modelo configurado.

### Fluxo C — Race controlada

1. Usuário abre laboratório de concorrência e escolhe `safe` ou `unsafe`.
2. Aplicação executa processo auxiliar com timeout e entrada fixa.
3. Threads incrementam contador/consomem fila; execução registra valor esperado e observado.
4. Modo seguro afirma invariantes; modo inseguro relata o que **aconteceu naquela run**, sem inventar race.
5. Execução encerra e mostra diferença, limites e reexecuções.

### Fluxo D — Comparação do SJF

1. Gerador produz cenário e split independente para predição.
2. Estimadores geram tamanhos previstos sem acessar saída verdadeira das amostras de avaliação.
3. Runner aplica mesma carga a FCFS, SJF exponencial, SJF preditor e SJF oráculo.
4. Cada política tem resultados separados, calculados com mesmo modelo de tempo.
5. Relatório mostra diferença em espera, p95, throughput e erro de previsão.

## 5. Matriz de requisitos e testes mínimos

| ID | Critério obrigatório | Tipo de teste preferencial |
|---|---|---|
| F01 | estados válidos e PCB consistente | unidade + propriedades |
| F02 | entradas válidas/inválidas + desempate | unidade + integração |
| F03 | FCFS em carga pequena conhecida | fixture golden |
| F04 | variantes SJF e isolamento do oráculo | unidade + experimento |
| F05 | quantum em tokens e ordem RR | fixture golden |
| F06 | aging reduz starvation no cenário | unidade + experimento |
| F07 | worker exclusivo e shutdown | integração + POSIX |
| F08 | integridade no modo seguro, race isolada | stress + laboratório |
| F09 | detectar/prevenir ciclo e timeout | unit + laboratório isolado |
| F10 | limites de memória/page table/fragmentação | unidade + integração |
| F11 | FIFO/LRU/OPT sobre mesmo trace | golden |
| F12 | mensagem completa, EOF, limpeza | integração de processos |
| F13 | SIGUSR1 solicita dump fora do handler | teste de integração POSIX |
| F14 | sem vazamento + fallback | contrato + avaliação offline |
| F15 | geração por seed e proveniência | unidade + integração |
| F16 | tempos, percentis, vazão e faults | testes matemáticos golden |
| F17 | quatro experimentos reexecutáveis | sistema + relatório |
| F18 | fluxo GUI de criação→run→resultado | E2E, após escolher stack |
| F19 | replay e tempo virtual coerentes | golden/event trace |
| F20 | regressões e rastreabilidade | CI/validação de artefatos |

## 6. Dependências para execução em paralelo

| Entrega dependente | Precisa primeiro de | Pode começar antes com |
|---|---|---|
| Interface GUI | comandos/snapshots/eventos | mock de snapshot e wireframes |
| Escalonadores | PCB e visão de fila | fixtures com `RequestView` falso |
| Memória | contrato de reserva e ID do PCB | testes de blocos isolados |
| Runtime A/B | comandos do motor e eventos | execução de token com fake memory |
| IPC e sinais | ciclo de vida da aplicação | laboratório POSIX independente |
| Preditor de IA | schema do cenário, IDs e métricas | dados offline em CSV/JSON |
| Experimentos | Runner + métricas + políticas | fixtures sintetizadas e expectativa golden |

**Congelar primeiro:** IDs, unidade dos relógios, representação de burst e token, estados, erros, eventos, `Scenario`, `RunResult`, `snapshot` e capacidades de controle `pause/step` por modo.

## 7. Ordem sugerida de desenvolvimento (não é a decisão sobre runtime)

| Etapa | Entrega | Critério de aprovação |
|---|---|---|
| P0 | contratos, 2 cenários de exemplo, wireframes GUI | casos de uso e dados consistentes |
| P1 | core + gateway + eventos + FCFS | run simples com resultados golden |
| P2 | RR, SJF, prioridades/aging e métricas | comparação de algoritmos sobre mesma carga |
| P3 | paginação, 3 políticas e integração | page faults + invariantes reproduzidos |
| P4 | worker pool POSIX, mutex/semaforo, race, deadlock, IPC, sinal | demonstrações reais isoladas passam |
| P5 | GUI inicial conectada a comandos/snapshots | fluxo criar→executar→visualizar |
| P6 | coleta com LLM pequeno, preditor e 4 experimentos | matrizes + relatórios reproduzíveis |
| P7 | observabilidade/replay refinado, regressão e apresentação | documentação e demos estáveis |

**Nota:** P4 pode ser antecipada e P5 pode começar com mocks desde P0. A decisão A/B é *qual runtime priorizar e qual será núcleo principal dos experimentos*, não se threads POSIX podem ser ignoradas.

## 8. Outras decisões a manter em aberto

### C11 ou C++20 para o motor

A proposta **original prevê C**. Usar C++ exige confirmar permissão do professor. A arquitetura permite ambos, mas os contratos de código serão diferentes:

- **C11 + pthread:** estruturas e funções públicas, controle explícito de memória, ótima aderência didática ao POSIX; maior disciplina manual para ownership e limpeza.
- **C++20 + POSIX:** tipos, RAII, containers e interfaces facilitam manutenção e composição; manter POSIX para demonstrar explicitamente pthread/mutex/semaforos/IPC quando obrigatório. **Não substituir conceitos POSIX pedidos por wrappers sem explicar**.
- **Python:** indicado como possibilidade para IA/análise e eventual backend/UI, mas não deve ser assumido como substituto do núcleo enquanto a proposta e rubrica forem desconhecidas.

**Critério futuro:** permissões da disciplina, facilidade de integrar GUI, segurança de memória, prazo do grupo, bibliotecas de teste, clareza da demonstração de SO.

### GUI web local ou desktop

- **Web local:** frontend no navegador, engine em processo separado; facilita interface rica e visualização, mas exige protocolo e empacotamento de servidor local.
- **Desktop:** janela executável nativa, integração direta ou local com motor; pode simplificar distribuição offline, mas depende do toolkit.

**Ainda não escolher** React/TypeScript, Qt, Electron, JavaFX, Python etc.; primeiro validar wireframes e jornada.

### Custo de token e de page fault

- **Simples:** `cost = tokens × custo_constante`, com overhead explícito de fault/preempção. Facilita aula e validação.
- **Calibrado:** distribuições observadas em um LLM pequeno alimentam tempos/tamanhos, sem que a GPU execute a simulação completa. Facilita realismo, mas exige controlar variância e proveniência.

### Dinâmica de memória

- Bloqueio versus eviction, escopo local/global da substituição, custo de recomputação de KV, número de tokens por bloco e políticas de liberação exigem decisões explícitas antes de experimentar. Evitar converter o KV cache simulado em uma afirmação falsa sobre implementação interna do vLLM.

## 9. Perguntas objetivas a levar para a reunião do grupo

1. O professor exige **C** ou permite C++?
2. O uso de `pthread`, semáforos, pipes e SIGUSR1 deve acontecer no executável principal ou um laboratório demonstrável é aceito?
3. A nota depende de **execução real** ou de **experimentos reprodutíveis de escalonamento**? Ambos estão previstos, mas a ordem muda.
4. Interface web local ou desktop é mais conveniente para a apresentação e o laboratório?
5. Qual é o prazo/escopo exigido para a primeira demonstração?

## 10. Condições de encerramento do projeto

- [ ] os cinco estados e a disciplina de PCB estão demonstrados;
- [ ] FCFS, SJF (incluindo 3 variantes), RR e prioridade com aging funcionam;
- [ ] trabalhadores, preempção em tokens, bloqueios e conclusão estão corretos;
- [ ] condição de corrida, mutex e semáforos demonstrados em POSIX real;
- [ ] deadlock reproduzido/detectado/prevenido de forma segura;
- [ ] KV cache paginado, tabela de páginas, FIFO/LRU/OPT e fragmentação calculados;
- [ ] IPC didático e SIGUSR1 funcionando conforme proposta;
- [ ] workload realista e preditor comparado a baselines de SJF;
- [ ] quatro experimentos com métricas, gráficos e relato de ambiente;
- [ ] interface gráfica permite operar e observar cenário sem terminal;
- [ ] testes e documentação tornam a demonstração reproduzível.

## Base e limites

- **Exigências:** derivadas do documento da disciplina *LLM-Sched: um mini sistema operacional para servir requisições de LLM*.
- **Ampliações propostas aqui:** runtime por eventos, interface gráfica, versões de contratos, mocks, replay, organização em módulos, segurança das demos e CI.
- **Decisões deliberadamente não tomadas:** linguagem final, framework de GUI, runtime principal, transporte IPC e modelo detalhado de custo.
- **Sem medições reais:** todos os exemplos de tempo, desempenho e Gantt são casos de referência para implementação futura, não experimentos executados.