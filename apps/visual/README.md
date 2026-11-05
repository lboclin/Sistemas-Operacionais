# OS-LLM Studio — Interface gráfica (Bloco I25–I28)

MVP **web local** com Python (biblioteca padrão) e HTML/CSS/JavaScript, sem npm, CDN, API paga nem chamadas reais ao Jev. O servidor é um adaptador para o **motor C11 existente**; não reimplementa o escalonador.

## Iniciar no Ubuntu

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
python3 apps/visual/server.py --cli ./build/os_llm_cli --port 8765
```

Abrir **http://127.0.0.1:8765** no navegador da própria máquina. `--cli` recebe somente o caminho do executável **na linha de comando do administrador**, nunca de uma requisição HTTP. O servidor sempre escuta em `127.0.0.1` (não disponibiliza a UI na rede). CTRL+C encerra o servidor.

## Dashboard base (I25)

- Criar, editar e remover PCBs; importar cenários CSV v2 de 4 colunas `id,arrival_tick,input_tokens,priority` (incluindo cabeçalho `os-llm-scenario v2`), restaurar preset.
- FCFS, SJF (pelo tamanho da entrada), RR e prioridade com aging. Configurar quantidade de workers (1–16) e quantum RR.
- Executar a CLI de verdade, recebendo sua exportação JSON. Exibir métricas calculadas (concluídas, espera média, throughput, tempo total e **utilização dos workers**, a fração do tempo em que os workers processaram tokens) e estado final dos PCBs. Tudo em **ticks virtuais** e sem números fabricados.
- Proteções: bind de loopback, verificação de Host/Origin, assets em allowlist (sem acesso ao sistema de arquivos), CSP, limite de 64 KiB por solicitação, até 256 PCBs, numeração validada com limite de 64 bits para chegada, subprocess sem shell e timeout de 25 segundos, export máximo de 16 MiB. Erros são enviados sem revelar caminhos privados.

**Implementado em I26–I28:** Gantt/replay visual, painel de memória/concorrência, comparação e relatórios. **Limitação:** não há pausa persistente de uma simulação em execução; cada clique executa um novo processo do simulador até seu término.

## Testes

```bash
python3 -m unittest discover -s tests/visual -p 'test_server.py' -v
# com binário compilado, habilite também o teste ponta a ponta:
OSLLM_CLI="$PWD/build/os_llm_cli" python3 -m unittest discover -s tests/visual -p 'test_server.py' -v
ctest --test-dir build --output-on-failure
```

CMake registra `visual_api_py` e `final_acceptance_py` quando houver Python. Os testes incluem mock de contrato e integração real com o binário C, todas as políticas, memória, limites, determinismo e HTTP. A UI não depende de backends remotos nem persiste cenários.

## I26 — Gantt e replay visual

O Gantt usa os eventos exportados pelo motor C (sequência, tick, tipo, PCB e worker). Intervalos são construídos somente quando há dispatched e um evento de término/preempção/bloqueio correspondente; intervalos abertos são explicitados, jamais completados artificialmente. Controles de play, pause, step e reset são apenas reprodução visual: não interrompem processos, métricas ou o motor. Limites de precisão de inteiros JavaScript invalidam apenas a visualização. Testes: `node --test tests/visual/test_timeline.cjs` (CTest `visual_timeline_js` se Node estiver disponível).

## I27 — Memória e laboratórios

Painel executa o motor C com `--frames`, `--page-tokens`, FIFO/LRU e `--page-fault-latency`. Somente eventos/métricas realmente exportados são visualizados: não há mapa fictício de frames. OPT é offline. As demos são invocadas em subprocessos isolados por allowlist (`race safe|unsafe`, `deadlock ordered|deadlock`) com watchdog 8s, confirmação dos modos perigosos e caminhos dos executáveis configurados apenas pelo administrador: `--race-lab` e `--deadlock-lab`. Testes Python: `test_memory_lab.py`.

## I28 — Resultados e relatórios

Cada execução concluída é exibida em comparação pareada apenas quando compartilha o mesmo input (ids, chegadas, tokens de entrada e prioridade). Comparação e gráficos usam métricas do JSON original, sem modelos artificiais. O botão Exportar JSON bruto salva o último JSON completo. O botão Importar relatório H24 lê um `report.json` produzido por `tools/analysis/report.py` no navegador sem dar acesso ao servidor a paths arbitrários. O relatório precisa corresponder ao schema `os-llm-report/v1`. A UI distingue ticks virtuais de latência real. Testes JS: `node --test tests/visual/test_reports.cjs` e CTest opcional `visual_reports_js`.

**Precisão dos ticks no navegador:** a CLI continua aceitando uint64_t. A UI bloqueia cenários cujo limite conservador de ticks ultrapassa `Number.MAX_SAFE_INTEGER` (2^53−1), evitando arredondamento silencioso do JSON ao exibir métricas e Gantt. Para execuções com ticks maiores, utilize diretamente a CLI e os exportadores.

## Browser E2E com executáveis reais

O job gcc-debug do CI compila os executáveis C, instala Playwright e Chromium em um ambiente virtual temporário, inicializa o servidor HTTP em loopback e testa o fluxo pelo navegador: FCFS/RR, estados e Gantt, replay, comparação/JSON export, paginação LRU, laboratório seguro, importação CSV, validação de números e relatório H24 sintético explicitamente rotulado. Erros JavaScript não tratados fazem o teste falhar. Isso complementa os testes HTTP e CTest, e não usa respostas de simulador mock.
