# Runtime virtual

Workers **lógicos**, custo fixo de 1 tick/token e despacho de FCFS/SJF/RR/prioridade. `os_virtual_run_observed` emite eventos síncronos; `os_virtual_run` continua sem observer.

A configuração pode habilitar frames online opcionais. Há stepping e snapshots por meio da API opaca `session.h` (`os_session_create`, `os_session_step`, `os_session_snapshot`, `os_session_run`, `os_session_destroy`). A sessão copia os PCBs; snapshots são consultados entre passos e buffers insuficientes são rejeitados sem escrita parcial. O motor é single-threaded e um passo pode saltar ticks ociosos. Não há pthread, persistência de sessão nem penalidade temporal por faltas de página. Ver [contratos](../../docs/CONTRACTS.md).
