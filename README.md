# OS-llm-project

Simulador acadêmico de Sistemas Operacionais aplicado a requisições de LLM. Repositório privado, em desenvolvimento incremental.

## Estado atual

**Protótipo funcional (C11, provisório):** PCB e estados, gateway de admissão, FCFS, SJF estimado/oráculo, RR, prioridade com aging, workers virtuais, relógio de 1 tick/token, p50/p95 e métricas de utilização, traces determinísticos com replay verificável, gerador de cargas com seed, paginação FIFO/LRU online integrada e replay offline FIFO/LRU/Ótimo. A CLI aceita cenários CSV, eventos, execução limitada por passos e snapshots intermediários. Há 37 testes automatizados no CTest, aprovados no CI com GCC, Clang e GCC ASan/UBSan. Exportação JSON/CSV versionada e componentes POSIX isolados (pool pthread e fila protegida) estão disponíveis.

**Ainda não implementado:** integração entre threads POSIX, fila, escalonamento e KV cache; IPC, sinais, laboratório de corrida, deadlocks, SJF com média exponencial ou preditor treinado, experimentos completos e interface gráfica. A CLI é apenas uma ferramenta técnica para testar o núcleo; não substitui a GUI planejada.

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

Os eventos incluem chegadas, despachos, preempções, conclusões, hits, faults, evicções, bloqueios e wakeups. Cada evento observado tem `sequence` monotônico; `src/metrics/trace.h` oferece captura, fingerprint versionado e `src/runtime/replay.h` verifica replay por reexecução, sem inventar estados. Por padrão, faults não custam ticks extras. Com `--page-fault-latency N` (exige `--frames`), um miss bloqueia a requisição por N ticks virtuais e libera o worker; OPT só funciona em replay offline.

`--scenario` recebe linhas `id,arrival_tick,total_tokens,predicted_tokens,priority`, com no máximo 256 requisições; linhas iniciadas com `#` são comentários. Tempos são **virtuais**, não latências reais de inferência de LLM.


## Inspecionar a execução passo a passo

```bash
./build/os_llm_cli --steps 0 --snapshots
./build/os_llm_cli --policy rr --quantum 1 --steps 2 --snapshots
./build/os_llm_cli --workers 2 --frames 2 --page-policy lru --snapshots
```

`--steps N` limita a **N avanços da sessão**; `--steps 0` mostra o estado inicial sem executar tokens. `--snapshots` imprime o estado inicial e o estado após cada passo realizado. Um passo ocioso pode saltar o relógio até a próxima chegada. O relatório final indica `paused` ou `completed`, com `-` para ticks ainda não registrados. Sem limite, a sessão executa até terminar.

Os snapshots exibem contagens por estado, workers, progresso de cada PCB, estatísticas de páginas, frames livres, fragmentação interna de páginas residentes e o próximo wakeup virtual. O contrato `os_session_request_wakeup` permite consultar o tick de desbloqueio por ID. Em contenda extrema, uma página evictada durante a espera pode ser recarregada no primeiro retry sem segunda latência; não há pinning nem fragmentação externa por blocos variáveis (a fragmentação interna é medida). A CLI **não persiste sessões**: não é possível retomar uma execução em outra invocação. `--trace` pode ser combinado com as opções de inspeção.

## Módulos existentes

- `src/core/`: estrutura PCB e validação de estados;
- `src/gateway/`: validação e admissão de requisições;
- `src/scheduling/`: seleção de requisição para políticas de CPU;
- `src/runtime/`: sessão virtual incremental, snapshots e execução completa com múltiplos workers lógicos;
- `src/metrics/`: médias, percentis, traces e eventos de execução/paginação;
- `src/memory/`: replay offline e cache online de frames FIFO/LRU integrado;
- `src/workload/`: geração determinística de cenários versionados;
- `src/results/`: exportação de resultados virtuais como JSON v1 e CSV longo;
- `src/concurrency/`: pool pthread e fila POSIX independentes;
- `apps/cli/`: adaptador local de execução;
- `apps/workload/`: gerador de CSV v1;
- `tests/`: testes unitários e de integração.

A estrutura prevê também os módulos futuros em `apps/visual/`, `src/ipc/`, `src/prediction/`, `src/experiments/` e `tools/`. A biblioteca separada `os_posix` contém o ciclo de vida pthread e a fila POSIX, ainda não conectados ao motor virtual.

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


## Gerar cargas reproduzíveis

```bash
./build/os_llm_workload --seed 42 --count 4 --distribution uniform --arrival-span 10 --min-tokens 2 --max-tokens 8 --output scenario.csv
./build/os_llm_cli --scenario scenario.csv
```

O gerador usa SplitMix64 determinístico, com distribuições `uniform`, `burst` e `staggered`. O CSV v1 inclui seed, distribuição e quantidade no cabeçalho. Cenários legados sem cabeçalho continuam válidos; versões desconhecidas são rejeitadas. Há limite de 256 requisições.


## Métricas de desempenho

O resumo da CLI inclui p50/p95 (espera, resposta e turnaround), espera máxima, utilização dos workers, ticks bloqueados e preempções. O limiar opcional `--starvation-threshold N` marca pedidos concluídos com espera acumulada >= N e pedidos atualmente READY com idade >= N. Os percentis usam nearest-rank e consideram apenas pedidos concluídos. Eventos de despacho/preempção/bloqueio/conclusão permitem gerar intervalos de Gantt, ainda sem interface gráfica.

## Exportar resultados e testar POSIX

```bash
./build/os_llm_cli --scenario scenarios/seed42-v1.csv --policy rr --quantum 2 \
  --export-json run.json --export-csv run.csv
ctest --test-dir build -R '^(export|thread_pool|work_queue)$' --output-on-failure
```

Os artefatos contêm a entrada, configuração, seed quando disponível, PCBs finais, eventos e métricas; tempos são ticks virtuais. O CSV usa colunas `section,index,field,value`. Detalhes em [src/results/README.md](src/results/README.md). Os testes POSIX exercitam inicialização/parada/join e fila limitada com mutex, semáforos e 4000 itens sob concorrência. O pool **ainda não executa requisições do simulador**. [CI verificado (37/37 × 3)](https://github.com/fragaeduardo/OS-llm-project/actions/runs/40307085061).
