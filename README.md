# OS-llm-project

Simulador acadêmico de Sistemas Operacionais aplicado a requisições de LLM. Repositório privado, em desenvolvimento incremental.

## Estado atual

**Protótipo funcional (C11, provisório):** PCB e estados, gateway de admissão, FCFS, SJF estimado/oráculo, RR, prioridade com aging, workers virtuais, relógio de 1 tick/token, métricas, observação de eventos, paginação FIFO/LRU online integrada e replay offline FIFO/LRU/Ótimo. A CLI aceita cenários CSV, eventos, execução limitada por passos e snapshots intermediários. Há testes automatizados com CTest.

**Ainda não implementado:** threads POSIX reais, mutex e semáforos, IPC, sinais, deadlocks, SJF com média exponencial ou preditor treinado, p50/p95, experimentos completos e interface gráfica. A CLI é apenas uma ferramenta técnica para testar o núcleo; não substitui a GUI planejada.

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

A memória simulada é opcional. Por exemplo:

```bash
./build/os_llm_cli --frames 2 --page-tokens 2 --page-policy lru --trace
./build/os_llm_cli --frames 2 --page-fault-latency 2 --steps 1 --snapshots
```

Os eventos incluem chegadas, despachos, preempções, conclusões, hits, faults e evicções. Por padrão, faults não custam ticks extras. Com `--page-fault-latency N` (exige `--frames`), um miss bloqueia a requisição por N ticks virtuais e libera o worker; OPT só funciona em replay offline.

`--scenario` recebe linhas `id,arrival_tick,total_tokens,predicted_tokens,priority`, com no máximo 256 requisições; linhas iniciadas com `#` são comentários. Tempos são **virtuais**, não latências reais de inferência de LLM.


## Inspecionar a execução passo a passo

```bash
./build/os_llm_cli --steps 0 --snapshots
./build/os_llm_cli --policy rr --quantum 1 --steps 2 --snapshots
./build/os_llm_cli --workers 2 --frames 2 --page-policy lru --snapshots
```

`--steps N` limita a **N avanços da sessão**; `--steps 0` mostra o estado inicial sem executar tokens. `--snapshots` imprime o estado inicial e o estado após cada passo realizado. Um passo ocioso pode saltar o relógio até a próxima chegada. O relatório final indica `paused` ou `completed`, com `-` para ticks ainda não registrados. Sem limite, a sessão executa até terminar.

Os snapshots exibem contagens por estado, workers, progresso de cada PCB, estatísticas de páginas e o próximo wakeup virtual. O contrato `os_session_request_wakeup` permite consultar o tick de desbloqueio por ID. Em contenda extrema, uma página evictada durante a espera pode ser recarregada no primeiro retry sem segunda latência; pinning/fragmentação seguem pendentes. A CLI **não persiste sessões**: não é possível retomar uma execução em outra invocação. `--trace` pode ser combinado com as opções de inspeção.

## Módulos existentes

- `src/core/`: estrutura PCB e validação de estados;
- `src/gateway/`: validação e admissão de requisições;
- `src/scheduling/`: seleção de requisição para políticas de CPU;
- `src/runtime/`: sessão virtual incremental, snapshots e execução completa com múltiplos workers lógicos;
- `src/metrics/`: médias e eventos de execução/paginação;
- `src/memory/`: replay offline e cache online de frames FIFO/LRU integrado;
- `apps/cli/`: adaptador local de execução;
- `tests/`: testes unitários e de integração.

A estrutura de repositório prevê também os módulos futuros em `apps/visual/`, `src/concurrency/`, `src/ipc/`, `src/prediction/`, `src/experiments/` e `tools/`.

## Documentação e colaboração

- [Arquitetura](docs/ARCHITECTURE.md)
- [Especificação funcional completa](docs/FUNCTIONAL_SPEC.md)
- [Contratos planejados](docs/CONTRACTS.md)
- [Estado da implementação](docs/IMPLEMENTATION_STATUS.md)
- [Roadmap por commits (próximos 14)](docs/COMMIT_ROADMAP.md)
- [Interface gráfica planejada](docs/UI_SPEC.md)
- [Decisões pendentes](docs/DECISIONS.md)
- [Contribuição](CONTRIBUTING.md)

O projeto segue a proposta acadêmica original, mas a linguagem principal, modo de execução final e stack gráfica continuam sujeitos a decisão do grupo. Não existe banco SQL no escopo aprovado.
