# Interface gráfica — planejamento

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
