# Experimentos previstos

> Planejamento, sem resultados nem benchmarks executados.

## Comparabilidade
Fixar o cenário, seeds, número de workers, capacidade e política de memória e modelo de custo ao comparar escalonadores. Diferenciar tempo simulado e tempo real. Registrar hardware quando aplicável.

## Quatro blocos previstos
1. **Condições de corrida**: modo protegido e modo inseguro isolado com carga crescente; reportar atualizações perdidas observadas, sem assumir que uma corrida sempre acontece.
2. **Escalonamento**: FCFS, SJF (tamanho da entrada), RR com quantums distintos, prioridade com aging.
3. **Memória**: variar capacidade, FIFO, LRU e Ótimo; faltas de página, fragmentação e bloqueios.
4. **Deadlock**: reproduzir um impasse, detectar e prevenir pela ordem global dos recursos.

## Métricas
Média/p50/p95 de espera, resposta e retorno, throughput, maior espera/starvation, utilização dos workers, page faults e fragmentação interna.

Exportar configurações, origem da carga, versão, definições de métricas e ambiente junto aos resultados. Repetir execuções com threads reais para observar a variabilidade.

## H23 — runner de campanhas

Comandos, resultados e limitações em [tools/experiments/README.md](../tools/experiments/README.md). O runner registra resultados virtuais e observações de laboratórios, sinaliza atividades não implementadas como skipped e não fabrica comparações de modelos reais.

## H24 — Relatórios derivados dos experimentos

Código e instruções em [tools/analysis/README.md](../tools/analysis/README.md). Valida workload, métricas e configuração em cada artefato bruto antes de gerar CSV, Markdown, JSON ou SVG. Os resultados dependem da execução do H23; não há dados reais de LLM embutidos. Um smoke H23→H24 com C binário é registrado no CTest.
