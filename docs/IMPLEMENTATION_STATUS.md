# Estado da implementação

> Atualizado em 25/10/2026. Descreve o código da branch `main`. O projeto acadêmico ainda não está completo.

## Entregue e limitações

| Requisito | Situação | Detalhe |
|---|---|---|
| F01 PCB e estados | Parcial | estruturas e transições verificadas |
| F02 Gateway | Parcial | validação, duplicidade e admissão por tick |
| F03 FCFS | Parcial | escalonador virtual |
| F04 SJF | Parcial | estimativa fixa e oráculo; preditor/média exponencial pendentes |
| F05 Round Robin | Parcial | quantum em tokens com preempção |
| F06 Prioridade com aging | Parcial | envelhecimento em dispatch |
| F07 Pool de workers | Parcial | workers virtuais; pthreads pendentes |
| F10 Memória KV simulada | Parcial | frames online por página e visão de mapeamentos por requisição, com bloqueio opcional por fault e wakeup virtual; pressão de memória pendente |
| F11 Substituição | Parcial | FIFO/LRU online e FIFO/LRU/OPT offline |
| F16 Métricas/eventos | Parcial | médias, throughput, faults/hits/evictions e callbacks síncronos |
| Snapshots/stepping | Implementado no motor virtual | sessão opaca, snapshots entre passos; sem persistência nem GUI |
| F20 Testes | Parcial | 22 testes CTest configurados; sem laboratório POSIX; teste de latência de página adicionado |

