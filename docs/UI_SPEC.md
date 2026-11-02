# Interface gráfica — OS-LLM Studio

> O produto terá uma interface visual, web ou desktop a definir. Uma TUI não foi escolhida como interface principal.

## Objetivo
Permitir configurar requisições de um LLM simulado, visualizar fila, políticas, workers, memória, eventos e métricas. A interface é um adaptador externo ao motor.

| Tela | Interações e informações |
|---|---|
| Dashboard | start/pause/resume/reset e step quando suportado, fila, estado da execução, workers e métricas |
| Requisições | criar, editar, importar e visualizar PCB, prioridade, tokens, chegada e estado |
| Escalonamento | escolher FCFS/SJF/RR/prioridade, configurar quantum e visualizar Gantt |
| Memória | ajustar limite de blocos, FIFO/LRU/Ótimo, página por PCB e faltas |
| Concorrência | observar threads, locks, race conditions e deadlock em laboratório controlado |
| Experimentos | configurar carga, algoritmos, seeds, repetições e execuções |
| Resultados | Gantt, p50/p95, throughput, utilização, page faults, exportações |

## MVP
1. Inserir requisições com valores validados.
2. Selecionar política e configurar workers quando disponível.
3. Executar e inspecionar a fila e o progresso.
4. Inspecionar estado de PCB, resumo de memória e métricas.
5. Reiniciar exatamente a mesma carga.

## UX
Controles rotulados, navegação por teclado, estados de erro/vazio/carregamento, contraste acessível, foco visível, tempo virtual versus tempo real sempre identificado e animações desativáveis. O gráfico não pode modificar a simulação.

## Decisões pendentes
Web local versus desktop, framework, estilo visual, snapshots por polling versus stream e transporte UI↔motor. Não instalar framework no scaffold.

## Decisão do Bloco I25 (2026-11-01)

Escolhida interface **web local** em `apps/visual`: servidor Python stdlib vinculado a `127.0.0.1` e frontend HTML/CSS/JS sem dependências remotas. O endpoint `/api/run` valida até 256 requisições e invoca o executável C via subprocess com argumentos permitidos e timeout; devolve os JSONs reais de `os_llm_cli`. O MVP mostra formulário e métricas/PCBs finais. Não representa o modo de concorrência POSIX como inferência real, não inventa dados e não oferece pausa persistente; Gantt/replay, memória e relatórios ficam para I26–I28. Ver [README de apps/visual](../apps/visual/README.md).

## Implementação Bloco I

Web local em `apps/visual`, sem dependências frontend. I25: cenário/CLI e PCBs. I26: Gantt e replay visual (sem pausa real no motor). I27: eventos e métricas de paginação FIFO/LRU e laboratórios POSIX isolados. I28: comparação pareada de execuções, gráficos e import/export de relatórios H24. Mapeamentos exatos de frames não são inventados. Sem inferência LLM real. Para iniciar, consultar [apps/visual/README.md](../apps/visual/README.md).
