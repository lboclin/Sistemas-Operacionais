# Decisões técnicas pendentes

As decisões finais do grupo seguem pendentes. Para viabilizar o primeiro núcleo testável, C11 + CMake foram adotados **provisoriamente**; a escolha definitiva de C ou C++ ainda depende do grupo/professor. Registre escolhas importantes como ADRs em `docs/adr/`.

| ID | Decisão | Alternativas / observações |
|---|---|---|
| D-01 | Linguagem do motor | Primeiro núcleo em C11 (provisório); C11 ou C++20 definitivo pendente |
| D-02 | Interface gráfica | web local ou desktop; stack ainda pendente |
| D-03 | Interface↔motor | API local, IPC ou ligação direta/FFI |
| D-04 | Execução | determinística e POSIX pthreads; organização e prioridade |
| D-05 | IPC de SO | pipe ou memória compartilhada POSIX |
| D-06 | Modelo de custo | custo fixo/token ou calibração com carga real |
| D-07 | Licença | grupo decide antes de tornar o repositório público |

O nome do repositório definido é `OS-llm-project`. O código inicial está descrito em [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md).
