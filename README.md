# OS-llm-project · LLM OS Lab

**Laboratório interativo de Sistemas Operacionais aplicado ao atendimento de requisições de modelos de linguagem (LLMs).**

> **Estado:** organização inicial do repositório e planejamento. Nenhum simulador, interface gráfica, preditor ou experimento foi implementado ainda.

O objetivo acadêmico é modelar requisições de LLM como processos lógicos e estudar como decisões de escalonamento, memória, concorrência e comunicação afetam seu processamento.

## Escopo planejado

- Processos/PCB e estados: novo, pronto, executando, bloqueado e terminado.
- Políticas FCFS, SJF (estimadores e oráculo), Round Robin e prioridade com aging.
- Pool de workers, sincronização, race conditions, deadlocks, IPC POSIX e `SIGUSR1`.
- Memória de KV cache **simulada**, paginação e substituição FIFO/LRU/Ótimo.
- Geração de cargas, predição de tamanho de resposta, métricas e experimentos reproduzíveis.
- Interface **gráfica** para criar requisições, executar cenários e observar resultados.

## Documentação

- [Arquitetura e organização do repositório](docs/ARCHITECTURE.md) — fronteiras, módulos, colaboração e estrutura.
- [Especificação funcional](docs/FUNCTIONAL_SPEC.md) — funcionalidades e critérios de aceite, incluindo as duas opções de execução.
- [Contratos (rascunho)](docs/CONTRACTS.md) — vocabulário, comandos, eventos e observação.
- [Planejamento da interface gráfica](docs/UI_SPEC.md) — telas, interações e MVP visual.
- [Plano de testes](docs/TEST_PLAN.md) e [experimentos](docs/EXPERIMENTS.md).
- [Decisões técnicas pendentes](docs/DECISIONS.md) e [glossário](docs/GLOSSARY.md).
- [Como contribuir](CONTRIBUTING.md).

## Organização

```text
src/          Regras do simulador, escalonamento, memória, execução e métricas
apps/         Interface gráfica futura e ferramenta de automação por comandos
contracts/    Formatos de troca entre aplicações e motor
scenarios/    Pequenos cenários versionados de referência
tests/        Testes automatizados por nível
tools/        IA, coleta e análises auxiliares
configs/      Configurações de exemplo
docs/         Especificações e decisões
.github/      Templates de colaboração no GitHub
```

Cada módulo tem um `README.md` de responsabilidade. As pastas não contêm código-fonte fictício.

## Início de implementação

1. Alinhar com o professor se a linguagem precisa ser C ou se C++ é permitido.
2. Definir os contratos comuns de requisição, cenário, evento e resultado.
3. Aprovar os critérios de aceite do primeiro fluxo; só então configurar compilação e testes da stack escolhida.
4. Desenvolver módulos em branches pequenas e integrar continuamente.

**Não há comando de execução por enquanto**, porque a linguagem, o runtime principal e a tecnologia da interface ainda não foram escolhidos. Não há banco SQL nem requisito de banco de dados definido.

Projeto acadêmico em equipe. Repositório privado em fase de planejamento. [Proposta original e relação com a disciplina](docs/FUNCTIONAL_SPEC.md).