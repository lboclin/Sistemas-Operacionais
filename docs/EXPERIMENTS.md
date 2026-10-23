# Experimentos previstos

> Planejamento, sem resultados nem benchmarks executados.

## Comparabilidade
Fixar o cenário, seeds, número de workers, capacidade e política de memória e modelo de custo ao comparar escalonadores. Diferenciar tempo simulado e tempo real. Registrar hardware quando aplicável.

## Quatro blocos previstos
1. **Condições de corrida**: modo protegido e modo inseguro isolado com carga crescente; reportar atualizações perdidas observadas, sem assumir que uma corrida sempre acontece.
2. **Escalonamento**: FCFS, SJF média exponencial, SJF preditivo, SJF oráculo, RR com quantums distintos, prioridade com aging.
3. **Memória**: variar capacidade, FIFO, LRU e Ótimo; faltas de página, fragmentação e bloqueios.
4. **Deadlock**: reproduzir um impasse, detectar e prevenir pela ordem global dos recursos.

## Métricas
Média/p50/p95 de espera, resposta e retorno, throughput, maior espera/starvation, utilização dos workers, page faults e fragmentação interna.

Exportar configurações, origem da carga, versão, definições de métricas e ambiente junto aos resultados. Repetir execuções com threads reais para observar a variabilidade.
