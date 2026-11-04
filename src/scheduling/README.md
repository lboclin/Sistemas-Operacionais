# Escalonamento

O scheduler seleciona PCBs READY por FCFS, SJF, Round Robin (RR) e prioridade com aging.

O simulador modela um servidor do Jev (TypeSafe AI). O Jev devolve uma resposta curta e de tamanho fixo, então o custo de uma requisição é o tamanho da entrada (`total_tokens`, em tokens de entrada), conhecido na chegada. Por isso o **SJF ordena pelo tamanho real da entrada**, sem previsão. Empates usam `ready_order` e ID.

FCFS, SJF e prioridade são não preemptivos; RR preempta ao fim do quantum. A prioridade com aging soma um bônus a cada `aging_interval` ticks de espera.

As variantes antigas (SJF estimado por `predicted_tokens`, SJF-oráculo, SJF por média exponencial e o preditor offline) foram removidas pela [ADR 0002](../../docs/adr/0002-foco-typesafe-jev.md).

Exemplo: `./build/os_llm_cli --policy sjf --scenario scenarios/sample.csv`.
