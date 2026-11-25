# Estado da implementação

Simulador de sistemas operacionais para um servidor que atende requisições do Jev (TypeSafe AI). **1 tick virtual = 1 token de entrada processado.** Os ticks não medem a latência real do Jev. Desde a [ADR 0002](adr/0002-foco-typesafe-jev.md), o tamanho de cada requisição é conhecido na chegada e não há previsão.

## Implementado

- Políticas FCFS, SJF (pelo tamanho da entrada), Round Robin, prioridade com aging e HRRN (não preemptivo, razão comparada com aritmética inteira).
- `--aging-interval N` na CLI (padrão 2; 0 desativa o aging).
- Sessões incrementais, snapshots, eventos, métricas e replay determinístico.
- Memória paginada FIFO/LRU, page faults com latência configurável e exportação de resultados.
- Geração de workloads reproduzíveis (CSV v2) e cenário [scenarios/starvation.csv](../scenarios/starvation.csv), em que o SJF só atende o pedido longo quando o fluxo de curtos termina.
- Painel web local para executar e comparar simulações, com Gantt e replay visual.
- Laboratórios isolados de corrida, deadlock, sinal SIGUSR1 e IPC por pipes (gateway e motor em processos separados).
- Experimentos E1–E4 do CHK3, com script de reprodução, relatórios e agregados: [results/chk3/SUMMARY.md](../results/chk3/SUMMARY.md).
- 70 testes no CTest (com Node e Python instalados), executados no CI com gcc, clang e gcc com ASan/UBSan. Ver [TEST_PLAN.md](TEST_PLAN.md).

## Removido (histórico)

- SJF por média exponencial (EMA), SJF-oráculo, o preditor de tokens de saída (`tools/predictor/`) e a coluna `predicted_tokens`. Com o Jev, o tamanho do job é conhecido, então nada disso é necessário. Veja a [ADR 0002](adr/0002-foco-typesafe-jev.md).

## Limites

- Os ticks são virtuais: o sistema não chama o Jev nem executa inferência, e o custo proporcional ao tamanho da entrada é uma simplificação. As cargas dos experimentos são sintéticas.
- Em memória, a latência é cobrada uma vez por falta atendida: se a página for removida durante o I/O, a recarga conta como falta, mas não bloqueia de novo. Não há fixação (pinning) de frames, e OPT existe só no replay offline.
- `waiting_ticks` é a espera acumulada na fila; no Round Robin, `starvation_count` não significa que a requisição deixou de executar.
- Os laboratórios de corrida e deadlock rodam fora do motor e forçam a intercalação: mostram o fenômeno, não a sua frequência. Não há detecção de deadlock no runtime principal (o grafo de recursos é um modelo didático separado).
- O IPC por pipes é uma demonstração, sem serviço persistente. A execução com pthreads é um adaptador separado, não exposto na CLI.
- O painel executa uma simulação até o fim a cada clique; o replay visual não pausa nem persiste o motor C. O painel aceita até 16 workers e 256 requisições.

## Compilar e executar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
