# Sistemas Operacionais

Simulador acadêmico em C11 para estudar o escalonamento de requisições de geração de texto. O programa usa ticks e tokens virtuais; não executa nem consulta modelos de linguagem.

## O que faz

- Compara FCFS, SJF, SJF por média exponencial (EMA), oráculo, Round Robin e prioridade com aging.
- Simula workers, memória paginada FIFO/LRU, métricas, traces e reprodução de execuções.
- Gera workloads reproduzíveis e exporta resultados em JSON e CSV.
- Oferece um painel web local e laboratórios independentes de concorrência e IPC.

## Começar

Requer CMake 3.16+, compilador C11 e Python 3 para o painel web.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Execute uma simulação pela linha de comando:

```bash
./build/os_llm_cli --policy rr --quantum 2 --workers 2
```

Para abrir o painel, inicie o servidor local e acesse `http://127.0.0.1:8765`:

```bash
python3 apps/visual/server.py --cli ./build/os_llm_cli --port 8765
```

Use `./build/os_llm_cli --help` para ver as opções, ou passe um cenário CSV com `--scenario caminho/arquivo.csv`.

## Documentação

- [Interface web](apps/visual/README.md)
- [Especificação funcional](docs/FUNCTIONAL_SPEC.md)
- [Estado da implementação](docs/IMPLEMENTATION_STATUS.md)
- [Plano de testes](docs/TEST_PLAN.md)
- [Como contribuir](CONTRIBUTING.md)
