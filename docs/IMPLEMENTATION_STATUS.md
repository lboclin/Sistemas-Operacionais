# Estado da implementação

O projeto contém um simulador de sistemas operacionais para um servidor que atende requisições do Jev (TypeSafe AI). **1 tick virtual = 1 token de entrada processado.** Os ticks não medem a latência real do Jev. Desde a [ADR 0002](adr/0002-foco-typesafe-jev.md), o tamanho de cada requisição é conhecido na chegada e não há previsão.

## Implementado

- Políticas FCFS, SJF (pelo tamanho da entrada), Round Robin e prioridade com aging.
- Sessões incrementais, snapshots, eventos, métricas e replay determinístico.
- Memória paginada FIFO/LRU, page faults com latência configurável e exportação de resultados.
- Geração de workloads reproduzíveis (CSV v2) e ferramentas de experimentos e análise.
- Painel web local para executar e comparar simulações.
- Laboratórios isolados de pthreads, deadlock, sinal SIGUSR1 e IPC por pipes (gateway e motor em processos separados).

## Removido

- SJF por média exponencial (EMA), SJF-oráculo, o preditor de tokens de saída (`tools/predictor/`) e a coluna `predicted_tokens`. Com o Jev, o tamanho do job é conhecido, então nada disso é necessário. Veja a [ADR 0002](adr/0002-foco-typesafe-jev.md).

## Limites

O sistema não executa inferência de modelos, não mantém serviço IPC persistente e não integra os laboratórios POSIX ao controlador principal como um serviço de produção. O replay visual do painel não pausa nem persiste o motor C.

## Compilar e executar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
