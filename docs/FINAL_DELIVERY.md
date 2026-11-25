# Guia de execução

O projeto demonstra escalonamento (FCFS, SJF, RR, prioridade com aging e HRRN), memória, concorrência e IPC com um simulador C11 de um servidor do Jev (TypeSafe AI). Os tempos são ticks virtuais: 1 tick = 1 token de entrada. O simulador não chama o Jev.

## Requisitos

- Linux, CMake 3.16+, compilador C11, pthreads e Python 3.
- Node.js é necessário apenas para os testes JavaScript.
- O relatório do CHK3 está no [README](../README.md), os resultados em [results/chk3](../results/chk3/SUMMARY.md) e o roteiro da demonstração em [DEMO.md](DEMO.md).

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
./build/os_llm_cli --policy sjf --scenario scenarios/seed42-v2.csv
./build/os_llm_cli --policy hrrn --scenario scenarios/starvation.csv --starvation-threshold 320
./build/os_llm_cli --policy priority --aging-interval 0 --scenario scenarios/starvation.csv
./build/os_llm_ipc_runtime --fcfs   # gateway e motor em processos separados, ligados por pipes (Linux)
python3 apps/visual/server.py --cli ./build/os_llm_cli --port 8765
```

Com o servidor ativo, abra `http://127.0.0.1:8765`. Consulte [a documentação do painel](../apps/visual/README.md) e [as ferramentas de experimentos](EXPERIMENTS.md) para mais opções.

## Reproduzir os experimentos do CHK3

```bash
tools/experiments/chk3.sh build /tmp/chk3
```

O script recria as 11 cargas, os relatórios e o agregado em `/tmp/chk3`. Ver [EXPERIMENTS.md](EXPERIMENTS.md).

Os testes novos (`visual_app_js`, `starvation_scenario_py` e os de aging) estão descritos em [TEST_PLAN.md](TEST_PLAN.md).

## Limites

Os resultados não representam latência ou throughput reais do Jev. Os laboratórios POSIX são demonstrações isoladas; não há serviço IPC persistente nem detector de deadlock integrado ao runtime principal. Veja a lista completa em [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md).
