# Estado da implementação

O projeto contém um simulador de sistemas operacionais para workloads virtuais de geração de texto. Os ticks representam avanço simulado e não medem inferência ou latência real de LLM.

## Implementado

- Políticas FCFS, SJF, SJF por média exponencial (EMA), oráculo, Round Robin e prioridade com aging.
- Sessões incrementais, snapshots, eventos, métricas e replay determinístico.
- Memória paginada FIFO/LRU, page faults com latência configurável e exportação de resultados.
- Geração de workloads reproduzíveis, previsão SJF treinada e ferramentas de análise de experimentos.
- Painel web local para executar e comparar simulações.
- Laboratórios isolados de pthreads, deadlock e IPC por pipes.

## Limites

O sistema não executa inferência de modelos, não mantém serviço IPC persistente e não integra os laboratórios POSIX ao controlador principal como um serviço de produção. O replay visual do painel não pausa nem persiste o motor C.

## Compilar e executar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
