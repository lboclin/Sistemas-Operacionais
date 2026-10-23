# integração de previsões

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Usar predições validadas sem obrigar o motor básico a carregar Python/LLM.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: scheduling, workload. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
