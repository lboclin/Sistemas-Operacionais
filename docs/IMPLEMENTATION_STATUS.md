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
| F10 Memória KV simulada | Parcial | frames online por página, sem custo ou bloqueio por fault |
| F11 Substituição | Parcial | FIFO/LRU online e FIFO/LRU/OPT offline |
| F16 Métricas/eventos | Parcial | médias, throughput, faults/hits/evictions e callbacks síncronos |
| Snapshots/stepping | Implementado no motor virtual | sessão opaca, snapshots entre passos; sem persistência nem GUI |
| F20 Testes | Parcial | vinte testes CTest; sem laboratório POSIX |

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
- Um token custa um tick virtual; faltas e substituições de páginas não adicionam ticks.
- Memória cheia implica evicção, não estado BLOCKED; o modelo não implementa custo de IO.
- `page_id` é derivado de `generated_tokens/tokens_per_page`; isso é um modelo de ensino, não um KV cache de inferência real.
- OPT possui conhecimento futuro e não pode ser selecionado no gerenciador online.
- O observer recebe eventos síncronos; snapshots consistentes são consultados entre passos pela API da sessão, não durante callbacks.
- A linguagem C11 é **provisória**, sujeita à decisão do grupo.

## Próximas frentes

1. Política didática de bloqueios, page faults com custo e recuperação.
2. Threads pthread + mutexes/semafóros com verificação de invariantes.
3. Deadlock, IPC POSIX e SIGUSR1 com tratamento seguro.
4. SJF adaptativo/IA, métricas percentis e experimentos completos.
5. GUI web/desktop após escolha de stack.