## Funcionalidades verificáveis agora

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/os_llm_cli --frames 2 --page-tokens 2 --page-policy lru --trace
```

- `src/gateway/`: validação e admissão.
- `src/scheduling/`: políticas já disponíveis.
- `src/runtime/`: sessão incremental, snapshots, workers lógicos e eventos; execução completa reutiliza o mesmo motor.
- `src/memory/`: replay offline e frames FIFO/LRU integrados, com liberação de páginas por requisição.
- `src/metrics/`: métricas e nomes/contratos de eventos.
- `apps/cli/`: configuração da carga, política, quantum, frames, trace, limite de passos e snapshots.


## Inspeção incremental

```bash
./build/os_llm_cli --steps 0 --snapshots
./build/os_llm_cli --policy rr --quantum 1 --steps 2 --snapshots
./build/os_llm_cli --workers 2 --frames 2 --snapshots
```

`os_session_create` copia os PCBs e valida a configuração; `os_session_step` executa até um token por worker ocupado, podendo saltar ticks ociosos. `os_session_snapshot` copia métricas, contadores de estados, workers e PCBs; exige buffers suficientes e falha sem escrita parcial quando a capacidade é insuficiente. `os_session_run` termina a sessão; chamadas posteriores a `step` são idempotentes.

`--steps N` limita a execução a N passos; `--snapshots` exibe o estado inicial e cada avanço. A CLI encerra o processo após o limite e não permite retomada entre invocações. Médias em execução parcial consideram somente PCBs concluídos.

## Testes

Verificação **local**, não uma execução confirmada no servidor GitHub:

- GCC 14.2 (Debug e Release): 20/20.
- Clang 17 (Debug): 20/20.
- GCC com AddressSanitizer + UndefinedBehaviorSanitizer: 20/20.

A validação foi local, com reconstrução das fontes equivalentes ao núcleo publicado. Não foi confirmado um workflow remoto para este commit.

## Restrições explícitas

- PCB e workers são **lógicos**, não threads/processos Linux reais.
- Um token custa um tick virtual; faltas adicionam bloqueio configurável com `page_fault_latency_ticks` (padrão 0).
- Memória cheia implica evicção; `OS_BLOCKED` modela espera por I/O quando a latência é positiva. Não há reserva/pinning de frames.
- `page_id` é derivado de `generated_tokens/tokens_per_page`; isso é um modelo de ensino, não um KV cache de inferência real.
- OPT possui conhecimento futuro e não pode ser selecionado no gerenciador online.
- O observer recebe eventos síncronos; snapshots consistentes são consultados entre passos pela API da sessão, não durante callbacks.
- A linguagem C11 é **provisória**, sujeita à decisão do grupo.

## Validação de PCBs e plano de commits (25/10/2026)

- A validação de um PCB ainda não admitido agora é centralizada em `os_request_is_fresh` (`src/core/`), aplicada tanto em `os_gateway_validate` quanto em `os_gateway_admit`.
- Campos indevidamente preenchidos antes da admissão (`ready_tick`, `ready_order`, marcas de execução, contadores) passam a ser rejeitados sem modificar a estrutura.
- Novo teste `fresh_pcb` registrado em CTest: **22 testes configurados** no projeto. O novo teste foi executado isoladamente com GCC, Clang e ASan/UBSan (aprovado); a suíte integral não foi reexecutada nesta alteração.
- Planejamento dos próximos commits: [COMMIT_ROADMAP.md](COMMIT_ROADMAP.md). Tempos são estimativas de trabalho, não datas de commit fabricadas.

## Mapeamento de páginas residentes por requisição (25/10/2026)

- `os_frames_lookup` consulta o índice físico da página lógica (`request_id`, `page_id`) sem alterar métricas nem política de substituição.
- `os_frames_snapshot_request` oferece uma visão da tabela residente de um pedido, com verificação de capacidade e falha sem escrita parcial. O índice do frame não é estável após mutações; não há tabela redundante para sincronizar.
- Novo teste `page_tables` em CTest: **22 testes configurados**. O teste novo e o teste `frames` foram compilados e executados isoladamente com GCC e Clang (`-Wall -Wextra -Wpedantic -Wconversion -Werror`) e GCC com ASan/UBSan: 2/2 aprovados em cada configuração. A suíte completa não foi executada nesta alteração.
- **Implementado no commit 03:** latência opcional por falta de página, BLOCKED/READY, worker liberado, wakeup, consulta por requisição e snapshot com próximo wakeup. **Pendente:** exposição dos mapeamentos na API de sessão; a política de pressão usa evicção FIFO/LRU, e a fragmentação interna é observável no snapshot.

## Próximas frentes

1. Política didática de bloqueios, page faults com custo e recuperação.
2. Threads pthread + mutexes/semafóros com verificação de invariantes.
3. Deadlock, IPC POSIX e SIGUSR1 com tratamento seguro.
4. SJF adaptativo/IA, métricas percentis e experimentos completos.
5. GUI web/desktop após escolha de stack.

## Page faults com latência (commit 03)

`OsVirtualConfig.page_fault_latency_ticks` controla espera virtual por falta de página: 0 mantém a execução anterior. Com valor positivo, um miss bloqueia a requisição, libera o worker e agenda wakeup. O próximo passo pode saltar ticks ociosos até uma chegada ou wakeup; o primeiro token é contabilizado somente quando efetivamente gerado. `os_session_request_wakeup` consulta o prazo por ID, e `OsSessionSnapshot.next_wakeup_tick` indica o mais próximo (ou `OS_REQUEST_NEVER`).

A página é inserida no cache ao detectar o fault. Se for evictada durante a espera, o primeiro acesso após o wakeup pode recarregá-la sem segunda espera; o contador registra esse novo miss. Este modelo simplificado evita livelock em pressão extrema, mas **não implementa pinning, fila de I/O, fragmentação nem garantias de residência durante o bloqueio** (commit 04).

Teste novo: `page_fault_latency` (bloqueio, wakeup, snapshot, compatibilidade zero, contenda e overflow). A verificação de CI será registrada após execução, sem antecipar resultados.

## Pressão de frames e fragmentação (commit 04)

Frames são páginas de tamanho fixo. Com capacidade cheia, o cache substitui deterministicamente por FIFO ou LRU; não existe espera por frame livre. O estado BLOCKED representa exclusivamente I/O virtual de page fault, não fila de alocação. Páginas de requisições concluídas são liberadas. Em pressão extrema, uma página reservada por um fault pode ser evictada enquanto o pedido aguarda; o primeiro retry após wakeup não cobra nova latência (limitação didática explícita).

`os_frames_page_at` consulta uma posição física sem alterar relógios ou métricas. `OsSessionSnapshot.free_frames` informa a capacidade livre; `internal_fragmentation_tokens` soma os slots ainda não preenchidos nas páginas **residentes**, incluindo páginas alocadas para I/O pendente. Não há fragmentação externa porque todos os frames têm tamanho fixo. A métrica é instantânea, não pico nem histórico.

Teste novo: `memory_pressure` (golden de fragmentação, pressão FIFO, frame liberado, página durante bloqueio, consulta física). Validação CI registrada na execução correspondente.

## Traces determinísticos e verificação (commit 05)

Cada `OsEvent` observado recebe `sequence` monotônico. O módulo `src/metrics/trace.{h,c}` captura eventos em ordem, sinaliza falha de alocação, compara campos explicitamente (sem depender de padding) e calcula fingerprint FNV-1a64 com codificação little-endian e versão `OS_TRACE_VERSION=1`. O hash é de integridade/reprodutibilidade, **não criptográfico**.

`os_trace_verify_replay` reexecuta o mesmo cenário fresco e a mesma configuração em uma sessão independente, comparando eventos por índice; não reconstrói estados de PCB a partir do log. Retorna `OS_RUN_OK` para replay executado mesmo se `matched=false`, e informa o primeiro evento divergente. Ordenação atual: chegada por ordem de entrada, despacho por índice do worker, acessos por índice do worker, conclusão/preempção por worker; callbacks síncronos. Eventos de chegada ao fim de um tick podem preceder wakeups processados no início do próximo passo no mesmo tick virtual.

Teste `trace_replay`: golden de sequência e fingerprint, repetição determinística com faults/RR, alteração de evento e configuração divergente. A CLI `--trace` exibe `seq`.
