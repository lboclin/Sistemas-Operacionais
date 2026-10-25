# entrada de requisições

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Validar e admitir requisições no momento apropriado, emitindo eventos ao motor.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: core, workload, app. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
