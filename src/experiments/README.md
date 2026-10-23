# orquestração dos experimentos

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Executar matrizes de cenário/política, coletar métricas e identificar resultados reprodutíveis.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: workload, scheduling, memory, metrics. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
