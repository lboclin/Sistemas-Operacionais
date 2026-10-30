# Escalonamento

O scheduler seleciona PCBs READY por FCFS, SJF estimado (campo `predicted_tokens`),
SJF oráculo (comprimento real; somente referência), SJF-EMA, RR e prioridade com aging.

SJF-EMA é não preemptivo: `tau_next = alpha * last_burst + (1-alpha) * tau_previous`,
com `alpha` em [0,1] e `initial_tokens` positivo. Cada prioridade é usada
provisoriamente como classe de histórico (o cenário CSV v1 não tem classe).
Todas começam com a mesma estimativa; somente requisições concluídas
atualizam a classe com `generated_tokens`. Empates usam `ready_order` e ID.
O histórico é por sessão, determinístico e não consulta o comprimento
real de requisições ainda não concluídas. Não há preditor de IA treinado.

Exemplo: `./build/os_llm_cli --policy ema --ema-alpha 0.5 --ema-initial 6 --scenario scenarios/ema-demo.csv`.
