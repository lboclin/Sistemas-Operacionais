# Contribuindo para o LLM OS Lab

## Antes de começar

Leia [arquitetura](docs/ARCHITECTURE.md), [especificação](docs/FUNCTIONAL_SPEC.md) e [contratos](docs/CONTRACTS.md). Combine interfaces compartilhadas antes de implementar módulos em paralelo. Não escolha frameworks ou bibliotecas globais sem registrar a decisão.

## Fluxo de trabalho

1. Abra uma issue com comportamento esperado, escopo, dependências e critérios de aceite.
2. Crie uma branch a partir de `main`: `feat/<modulo>-<objetivo>`, `fix/<modulo>-<objetivo>`, `docs/<assunto>` ou `test/<assunto>`.
3. Faça mudanças pequenas e isoladas, mantendo documentação e testes próximos do comportamento alterado.
4. Abra um PR explicando o que mudou, por quê, como foi verificado e o que permanece pendente.
5. Peça revisão cruzada quando modificar contratos, dados compartilhados ou semântica de métricas.

## Convenções

- **Alta coesão e baixo acoplamento**; cada módulo é dono das próprias regras e do estado que controla.
- Interface gráfica/CLI não alteram filas, PCBs ou memória diretamente.
- Não criar classes, abstrações, serviços, bancos ou diretórios sem uma responsabilidade real.
- Não adicionar segredos, modelos grandes ou datasets brutos ao Git.
- Não declarar testes executados se ainda não foram executados.
- A documentação é escrita preferencialmente em português; o núcleo provisório usa C11, identificadores `os_*` e headers próprios por módulo.\n- Antes de abrir PR, rode `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel && ctest --test-dir build --output-on-failure`.

## GitHub

Proposta de labels: `area:core`, `area:scheduling`, `area:memory`, `area:runtime`, `area:ui`, `area:ai`, `area:experiments`, `type:feature`, `type:bug`, `type:test`, `type:docs`, `priority:p0`, `priority:p1`, `blocked`, `decision-needed`.

Milestones sugeridos: M0 (contratos), M1 (núcleo), M2 (escalonamento/memória), M3 (concorrência), M4 (GUI), M5 (IA/experimentos), M6 (consolidação).

**CI:** workflow somente em pull requests e execução manual (sem gatilho em todo push), compilando e testando o núcleo. **Licença, CODEOWNERS e proteção de branch** permanecem pendentes de decisão do grupo.