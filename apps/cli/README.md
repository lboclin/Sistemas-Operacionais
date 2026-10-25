# CLI de testes

Interface auxiliar: não é a GUI principal do projeto.

```bash
./build/os_llm_cli --policy rr --quantum 2 --workers 2
./build/os_llm_cli --scenario scenarios/sample.csv
./build/os_llm_cli --frames 3 --page-tokens 2 --page-policy fifo --trace
```

`--frames` habilita paginação FIFO/LRU; `--trace` mostra eventos. `--help` contém o restante das opções. OPT online e interface gráfica ainda não existem.
