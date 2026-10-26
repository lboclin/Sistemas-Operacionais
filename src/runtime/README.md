# Runtime virtual

Workers **lógicos**, custo fixo de 1 tick/token e despacho de FCFS/SJF/RR/prioridade. `os_virtual_run_observed` emite eventos síncronos; `os_virtual_run` continua sem observer.

A configuração pode habilitar frames online opcionais. Há stepping e snapshots por meio da API opaca `session.h` (`os_session_create`, `os_session_step`, `os_session_snapshot`, `os_session_run`, `os_session_destroy`). A sessão copia os PCBs; snapshots são consultados entre passos e buffers insuficientes são rejeitados sem escrita parcial. O motor é single-threaded e um passo pode saltar ticks ociosos. Não há pthread nem persistência de sessão. A latência de falta de página é opcional (`page_fault_latency_ticks`): quando positiva, a requisição entra em BLOCKED, libera o worker e volta a READY no wakeup. O snapshot informa o próximo wakeup e a API `os_session_request_wakeup` consulta o prazo por ID. Sem latência, o comportamento anterior é preservado. A reserva/pinning de páginas ainda não está implementada. Ver [contratos](../../docs/CONTRACTS.md).


Traces observados possuem `sequence` por sessão. `os_trace_verify_replay` reexecuta a simulação para comparar eventos, retornando o índice da primeira divergência; não infere PCBs a partir do log. A ordem de eventos no mesmo tick segue os pontos de emissão do runtime e a ordem do cenário/workers.
