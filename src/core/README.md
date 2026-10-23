# núcleo de domínio

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Controlar PCBs, estados e invariantes; única fonte de verdade do estado de requisição.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: scheduling, runtime, gateway, memory. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
