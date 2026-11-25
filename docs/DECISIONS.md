# Registro de decisões

Decisões do grupo sobre o projeto. As que afetam vários módulos têm uma ADR em [adr/](adr/README.md).

| ID | Decisão | Estado | Escolha |
|---|---|---|---|
| D-01 | Linguagem do motor | Decidido | C11 ([ADR 0001](adr/0001-provisional-c11-core.md)) |
| D-02 | Interface gráfica | Decidido | Painel web local: servidor Python (biblioteca padrão) e JavaScript sem dependências |
| D-03 | Interface e motor | Decidido | O painel chama a CLI por subprocesso e lê o JSON exportado |
| D-04 | Execução | Decidido | Motor determinístico, com adaptador pthread separado |
| D-05 | IPC | Decidido | Pipe POSIX |
| D-06 | Modelo de custo | Decidido | 1 tick = 1 token de entrada ([ADR 0002](adr/0002-foco-typesafe-jev.md)) |
| D-07 | Licença | Pendente | Decisão do grupo |

O que está implementado: [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md).
