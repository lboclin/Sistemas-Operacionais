# ADR 0001 — C11 para o núcleo

**Estado:** aceita. C11 foi mantido até a entrega. A decisão começou como provisória em outubro de 2026 e foi confirmada depois.

## Contexto
A proposta acadêmica descreve o simulador em C. Em outubro, o grupo ainda não havia escolhido entre C e C++ nem definido a interface. Implementar o núcleo mínimo em C11 permitia validar estruturas, regras e políticas sem impor um framework gráfico.

## Decisão
Usar C11 e CMake/CTest, com módulos públicos por headers e sem dependência de interface gráfica no núcleo.

## Consequências
- Trabalha diretamente com os conceitos da disciplina.
- O núcleo não depende da interface. O painel escolhido depois (Python e JavaScript) chama a CLI por subprocesso.
- O código POSIX (pthreads, pipes, sinais) fica em bibliotecas separadas do núcleo.
- Trocar de linguagem exigiria refatoração.
