# CLI de testes

Interface auxiliar: não é a GUI principal do projeto.

```bash
./build/os_llm_cli --policy rr --quantum 2 --workers 2
./build/os_llm_cli --scenario scenarios/sample.csv
./build/os_llm_cli --frames 3 --page-tokens 2 --page-policy fifo --trace
```

`--frames` habilita paginação FIFO/LRU; `--trace` mostra eventos. `--help` contém o restante das opções. OPT online e interface gráfica ainda não existem.

## Snapshots da sessão

```bash
./build/os_llm_cli --steps 0 --snapshots
./build/os_llm_cli --policy rr --quantum 1 --steps 3 --snapshots
./build/os_llm_cli --workers 2 --frames 2 --snapshots
```

`--steps N` limita os avanços da sessão; `--snapshots` exibe o estado inicial e o estado após cada passo. Um passo pode saltar ticks ociosos. O relatório final distingue `paused` e `completed`. Não há persistência nem retomada entre processos.
