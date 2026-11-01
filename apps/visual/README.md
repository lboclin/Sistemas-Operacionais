# OS-LLM Studio — Interface gráfica (Bloco I25)

MVP **web local** com Python (biblioteca padrão) e HTML/CSS/JavaScript, sem npm, CDN, API paga nem inferência LLM real. O servidor é um adaptador para o **motor C11 existente**; não reimplementa o escalonador.

## Iniciar no Ubuntu

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
python3 apps/visual/server.py --cli ./build/os_llm_cli --port 8765
```

Abrir **http://127.0.0.1:8765** no navegador da própria máquina. `--cli` recebe somente o caminho do executável **na linha de comando do administrador**, nunca de uma requisição HTTP. O servidor sempre escuta em `127.0.0.1` (não disponibiliza a UI na rede). CTRL+C encerra o servidor.

## Funcionalidades concluídas no I25

- Criar, editar e remover PCBs; importar cenários CSV de 5 colunas (incluindo cabeçalho `os-llm-scenario v1`), restaurar preset.
- FCFS, SJF estimado, SJF-EMA (`alpha=0.5`/estimativa inicial 6), SJF-oráculo, RR e prioridade com aging. Configurar quantidade de workers (1–16) e quantum RR.
- Executar a CLI de verdade, recebendo sua exportação JSON. Exibir métricas calculadas e estado final dos PCBs. Tudo em **ticks virtuais** e sem números fabricados.
- Proteções: bind de loopback, verificação de Host/Origin, assets em allowlist (sem acesso ao sistema de arquivos), CSP, limite de 64 KiB por solicitação, até 256 PCBs, numeração validada com limite de 64 bits para chegada, subprocess sem shell e timeout de 25 segundos, export máximo de 16 MiB. Erros são enviados sem revelar caminhos privados.

**Ainda não implementado (não simular):** Gantt/replay visual (I26), painel de memória/concorrência (I27), comparação/relatórios visuais (I28), pausa persistente de um processo real. No I25, cada clique executa uma nova simulação até seu término.

## Testes

```bash
python3 -m unittest discover -s tests/visual -p 'test_server.py' -v
# com binário compilado, habilite também o teste ponta a ponta:
OSLLM_CLI="$PWD/build/os_llm_cli" python3 -m unittest discover -s tests/visual -p 'test_server.py' -v
ctest --test-dir build --output-on-failure
```

CMake registra `visual_api_py` automaticamente quando houver Python. O teste de mock verifica contratos e segurança; apenas o teste `test_real_compiled_simulator` exercita o binário do projeto. A UI não depende de backends remotos nem persiste cenários.
