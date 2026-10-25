# Estado da implementação

> Atualizado em 25/10/2026. Este documento descreve o código presente na branch `main`; não representa a conclusão da proposta acadêmica.

## Entregue nesta etapa
| Funcionalidade | Estado | Limites atuais |
|---|---|---|
| F01 PCB e estados | Parcial | Estruturas e transições; não existe gerenciador completo de filas |
| F03 FCFS | Parcial | Seleção não preemptiva no runtime virtual |
| F04 SJF | Parcial | estimativa fornecida e oráculo; média exponencial e IA pendentes |
| F05 Round Robin | Parcial | quantum em tokens, worker virtual |
| F06 Prioridade com aging | Parcial | aging por intervalo, decisão em dispatch |
| F07 Workers | Parcial | múltiplos workers virtuais; pthreads pendentes |
| F10/F11 KV/paginação | Parcial | replay independente FIFO/LRU/OPT; não integrado ao runtime |
| F16 Métricas | Parcial | médias de espera, resposta, retorno, throughput e makespan |
| F20 Testes | Parcial | seis casos CTest, ainda sem cobertura dos módulos POSIX |

## Contratos reais de código

- `os_request_init`: cria PCB lógico não iniciado, valida tokens positivos.
- `os_request_transition`: aceita apenas transições válidas; proíbe reativar terminado.
- `os_scheduler_select`: escolhe entre PCBs READY; desempate determinístico.
- `os_virtual_run`: executa tokens de requisições com chegada lógica, workers e política; altera a coleção fornecida pelo chamador.
- `os_metrics_summarize`: resume requisições terminadas.
- `os_pager_replay`: repete referências a páginas para FIFO, LRU e OPT em memória simulada (sem acoplamento com `os_virtual_run`).

## Hipóteses explícitas

- Cada token custa um tick virtual; os algoritmos SJF são não preemptivos, e RR é preemptivo por quantum.
- Prioridade numérica maior é mais importante; o aging aumenta a prioridade efetiva do PCB que aguarda.
- As requisições são processos **lógicos**, não processos Linux reais.
- A página é identificada pelo par (`request_id`, `page_id`). O OPT conhece o futuro do trace e não é uma política online.
- A CLI CSV tem limites fixos e não é API pública. A interface gráfica e protocolo ainda não foram escolhidos.
- `os_virtual_run` não permite retomada/stepping/snapshot; a implementação atual roda até terminar.
- Cenários muito grandes são caros no runtime token a token. Não comparar ticks virtuais com medições de desempenho reais.

## Validação nesta sessão

Local: GCC 14.2, CMake + CTest, Debug e Release, 6/6 testes. Uma execução com sanitizers ASan/UBSan também passou os 6 testes. Isso **não** comprova ausência de erros nem substitui CI em outros ambientes. O workflow GitHub, quando acionado, verificará compilação e testes de PRs.

## Próximas frentes

1. Eventos e snapshots do motor para suporte a interface gráfica.
2. Gateway formal, cargas versionadas e validação robusta dos cenários.
3. Modelo de frames KV integrado ao runtime e page faults.
4. Pool real pthreads + mutex/semafóros; laboratório isolado de corrida e deadlock.
5. IPC POSIX e `SIGUSR1` em handler seguro.
6. Predição offline e comparação dos quatro SJF da proposta, experimentos e métricas p50/p95.
7. Interface gráfica web ou desktop após escolha do grupo.
