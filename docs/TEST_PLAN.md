# Plano de testes

O CTest cobre módulos C, integração do runtime, argumentos e saídas da CLI, exportação, memória, scheduler, concorrência e IPC. Testes auxiliares cobrem workloads, análise de resultados e componentes do painel web.

## Executar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Os testes de aceitação do painel podem ser executados com:

```bash
OSLLM_CLI="$PWD/build/os_llm_cli" python3 -m unittest discover -s tests/visual -p 'test_acceptance.py' -v
node --test tests/visual/test_timeline.cjs tests/visual/test_reports.cjs
```

A suíte verifica resultados simulados e determinismo; não mede inferência real de modelos nem substitui a inspeção manual da interface.
