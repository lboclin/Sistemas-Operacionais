# comunicação e sinais

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Implementar, quando decidido, pipe ou memória POSIX entre processos e depuração SIGUSR1 segura.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: runtime, app. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
