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

O [GitHub Actions run 40255073092](https://github.com/fragaeduardo/OS-llm-project/actions/runs/40255073092) confirmou **34/34** testes em GCC Debug, Clang Debug e GCC ASan/UBSan. A suíte é baseada em asserts, mantidos ativos mesmo com NDEBUG para os alvos registrados. Os testes golden incluem page fault, fragmentação, fingerprint de trace, seed 42 e percentis nearest-rank.

## Riscos que continuam exigindo testes

Nenhum PCB em dois workers simultaneamente; nunca alocar além dos frames configurados; despertar requisições bloqueadas sem livelock; garantir que o preditor não enxergue resposta real; tratamento de `SIGUSR1` fora do handler; corrida e deadlock isolados com timeout/watchdog. Os testes de concorrência não devem depender de uma corrida acontecer necessariamente.

Ver [especificação funcional](FUNCTIONAL_SPEC.md).

Commit 15: testes pipe_channel e ipc_demo (45 configurados, 43 anteriormente aprovados no CI). Os novos testes passaram isoladamente em GCC/Clang/ASan; o [CI 40427656618](https://github.com/fragaeduardo/OS-llm-project/actions/runs/40427656618) também concluiu com sucesso nas três configurações, com 45 testes CTest configurados.

## Bloco J — testes randômicos determinísticos (02/11/2026)

O CTest `core_stress_seeded_py` usa 6 seeds fixas × 6 políticas (FCFS/SJF/EMA/Oracle/RR/prioridade) × 2 configurações de paginação = 72 execuções reais da CLI C, além de repetições determinísticas e casos malformados. Verifica estado TERMINATED, tokens produzidos, ordenação de eventos e sequence, page faults/evictions, limite de frames, identidade da carga, fingerprint, igualdade das métricas após replay. A aleatoriedade está inteiramente parametrizada por seeds reprodutíveis; cada subprocesso tem timeout. Este teste complementa, não substitui, os testes unitários, ASan/UBSan e análises de concorrência.
