# Estratégia de testes

O CTest organiza os testes por unidade, integração, CLI e concorrência. O conjunto atual contém 47 testes.

| Área | Cobertura |
|---|---|
| Unidade | PCB, gateway, escalonamento, memória, geração de cenários e métricas |
| Integração | sessões, snapshots, page faults, traces, replay e exportação |
| CLI | cenários CSV, argumentos, execução passo a passo e exportação de resultados |
| Concorrência e IPC | pool pthread, fila POSIX, adaptador paralelo, race lab, grafo de deadlock e pipes |

## Executar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

O [GitHub Actions](https://github.com/lboclin/Sistemas-Operacionais/actions) executa a suíte com GCC Debug, Clang Debug e GCC com AddressSanitizer e UndefinedBehaviorSanitizer.

## Limites da cobertura

- O laboratório de deadlock verifica ciclos em um grafo didático; não provoca deadlock real entre mutexes pthread.
- O transporte por pipes é testado isoladamente e ainda não integra o runtime principal.
- A interface gráfica, os experimentos completos e o ThreadSanitizer não fazem parte da suíte atual.
