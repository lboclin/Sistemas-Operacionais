# Guia de execução

O projeto demonstra escalonamento, memória, concorrência e IPC com um simulador C11. Os tempos são ticks virtuais; não há inferência de modelos de linguagem.

## Requisitos

- Linux, CMake 3.16+, compilador C11, pthreads e Python 3.
- Node.js é necessário apenas para os testes JavaScript.

## Compilar e verificar

```bash
git clone https://github.com/lboclin/Sistemas-Operacionais.git
cd Sistemas-Operacionais
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Executar exemplos

```bash
./build/os_llm_cli --policy fcfs --scenario scenarios/sample.csv --trace
./build/os_llm_cli --policy rr --quantum 1 --workers 2 --scenario scenarios/sample.csv
./build/os_llm_cli --policy ema --ema-alpha 0.5 --ema-initial 6 --scenario scenarios/ema-demo.csv
python3 apps/visual/server.py --cli ./build/os_llm_cli --port 8765
```

Com o servidor ativo, abra `http://127.0.0.1:8765`. Consulte [a documentação do painel](../apps/visual/README.md) e [as ferramentas de experimentos](EXPERIMENTS.md) para mais opções.

## Limites

Os resultados não representam latência ou throughput de inferência real. Os laboratórios POSIX são demonstrações isoladas; não há serviço IPC persistente nem detector de deadlock integrado ao runtime principal.
