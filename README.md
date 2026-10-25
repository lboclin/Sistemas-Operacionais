# OS-llm-project

Simulador acadêmico de Sistemas Operacionais aplicado a requisições de LLM. Repositório privado, em desenvolvimento incremental.

## Estado atual

**Primeira implementação funcional (C11, provisório):** PCB e transições de estado, FCFS, SJF por estimativa, SJF oráculo, Round Robin, prioridade com aging, workers **virtuais**, um relógio virtual de um tick por token, métricas médias e replay isolado de paginação FIFO/LRU/Ótimo. Uma CLI executa cenários de demonstração ou CSV. Há testes automatizados com CTest.

**Ainda não implementado:** threads POSIX reais, mutex e semáforos, IPC, sinais, deadlocks, integração da memória com o runtime, SJF com média exponencial ou preditor treinado, p50/p95, experimentos completos e interface gráfica. A CLI é apenas uma ferramenta técnica para testar o núcleo; não substitui a GUI planejada.

## Compilação e testes

Requer um compilador C11 e CMake >= 3.16. Em Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Para reproduzir a demonstração:

```bash
./build/os_llm_cli
./build/os_llm_cli --policy sjf --workers 2
./build/os_llm_cli --policy oracle
./build/os_llm_cli --policy rr --quantum 2
./build/os_llm_cli --policy priority
./build/os_llm_cli --scenario scenarios/sample.csv
```

`--scenario` recebe linhas `id,arrival_tick,total_tokens,predicted_tokens,priority`, com no máximo 256 requisições; linhas iniciadas com `#` são comentários. Tempos são **virtuais**, não latências reais de inferência de LLM.

## Módulos existentes

- `src/core/`: estrutura PCB e validação de estados;
- `src/scheduling/`: seleção de requisição para políticas de CPU;
- `src/runtime/`: execução virtual, com múltiplos workers lógicos;
- `src/metrics/`: espera, resposta, retorno e throughput médios;
- `src/memory/`: algoritmo independente de replay de referências de páginas;
- `apps/cli/`: adaptador local de execução;
- `tests/`: testes unitários e de integração.

A estrutura de repositório prevê também os módulos futuros em `apps/visual/`, `src/concurrency/`, `src/ipc/`, `src/prediction/`, `src/experiments/` e `tools/`.

## Documentação e colaboração

- [Arquitetura](docs/ARCHITECTURE.md)
- [Especificação funcional completa](docs/FUNCTIONAL_SPEC.md)
- [Contratos planejados](docs/CONTRACTS.md)
- [Estado da implementação](docs/IMPLEMENTATION_STATUS.md)
- [Interface gráfica planejada](docs/UI_SPEC.md)
- [Decisões pendentes](docs/DECISIONS.md)
- [Contribuição](CONTRIBUTING.md)

O projeto segue a proposta acadêmica original, mas a linguagem principal, modo de execução final e stack gráfica continuam sujeitos a decisão do grupo. Não existe banco SQL no escopo aprovado.
