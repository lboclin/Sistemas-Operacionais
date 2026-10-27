# Testes de concorrência

A suíte CTest cobre o pool pthread (`thread_pool`), fila POSIX com
4 produtores × 4 consumidores e 4000 itens (`work_queue`), adaptador
de sessões com ACKs por token (`parallel_runtime`), laboratório
isolado safe/unsafe (`race_safe`, `race_unsafe`) e grafo didático
de deadlock (`deadlock_graph`, `deadlock_liveness`).

```bash
ctest --test-dir build -R '^(thread_pool|work_queue|parallel_runtime|race_safe|race_unsafe|deadlock_graph|deadlock_liveness)$' --output-on-failure
```

O teste de liveness executa 1000 ciclos sintéticos com recuperação e
progresso posterior; CTest limita a 5s. O executável de corrida usa
subprocesso e watchdog de 3s. A suíte não usa ThreadSanitizer nem reproduz
deadlock real entre mutexes pthread.
