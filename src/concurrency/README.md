# laboratório de sincronização

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Encapsular mutexes, semáforos, demonstrações de corrida, deadlock e watchdog de segurança.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: runtime, app. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
