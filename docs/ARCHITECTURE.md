# Arquitetura

Simulador em C11 de um servidor do Jev. O núcleo (`os_core`) é determinístico e não cria threads; tudo o que usa threads ou processos fica em bibliotecas e executáveis separados. 1 tick virtual = 1 token de entrada ([ADR 0002](adr/0002-foco-typesafe-jev.md)).

## Módulos

| Diretório | Responsabilidade |
| --- | --- |
| [`src/core`](../src/core/README.md) | PCB (`OsRequest`), criação validada e transições NEW, READY, RUNNING, BLOCKED e TERMINATED |
| [`src/gateway`](../src/gateway/README.md) | Valida a carga (IDs únicos, PCBs novos) e admite chegadas |
| [`src/scheduling`](../src/scheduling/README.md) | Escolhe o próximo PCB READY: FCFS, SJF, RR, prioridade com aging ou HRRN |
| [`src/runtime`](../src/runtime/README.md) | Sessão (`session.c`): relógio, workers lógicos, despacho, quantum e bloqueios; replay de traces |
| [`src/memory`](../src/memory/README.md) | Frames FIFO/LRU usados pela sessão; `pager.c` reproduz traces offline com FIFO, LRU e OPT |
| [`src/metrics`](../src/metrics/README.md) | Eventos, trace com fingerprint, médias, p50/p95 e utilização |
| [`src/results`](../src/results/README.md) | Exportação JSON e CSV (`os-llm-run` v2) |
| [`src/workload`](../src/workload/README.md) | Gerador de cargas com seed (CSV v2) |
| [`src/concurrency`](../src/concurrency/README.md) | Pool pthread, fila com mutex e semáforos, adaptador pthread da sessão e grafo de deadlock |
| [`src/ipc`](../src/ipc/README.md) | Mensagens enquadradas por pipe entre processos |

## Programas

- **CLI** (`os_llm_cli`): lê o cenário, roda a sessão e imprime ou exporta o resultado. `os_llm_workload` gera cenários.
- **Painel** (`apps/visual`): servidor Python (biblioteca padrão) em `127.0.0.1`. Para cada execução, chama a CLI por subprocesso e devolve o JSON exportado; o JavaScript só exibe. Não altera o estado do motor.
- **Laboratórios**: `os_llm_race_lab` (atualização perdida) e `os_llm_deadlock_lab` (mutexes em ordem global ou cruzada) rodam em subprocesso com watchdog. `os_llm_ipc_demo` e `os_llm_ipc_runtime` usam `fork` e pipes; `os_llm_signal_demo` trata SIGUSR1. Os de race e deadlock não usam o núcleo.
- **Experimentos**: `tools/experiments/run.py` executa E1–E4 e `tools/analysis/report.py` valida e gera tabelas e gráficos. Ver [EXPERIMENTS.md](EXPERIMENTS.md).

## Fluxo de um tick

`os_session_step_with_executor`, em `src/runtime/session.c`:

1. Acorda os bloqueados cujo I/O terminou e admite as chegadas do tick.
2. Para cada worker livre, o escalonador escolhe um PCB READY. Se não há trabalho, o relógio salta para a próxima chegada ou o próximo wakeup e repete.
3. Com memória ativa, cada worker acessa a página `processed_tokens / tokens_per_page`. Uma falta com latência bloqueia o PCB e libera o worker.
4. No modo pthread, o coordenador entrega um token por worker às threads e espera um ACK por pedido. No modo virtual, esse passo não existe.
5. O relógio avança 1 tick e admite as chegadas desse tick.
6. Cada worker que pôde executar conclui 1 token. O PCB termina (e libera suas páginas), volta a READY se o quantum do RR acabou, ou continua.

Só o coordenador altera PCBs, frames e relógio. Observadores recebem eventos síncronos, e a mesma entrada gera sempre o mesmo trace.
