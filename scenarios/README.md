# Cenários de referência

Workloads pequenos e auditáveis, com seed e formato versionados.

Formato v2: `id,arrival_tick,input_tokens,priority`. Cada token de entrada custa 1 tick virtual (servidor do Jev, ver [ADR 0002](../docs/adr/0002-foco-typesafe-jev.md)). A CLI aceita o cabeçalho `# os-llm-scenario v2` e CSV sem cabeçalho; o formato v1, de cinco colunas com `predicted_tokens`, é rejeitado.

- `sample.csv`: três requisições usadas nos exemplos e na interface.
- `seed42-v2.csv`: fixture gerada com `os_llm_workload --seed 42 --count 4 --arrival-span 10 --min-tokens 2 --max-tokens 8`.
