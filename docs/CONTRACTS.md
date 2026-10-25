# Contratos entre módulos — rascunho (M0)

> Documento de planejamento. Tipos, serialização e linguagem ainda não definidos.

## Princípios
- Identificadores de requisição e execução têm escopos definidos.
- O motor é o único responsável por alterar estados do PCB.
- Aplicações gráficas e CLI enviam comandos e recebem eventos/snapshots; nunca acessam estruturas internas.
- O escalonador escolhe candidatos elegíveis, sem controlar memória ou workers.
- O gerenciador de memória é proprietário dos blocos e das operações de reserva/liberação.
- Métricas registram a base de tempo usada para permitir comparações corretas.

## Contratos planejados

| Contrato | Campos conceituais | Responsável | Consumidores |
|---|---|---|---|
| `Request/PCB` | ID, prioridade, chegada, tokens, progresso, estado, páginas | core | gateway, scheduling, runtime, memory |
| `Scenario` | ID, versão, configuração, requisições, seed, limites | workload | app, experiments, UI |
| `Command` | ação, argumentos e ID da execução | app | aplicações |
| `Event` | tipo, request ID, timestamp, clock, payload versionado | metrics | UI, relatórios |
| `Snapshot` | filas, workers, PCBs, páginas, contadores, capacidades | app | UI, CLI |
| `RunResult` | cenário, política, runtime, métricas, proveniência | experiments | UI, análise |

## Operações conceituais
`load_scenario`, `submit_request`, `select_policy`, `start`, `pause`, `resume`, `stop`, `snapshot`, `export_result`.

O comando `step` exige capacidade específica do runtime; não supor suporte em threads reais.

## Eventos conceituais
`request_arrived`, `request_ready`, `request_dispatched`, `quantum_expired`, `request_blocked`, `request_unblocked`, `page_fault`, `page_evicted`, `request_completed`, `run_finished`, `run_failed`.

## Invariantes a fechar antes da implementação
1. Estados e transições do PCB; identificadores únicos e um só dono do estado.
2. Unidades e semântica de relógio virtual versus tempo de parede.
3. Definições de burst, quantum, custo de token, frame e bloco.
4. Erros, compatibilidade e versionamento de comandos/eventos.
5. Capacidade de pausa, stepping e shutdown por modo de execução.
6. Protocolo de integração com a UI após escolher a stack.

Ver [especificação funcional](FUNCTIONAL_SPEC.md).

## APIs concretas implementadas em C11 (25/10/2026)

As definições acima são contratos de **planejamento** para futuras interfaces. O núcleo C atualmente disponibiliza:

- `os_gateway_validate(requests, count)`: valida requisições NEW com IDs distintos.
- `os_gateway_admit(request, now, order)`: admite uma requisição chegada (NEW → READY).
- `os_gateway_next_arrival(requests, count)`: informa a próxima chegada pendente.
- `os_virtual_run(...)`: executa o cenário virtual até concluir.
- `os_virtual_run_observed(..., callback, context)`: executa o mesmo cenário e entrega eventos síncronos para observação.
- `os_frames_init/access/release/destroy`: gerencia frames online FIFO/LRU; `os_pager_replay` aceita FIFO/LRU/OPT com trace offline completo.

### Semântica dos eventos e frames

Cada `OsEvent` contém tipo, tick **virtual**, request ID, worker e página quando aplicável. `SIZE_MAX` sinaliza evento sem worker. Os eventos de página são `page_hit`, `page_fault` e `page_evicted`. O callback recebe um ponteiro temporário; não deve alterar os PCBs nem chamar novamente o simulador.

A paginação online é **opcional**: `memory_enabled=false` mantém a execução CPU-only. Quando ativada, `frame_capacity>0` e `tokens_per_page>0`; cada token acessa a página `generated_tokens/tokens_per_page`. A capacidade é compartilhada pelos workers e páginas residentes são liberadas na conclusão do PCB.

**Ainda não suportado:** espera/bloqueio e custo adicional em faltas de página, execução POSIX real, API `step`, snapshots interativos, streaming de rede e política OPT online. O modelo de KV cache é didático e não representa blocos KV reais de um modelo.
