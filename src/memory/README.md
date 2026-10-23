# memória e paginação

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Manter frames, tabelas de páginas de KV simulado e substituição FIFO/LRU/OPT.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: core, runtime. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
