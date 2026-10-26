# Gerador de cargas

`workload.h`/`workload.c`: SplitMix64 determinístico com seed de 64 bits e três distribuições de chegada: uniforme, dois bursts e espaçada. O formato CSV v1 é compatível com a CLI existente e inclui metadados de seed, distribuição e quantidade. Sem cabeçalho, CSV antigo continua aceito.

```bash
./build/os_llm_workload --seed 42 --count 4 --distribution uniform --arrival-span 10 --min-tokens 2 --max-tokens 8 --output scenario.csv
./build/os_llm_cli --scenario scenario.csv
```

Máximo de 256 requisições. O algoritmo usa aritmética inteira e não depende do `rand()` da plataforma; não simula tokens reais de um LLM.
