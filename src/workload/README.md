# cargas e cenários

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Gerar, validar, importar e versionar cenários de requisições com seed controlada.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: gateway, experiments, app. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
