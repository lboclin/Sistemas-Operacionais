# mecanismos de execução

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Progredir trabalho e coordenar workers, em relógio virtual ou POSIX; reportar eventos e capacidades.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: core, scheduling, memory, metrics. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
