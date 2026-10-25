# Métricas e eventos

`summary.c` calcula médias de espera, resposta e retorno, makespan e throughput **virtuais**.

`events.c` fornece nomes de eventos de chegada, despacho, preempção, conclusão, page hit, page fault e evicção. `OsRunSummary` inclui estatísticas de frames. Observadores são callbacks locais síncronos e não um serviço web.
