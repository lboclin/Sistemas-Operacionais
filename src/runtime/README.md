# Runtime virtual

Workers **lógicos**, custo fixo de 1 tick/token e despacho de FCFS/SJF/RR/prioridade. `os_virtual_run_observed` emite eventos síncronos; `os_virtual_run` continua sem observer.

A configuração pode habilitar frames online opcionais. Não há pthread, stepping, persistência de replay nem penalidade temporal por faltas de página. Ver [contratos](../../docs/CONTRACTS.md).
