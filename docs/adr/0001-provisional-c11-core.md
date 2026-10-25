# ADR 0001 — C11 provisório para o primeiro núcleo

**Estado:** provisório, revisável pelo grupo.

## Contexto
A proposta acadêmica original descreve o simulador em C. O grupo ainda não aprovou a escolha definitiva entre C e C++ e não definiu a GUI. Implementar o núcleo mínimo em C11 permite validar estruturas, regras e políticas sem impor framework gráfico.

## Decisão nesta etapa
Usar C11 e CMake/CTest somente para o primeiro conjunto de funcionalidades, com módulos públicos por headers e sem dependência de interface gráfica.

## Consequências
- Trabalha diretamente com os conceitos da disciplina.
- A UI continua independente e não exige Python, Qt ou React.
- A adoção definitiva de C, C++, POSIX e adaptadores de GUI continua em aberto.
- Alterar linguagem mais tarde pode exigir refatoração; não se promete migração automática.
