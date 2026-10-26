# Laboratório de concorrência POSIX

O núcleo virtual (`os_core`) continua determinístico e **não cria threads**.
O alvo separado `os_posix` introduz o ciclo de vida de workers pthread:

- `os_pool_start` inicia 1–4096 threads reais, com índice estável e callback
  por worker; se uma criação falhar, pede parada e faz join das anteriores.
- `os_pool_request_stop` solicita parada cooperativa e acorda os callbacks
  bloqueados em `os_pool_wait_stop`. É idempotente.
- `os_pool_shutdown` solicita parada, faz join e libera os recursos. Só pode
  ser chamado por um controlador **fora** do pool; callbacks que ignoram a
  parada podem impedir o join. Não usa `pthread_cancel`.
- `os_pool_stop_requested` permite polling cooperativo quando o callback
  executa trabalho entre checkpoints.

O módulo `work_queue.{h,c}` implementa agora uma **fila FIFO limitada**
protegida por mutex e semáforos POSIX para vagas/itens. `os_queue_push` e
`os_queue_pop` bloqueiam sem busy-wait; `os_queue_close` interrompe
produtores, acorda bloqueados e permite drenar os itens existentes. A fila
transporta apenas valores `request_id/token_budget`, sem possuir PCBs.
`os_queue_destroy` exige que todos os produtores e consumidores já tenham
terminado. Fechamento é cancelamento **cooperativo**, sem `pthread_cancel`.

O adaptador `parallel_runtime.{h,c}` conecta o motor de sessões ao
pool e à fila: o coordenador executa admissão, escalonamento, acesso ao cache
KV, eventos e métricas; cada tarefa de token é entregue a uma pthread, e o
coordenador só confirma o avanço após receber exatamente um ACK por pedido.
Um mutex e uma condição protegem os ACKs, e há timeout de 15 segundos por
lote. Não há acesso concorrente a PCBs, frames ou contadores, nem inferência
LLM real; a execução POSIX é um **laboratório de concorrência**, não um
acelerador de inferência. O caminho virtual padrão permanece inalterado.
Uma falha de executor invalida a sessão e não admite retry na mesma sessão.

Teste: `ctest --test-dir build -R '^(thread_pool|work_queue|parallel_runtime)
 --output-on-failure`.
O teste de fila inclui 4 produtores, 4 consumidores e 4000 itens, verificando
que cada ID é consumido exatamente uma vez, além de fechamento durante
espera em fila vazia/cheia. O teste de integração compara PCBs, métricas e
traces do adaptador com o motor virtual para FCFS, RR, prioridade, paging e
page-fault latency. Os laboratórios de race e deadlock seguem pendentes.

## Demonstração de condição de corrida (commit 12)

`os_llm_race_lab safe|unsafe` inicia um processo filho via `fork/exec`,
executa duas pthreads e impõe timeout de 3 segundos no pai, com `SIGKILL`
no filho travado. O modo `unsafe` força a perda de uma atualização por
`atomic_load` + `atomic_store` separados; o modo `safe` usa
`atomic_fetch_add`. Ambos são livres de **data race indefinida em C**:
este laboratório demonstra a condição de corrida lógica (lost update), não
acesso não atômico a memória compartilhada. A barreira torna o resultado
reproduzível: `unsafe` observa 1 em vez de 2, `safe` observa 2.
O executável é independente do motor e os dois cenários têm testes CTest.

## Detecção de deadlock (commit 13)

`deadlock.{h,c}` mantém um grafo de recursos exclusivos: cada recurso tem
zero ou um proprietário e cada processo espera no máximo um recurso. A aresta
de espera `processo → proprietário` é derivada, não duplicada em outro estado.
`os_deadlock_find_cycle` devolve uma testemunha determinística de ciclo
(inclusive autoespera), ou comprimento zero quando não há ciclo. Não bloqueia
threads nem tenta adquirir recursos reais; todas as mutações são chamadas por
um único coordenador. Limite didático: 128 processos e 128 recursos. Testes
cobrem ciclos de três, autoespera, remoção de proprietário, cadeias sem ciclo,
entradas inválidas e consulta de capacidade sem escrita parcial.

## Prevenção e recuperação controlada (commit 14)

`os_deadlock_acquire_ordered` só concede recurso livre com índice crescente
em relação aos já possuídos pelo processo. Não bloqueia nem cria aresta de
espera: em conflito, o chamador deve tentar novamente após liberação.
`os_deadlock_recover_one` detecta um ciclo existente, escolhe como vítima o
processo de maior índice no ciclo, limpa sua espera e libera seus recursos.
Não usa `pthread_cancel` nem tenta desbloquear mutexes reais de terceiros.

O teste `deadlock_liveness` cobre prevenção, rejeição por ordem/conflito e
1000 ciclos de deadlock sintético com recuperação e progresso posterior. O
CTest impõe watchdog de 5 segundos. É um modelo didático de grafo de recursos,
não um gerenciador de locks do SO, nem prova ausência de deadlock em sistemas
arbitrários com múltiplas esperas simultâneas.
