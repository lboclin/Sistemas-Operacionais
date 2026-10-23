# Glossário

- **PCB:** registro lógico de controle da requisição, não necessariamente um processo do kernel.
- **NEW/READY/RUNNING/BLOCKED/TERMINATED:** estados de ciclo de vida.
- **Worker:** slot lógico ou thread real, conforme o runtime.
- **Quantum:** cota de trabalho/tokens no Round Robin.
- **FCFS/SJF/RR:** First Come First Served, Shortest Job First e Round Robin.
- **Aging:** aumento gradual de prioridade durante a espera.
- **KV cache:** cache de atenção de LLM; a política de memória neste projeto é simulada para fins didáticos.
- **Frame/bloco:** unidade fixa de memória simulada.
- **Page fault:** referência a página ausente dos frames.
- **FIFO/LRU/OPT:** políticas de substituição de páginas, sendo Ótimo um oráculo de futuro.
- **Tempo virtual:** relógio lógico do simulador.
- **Wall time:** tempo real de execução.
- **IPC:** comunicação entre processos via mecanismo POSIX a decidir.
- **ADR:** registro de decisão arquitetural.
