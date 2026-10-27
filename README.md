# Simulador de sistemas operacionais para cargas de LLM

Projeto acadêmico em C que simula o escalonamento de requisições de geração de texto. Cada token produzido avança um tick virtual. O programa não carrega nem executa um modelo de linguagem; os tempos e métricas representam apenas a simulação.

## O que o simulador faz

- Compara FCFS, SJF estimado, SJF oráculo, Round Robin e prioridade com aging.
- Simula workers, sessões passo a passo e eventos de execução.
- Modela memória de requisições com paginação FIFO ou LRU e latência opcional de page fault.
- Calcula espera, resposta, turnaround, utilização dos workers e percentis.
- Gera cenários reproduzíveis e exporta resultados em JSON ou CSV.
- Inclui laboratórios POSIX independentes para pthreads, filas, deadlock e comunicação por pipes.

O núcleo e as ferramentas de linha de comando estão implementados. A interface gráfica e a execução de inferência real ainda não fazem parte do projeto.

## Compilar

Requer CMake 3.16 ou superior e um compilador C11. No Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Fazer uma simulação

```bash
./build/os_llm_cli
```

O comando executa um cenário de exemplo e mostra o estado de cada requisição e um resumo das métricas. Para escolher a política, o quantum do Round Robin ou o número de workers:

```bash
./build/os_llm_cli --policy rr --quantum 2 --workers 2
./build/os_llm_cli --policy priority
./build/os_llm_cli --help
```

## Usar um cenário CSV

O formato básico tem cinco colunas: `id,arrival_tick,total_tokens,predicted_tokens,priority`.

```csv
1,0,8,6,2
2,3,4,4,1
```

Salve os dados em um arquivo e passe o caminho com `--scenario`:

```bash
./build/os_llm_cli --scenario scenarios/sample.csv
```

O simulador aceita até 256 requisições. Também é possível gerar um cenário com seed fixa:

```bash
./build/os_llm_workload --seed 42 --count 4 --distribution uniform \
  --arrival-span 10 --min-tokens 2 --max-tokens 8 --output scenario.csv
./build/os_llm_cli --scenario scenario.csv
```

As distribuições disponíveis são `uniform`, `burst` e `staggered`.

## Inspecionar e exportar

Use `--steps` e `--snapshots` para acompanhar a sessão, `--trace` para listar eventos e `--frames` para habilitar a simulação de memória:

```bash
./build/os_llm_cli --policy rr --quantum 1 --steps 3 --snapshots
./build/os_llm_cli --frames 2 --page-tokens 2 --page-policy lru --trace
```

Para salvar os resultados:

```bash
./build/os_llm_cli --scenario scenarios/sample.csv \
  --export-json run.json --export-csv run.csv
```

Os ticks são virtuais; não correspondem à latência de um serviço de LLM.

## Executar os testes

```bash
ctest --test-dir build --output-on-failure
```

O conjunto inclui 47 testes CTest. A matriz de compilação está disponível nas [execuções do GitHub Actions](https://github.com/lboclin/Sistemas-Operacionais/actions).

## Documentação

- [Estado atual da implementação](docs/IMPLEMENTATION_STATUS.md)
- [Especificação funcional](docs/FUNCTIONAL_SPEC.md)
- [Contratos entre módulos](docs/CONTRACTS.md)
- [Estratégia de testes](docs/TEST_PLAN.md)
- [Como contribuir](CONTRIBUTING.md)
