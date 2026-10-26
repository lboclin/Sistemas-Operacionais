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

Ainda **não** há integração do pool com a fila ou com o escalonador, nem
proteção do KV cache entre threads; isso pertence ao commit 11.

Testes: `ctest --test-dir build -R '^(thread_pool|work_queue)
 --output-on-failure`.
O teste de fila inclui 4 produtores, 4 consumidores e 4000 itens, verificando
que cada ID é consumido exatamente uma vez, além de fechamento durante
espera em fila vazia/cheia.
