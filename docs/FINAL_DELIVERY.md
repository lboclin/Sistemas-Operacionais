# Guia de reprodução e entrega acadêmica — OS-llm-project

**Data da auditoria:** 02/11/2026. **Branch J:** `feat/bloco-j-auditoria-entrega`. Nenhum dado de desempenho de LLM real é reivindicado.

## 1. Requisitos

- Ubuntu/Linux com CMake >= 3.16, compilador C11 (GCC ou Clang), pthreads e Python 3.
- Node.js recomendado para os testes JS; não é necessário para servir a interface.
- Sem banco de dados, Docker, npm, conta de nuvem ou serviço externo obrigatório. A aplicação web usa Python stdlib.
- Para ver o dashboard, um navegador moderno no computador local.

## 2. Build e verificação reproduzível

```bash
git clone https://github.com/fragaeduardo/OS-llm-project.git
cd OS-llm-project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

O repositório é **privado**: o clone requer acesso autorizado. Se já possui um checkout, use `git pull origin main` após integração da PR J.

O CI da PR J passou em quatro configurações (67/67 cada) e repetiu nove testes críticos seis vezes: [evidência GitHub Actions](https://github.com/fragaeduardo/OS-llm-project/actions/runs/41399236743). Consulte [TEST_PLAN.md](TEST_PLAN.md) para a matriz, cobertura e limitações dos testes.

## 3. Roteiro de demonstração

### A. Simulador e políticas

```bash
./build/os_llm_cli --policy fcfs --scenario scenarios/sample.csv --trace
./build/os_llm_cli --policy rr --quantum 1 --workers 2 --scenario scenarios/sample.csv --trace
./build/os_llm_cli --policy ema --ema-alpha 0.5 --ema-initial 6 --scenario scenarios/ema-demo.csv
```

Comparar espera, resposta, turnaround, utilização e p50/p95. Os números são **ticks virtuais**; não são latências de tokens produzidos por LLM.

### B. Memória e página

```bash
./build/os_llm_cli --frames 2 --page-tokens 2 --page-policy fifo --trace
./build/os_llm_cli --frames 2 --page-tokens 2 --page-policy lru --page-fault-latency 2 --snapshots
```

Demonstrar hits, faults, evicções e bloqueios/wakeups virtuais. OPT é somente para análise offline e não pode ser selecionado na memória online.

### C. Interface local

```bash
python3 apps/visual/server.py --cli ./build/os_llm_cli --port 8765
```

Abrir **http://127.0.0.1:8765**. Demonstrar dashboard, políticas, Gantt/replay visual, memória e comparação de execuções. Para laboratórios isolados de concorrência, configurar binários com `--race-lab`/`--deadlock-lab` (padrão no diretório `build`). Modos perigosos requerem confirmação; o deadlock real ocorre apenas no subprocesso demonstrativo sob watchdog.

### D. Experimentos e relatórios

```bash
python3 tools/experiments/run.py --help
python3 tools/analysis/report.py --help
```

Consultar [EXPERIMENTS.md](EXPERIMENTS.md) e [apps/visual/README.md](../apps/visual/README.md) para usar as ferramentas H23/H24 e importar `report.json`. Não chamar fixture sintética de benchmark de LLM real.

## 4. Como avaliar o projeto

O projeto demonstra **escalonamento, concorrência, memória e IPC** com motor C11 e visualização local. Há pthreads reais no adaptador POSIX, mas o coordenador preserva propriedade exclusiva dos estados do simulador. A GUI reconstrói Gantt a partir dos eventos C, sem executar simulação própria.

**Limites explícitos para apresentação:** não há inferência LLM real, persistência IPC, detecção integrada de deadlock de mutex no runtime principal nem SIGUSR1 no controlador principal. Pausar o replay da UI não pausa a simulação de C. Comparações válidas requerem o mesmo cenário completo — inclusive `predicted_tokens`.

## 5. Checklist final para apresentação presencial

- [ ] Confirmar CI da revisão final e atualizar branch principal, sem conflitos.
- [ ] Executar CTest no Ubuntu do apresentador.
- [ ] Abrir a UI no navegador e testar entrada, importação CSV e resultados.
- [ ] Testar Gantt, FIFO/LRU, laboratórios POSIX, exportação e relatório H24.
- [ ] Capturar screenshots do sistema real, não mockups.
- [ ] Incluir no relatório acadêmico as limitações, versões do compilador e proveniência dos experimentos.

A aprovação automática do CI **não** substitui o teste manual da interface e não autoriza alegar requisitos futuros como implementados.
