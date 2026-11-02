# Plano e evidências de testes — Bloco J

> Revisado em 02/11/2026. O motor C11 simula ticks virtuais; o resultado dos testes não demonstra inferência de LLM real nem inspeção visual manual.

## Executar localmente

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
# Testes de aceitação E2E com o motor C: são registrados no CTest.
OSLLM_CLI="$PWD/build/os_llm_cli" python3 -m unittest discover -s tests/visual -p 'test_acceptance.py' -v
node --test tests/visual/test_timeline.cjs tests/visual/test_reports.cjs
```

## Matriz validada no CI

[Execução 41399236743](https://github.com/fragaeduardo/OS-llm-project/actions/runs/41399236743) — PR de auditoria J:

| Configuração | CTest | Observações |
|---|---|---|
| GCC Debug | **67/67** | Python/JS sintaxe + nove testes críticos repetidos seis vezes |
| Clang Debug | **67/67** | compilação estrita e testes |
| GCC Debug + ASan/UBSan | **67/67** | sanitarização de memória e comportamento indefinido |
| GCC Release | **67/67** | binários otimizados; testes baseados em assert mantêm `-UNDEBUG` |

O passo adicional `ctest --repeat until-fail:6` aprovou **54 execuções** dos seguintes grupos: fila POSIX, thread pool, adaptador paralelo, deadlock liveness, IPC RR/FCFS, replay de trace, page faults e aceitação final. Ao todo são **322 invocações CTest aprovadas** (268 da matriz completa + 54 repetições). Esse total conta repetições; não são 322 casos diferentes.

## Abrangência

- **Unidade e integração C:** PCB/gateway, FIFO/LRU, faltas, fragmentação, estados, scheduler, SJF-EMA, métricas, exportação, replay/fingerprint, pthreads e IPC.
- **Sistema/CLI:** limites, erros de parâmetros, cenários CSV, chegadas acima de 32 bits, snapshots, casos positivos e negativos.
- **Web local:** API HTTP, validação estrita, `Host`/`Origin`, allowlists de recursos, execução do binário real, laboratórios com confirmação e watchdog.
- **Aceitação J:** seis políticas × um/dois workers, memória FIFO/LRU com/sem latência, 18 workloads com seed fixa, determinismo de métricas/trace, 256 requisições e retorno JSON completo.
- **JavaScript:** geração de Gantt sem intervalos fictícios, precauções contra perdas de precisão, comparação de cenários usando todos os campos, importação H24 validada.

## Ainda fora do escopo verificado

- Inspeção visual manual do navegador, teclado/acessibilidade e responsividade.
- ThreadSanitizer e hardware/OS diferentes do runner Ubuntu do Actions.
- Inferência real do LLM, desempenho real, persistência de serviço IPC e observabilidade real de mutex deadlock no runtime principal.
- Testes longos de carga em máquina do usuário: os testes de estresse existentes são finitos e determinísticos.

**Recomendação de aceite:** além do CI, abrir a interface localmente; testar edição/importação de CSV, todas as políticas, Gantt/replay, FIFO/LRU, laboratório de corrida/deadlock com confirmação e comparação H24. Registrar capturas reais e declarar limitações na apresentação.
