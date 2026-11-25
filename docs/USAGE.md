# Guia de uso

Simulador acadêmico em C11 de um servidor que atende requisições do Jev, o modelo de decisão da TypeSafe AI. O Jev devolve uma resposta curta e de tamanho fixo (categoria, nota ou probabilidade), então o custo de cada requisição é o tamanho da **entrada**, conhecido na chegada. **1 tick virtual = 1 token de entrada processado.** O programa não executa nem consulta o Jev. A decisão está em [ADR 0002](adr/0002-foco-typesafe-jev.md).

## O que faz

- Compara FCFS, SJF (pelo tamanho da entrada, sem previsão), Round Robin, prioridade com aging e HRRN.
- Simula workers, memória paginada FIFO/LRU, métricas, traces e reprodução de execuções.
- Gera workloads reproduzíveis e exporta resultados em JSON e CSV.
- Oferece um painel web local e laboratórios de concorrência, deadlock, sinais (SIGUSR1) e IPC por pipes (gateway → motor em processos separados).

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

Use `./build/os_llm_cli --help` para ver as opções, ou passe um cenário CSV com `--scenario caminho/arquivo.csv`. Cada linha do CSV (formato v2) é `id,arrival_tick,input_tokens,priority`; veja [scenarios/README.md](../scenarios/README.md).

Para os experimentos, veja [EXPERIMENTS.md](EXPERIMENTS.md); para a demonstração, [DEMO.md](DEMO.md).

## Documentação

- [Índice da documentação](README.md)
- [Interface web](../apps/visual/README.md)
- [Estado da implementação](IMPLEMENTATION_STATUS.md)
- [Plano de testes](TEST_PLAN.md)
- [ADR 0002: foco no Jev](adr/0002-foco-typesafe-jev.md)
- [Especificação funcional (histórico)](historico/FUNCTIONAL_SPEC.md)
- [Como contribuir](../CONTRIBUTING.md)
