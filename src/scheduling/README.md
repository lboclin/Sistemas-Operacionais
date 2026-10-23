# políticas de escalonamento

**Estado:** módulo planejado, ainda sem implementação.

## Responsabilidade
Decidir próximo PCB elegível conforme FCFS, SJF, RR e prioridade com aging; expor contrato substituível.

## Integração
Interfaces apenas pelos contratos públicos com os módulos relacionados: core, runtime. Não acessar estado interno de outro módulo.

## Antes de codificar
Aprovar contratos em [CONTRACTS.md](../../docs/CONTRACTS.md), especificar testes observáveis e registrar escolhas técnicas com ADR quando necessário.
