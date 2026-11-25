# Experimentos

Executados para o CHK3. Números, tabelas e conclusões: [results/chk3/SUMMARY.md](../results/chk3/SUMMARY.md). Tabelas agregadas: [aggregate.md](../results/chk3/aggregate/aggregate.md). Os tempos são ticks virtuais, não latência real.

## O que foi medido

| ID | Experimento | Como |
| --- | --- | --- |
| E1 | Condição de corrida | `os_llm_race_lab` em modo safe e unsafe, com barreira que força a intercalação |
| E2 | Escalonamento | FCFS, SJF, HRRN, RR (q=1 e q=3) e prioridade com e sem aging |
| E3 | Memória | FCFS com FIFO e LRU, 2, 4, 8 e 16 frames, 4 tokens por página e 5 ticks por falta |
| E4 | Deadlock | `os_llm_deadlock_lab` com mutexes em ordem global e em ordem cruzada, com watchdog |

São 11 cargas: 200 requisições com `uniform` e `burst` (seeds 1–5, 2 workers) e o cenário [starvation.csv](../scenarios/starvation.csv) (1 worker). Todas as políticas usam a mesma carga e o mesmo `--starvation-threshold 320`; o relatório recusa comparações entre cargas diferentes. OPT existe só no replay offline e não entra no E3.

## Como reproduzir

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
tools/experiments/chk3.sh build /tmp/chk3
```

O primeiro argumento é o diretório de build e o segundo, o de saída. O script não sobrescreve resultados existentes; para refazer `results/chk3`, remova `raw/`, `reports/` e `aggregate/` antes. Com os mesmos binários, a saída é idêntica à versionada. Leva cerca de 30 segundos.

Os dumps completos (`raw/*/runs/`, cerca de 85 MB) não são versionados. Os manifests guardam as métricas e o `trace_fingerprint` de cada execução.

## Ferramentas

- [tools/experiments](../tools/experiments/README.md): `run.py` executa E1–E4 sobre um cenário.
- [tools/analysis](../tools/analysis/README.md): `report.py` valida os dados brutos e gera CSV, Markdown, JSON e SVG; `--aggregate` combina seeds.

## Métricas

Média, p50, p95 e máximo de espera, resposta e retorno (turnaround), throughput, utilização dos workers, preempções, `starvation_count`, faltas de página, evicções e ticks bloqueados. As definições estão em [src/metrics](../src/metrics/README.md) e as limitações, no [SUMMARY](../results/chk3/SUMMARY.md).
