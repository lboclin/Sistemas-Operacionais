# Estratégia de testes

> Atualizado em 26/10/2026. Núcleo virtual, POSIX e exportação implementados parcialmente; GUI e integração IPC pendentes.

| Tipo | Cobertura atual | Pendente |
|---|---|---|
| Unidade | PCB, gateway, FIFO/LRU/OPT, tabelas, gerador com seed | novos contratos POSIX |
| Integração | gateway→scheduler→runtime→memória→eventos, snapshots, page faults, replay e percentis | exportação JSON/CSV e pthreads |
| Sistema/CLI | cenários legados/v1, limites, tracing, snapshots, flags inválidas | fluxos E2E com exportação |
| Concorrência | pool pthread, fila POSIX, race isolada, grafo de deadlock, IPC com pipes/fork | deadlock real pthread, integração IPC e SIGUSR1 |
| Interface | ainda não implementada | GUI e testes E2E após escolha da stack |

## Execução

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Uma execução histórica na origem confirmou **34/34** testes em GCC Debug, Clang Debug e GCC ASan/UBSan; seus logs não foram migrados. Consulte as [execuções deste repositório](https://github.com/lboclin/Sistemas-Operacionais/actions). A suíte é baseada em asserts, mantidos ativos mesmo com NDEBUG para os alvos registrados. Os testes golden incluem page fault, fragmentação, fingerprint de trace, seed 42 e percentis nearest-rank.

## Riscos que continuam exigindo testes

Nenhum PCB em dois workers simultaneamente; nunca alocar além dos frames configurados; despertar requisições bloqueadas sem livelock; garantir que o preditor não enxergue resposta real; tratamento de `SIGUSR1` fora do handler; corrida e deadlock isolados com timeout/watchdog. Os testes de concorrência não devem depender de uma corrida acontecer necessariamente.

Ver [especificação funcional](FUNCTIONAL_SPEC.md).

Commit 15: testes pipe_channel e ipc_demo (45 configurados, 43 anteriormente aprovados no CI). Os novos testes passaram isoladamente em GCC/Clang/ASan; o CI histórico da origem também concluiu com sucesso nas três configurações, com 45 testes CTest configurados.
