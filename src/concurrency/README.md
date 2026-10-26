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

Este primeiro contrato **não** implementa fila de tarefas, semáforos, mutex
de recursos compartilhados, escalonamento ou KV cache entre threads. A fila
e a sincronização são o próximo commit; integração com o simulador vem depois.

Teste: `ctest --test-dir build -R '^thread_pool$' --output-on-failure`.
