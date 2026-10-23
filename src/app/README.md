# camada de aplicação

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Coordenar comandos, consultas e sessões do simulador; não codificar políticas nem manipular memória diretamente.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: core, gateway, workload, runtime, metrics, experiments. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
