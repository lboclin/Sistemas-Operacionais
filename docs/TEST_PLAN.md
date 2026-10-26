# Estratégia de testes

> Atualizado em 26/10/2026. Núcleo C11 virtual testado; laboratório POSIX, exportação de resultados e interface ainda pendentes.

| Tipo | Cobertura atual | Pendente |
|---|---|---|
| Unidade | PCB, gateway, FIFO/LRU/OPT, tabelas, gerador com seed | novos contratos POSIX |
| Integração | gateway→scheduler→runtime→memória→eventos, snapshots, page faults, replay e percentis | exportação JSON/CSV e pthreads |
| Sistema/CLI | cenários legados/v1, limites, tracing, snapshots, flags inválidas | fluxos E2E com exportação |
| Concorrência | ainda não implementada | mutex, semáforo, race isolada, deadlock e watchdog |
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
