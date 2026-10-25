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
| F20 Testes | Parcial | onze testes CTest; sem laboratório POSIX |

## Funcionalidades verificáveis agora

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/os_llm_cli --frames 2 --page-tokens 2 --page-policy lru --trace
```

- `src/gateway/`: validação e admissão.
- `src/scheduling/`: políticas já disponíveis.
- `src/runtime/`: simulação virtual, workers lógicos e eventos.
- `src/memory/`: replay offline e frames FIFO/LRU integrados, com liberação de páginas por requisição.
- `src/metrics/`: métricas e nomes/contratos de eventos.
- `apps/cli/`: configuração da carga, política, quantum, frames e trace.

## Testes

Verificação **local**, não uma execução confirmada no servidor GitHub:

- GCC (Debug): 11/11.
- Clang (Debug): 11/11.
- GCC com AddressSanitizer + UndefinedBehaviorSanitizer: 11/11.

A comparação dos SHAs de código-fonte publicado mostrou equivalência aos arquivos locais testados. O `CMakeLists.txt` publicado tem conteúdo semanticamente equivalente ao arquivo testado, embora a formatação não seja byte a byte idêntica. Não afirmar CI remoto verde sem verificar o workflow.

## Restrições explícitas

- PCB e workers são **lógicos**, não threads/processos Linux reais.
- Um token custa um tick virtual; faltas e substituições de páginas não adicionam ticks.
- Memória cheia implica evicção, não estado BLOCKED; o modelo não implementa custo de IO.
- `page_id` é derivado de `generated_tokens/tokens_per_page`; isso é um modelo de ensino, não um KV cache de inferência real.
- OPT possui conhecimento futuro e não pode ser selecionado no gerenciador online.
- O observer recebe eventos síncronos, não snapshots interativos.
- A linguagem C11 é **provisória**, sujeita à decisão do grupo.

## Próximas frentes

1. API de estados/snapshots para interface gráfica, passo a passo e replay.
2. Política didática de bloqueios, page faults com custo e recuperação.
3. Threads pthread + mutexes/semafóros com verificação de invariantes.
4. Deadlock, IPC POSIX e SIGUSR1 com tratamento seguro.
5. SJF adaptativo/IA, métricas percentis e experimentos completos.
6. GUI web/desktop após escolha de stack.
