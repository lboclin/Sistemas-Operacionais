# Plano de testes

O CTest registra 70 testes quando Node e Python estão instalados. Ele cobre módulos C, integração do runtime, argumentos e saídas da CLI, exportação, memória, scheduler, concorrência e IPC. Testes auxiliares cobrem workloads, análise de resultados e componentes do painel web.

## Testes adicionados no CHK3

| Teste | O que verifica |
| --- | --- |
| `core` (HRRN) | Regra da maior razão, desempate por `ready_order` e ID, e casos que estourariam 64 bits ou perderiam precisão em `double` |
| `virtual` (HRRN) | Ordem de execução diferente de FCFS e de SJF, sem preempção |
| `starvation_scenario_py` | Pedido longo espera 796 ticks no SJF e 120 no HRRN; aging evita a starvation; HRRN igual a um oráculo em frações exatas |
| `core_stress_seeded_py` | `--aging-interval` exportado, padrão 2 e 0 desativando o aging; HRRN incluído na matriz de políticas |
| `cli_invalid_aging_*`, `cli_missing_aging` | Valores inválidos de `--aging-interval` são rejeitados |
| `visual_app_js` | Validação do formulário e importação de CSV no painel, com prioridade em [-2^31, 2^31-1] |
| `experiments_runner_py`, `analysis_report_py` | HRRN, aging, limite de starvation e latência de falta nos experimentos; agregação por seed |

## Executar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Os testes de aceitação do painel podem ser executados com:

```bash
OSLLM_CLI="$PWD/build/os_llm_cli" python3 -m unittest discover -s tests/visual -p 'test_acceptance.py' -v
node --test tests/visual/test_timeline.cjs tests/visual/test_reports.cjs tests/visual/test_app.cjs
```

Para rodar só os testes novos:

```bash
ctest --test-dir build -R '^(core|virtual|starvation_scenario_py|core_stress_seeded_py|cli_invalid_aging.*|cli_missing_aging|visual_app_js)$' --output-on-failure
```

O teste de navegador (`tests/visual/test_browser_e2e.py`) não faz parte do CTest nem do CI. Ele requer Playwright com Chromium e os binários compilados:

```bash
OSLLM_CLI="$PWD/build/os_llm_cli" OSLLM_RACE="$PWD/build/os_llm_race_lab" \
OSLLM_DEADLOCK="$PWD/build/os_llm_deadlock_lab" \
python3 -m unittest discover -s tests/visual -p 'test_browser_e2e.py' -v
```

Os links dos documentos são verificados com `python3 tools/check_links.py`.

A suíte verifica resultados simulados e determinismo; não mede inferência real de modelos nem substitui a inspeção manual da interface.
