# Métricas e eventos

`summary.c` calcula médias de espera, resposta e retorno, makespan e throughput **virtuais**.

`events.c` fornece nomes de eventos de chegada, despacho, preempção, conclusão, page hit, page fault e evicção. `OsRunSummary` inclui estatísticas de frames. Observadores são callbacks locais síncronos e não um serviço web.

`trace.h` oferece captura dinâmica, igualdade por campos e fingerprint estável FNV-1a64 (versão 1, little-endian, não criptográfico). O callback `os_trace_capture` marca `failed` em erro de alocação. A verificação de replay fica em `src/runtime/replay.h`: reexecuta a sessão com entradas frescas e compara eventos, sem reconstruir estados ocultos.
