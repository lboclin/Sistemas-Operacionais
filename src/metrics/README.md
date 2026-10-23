# telemetria e métricas

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Coletar eventos, snapshots e cálculos de espera, retorno, resposta, throughput, faults e utilização.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: core, runtime, experiments. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
