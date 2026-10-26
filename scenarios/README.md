# Cenários de referência

Local para workloads pequenos e auditáveis, com seed e formato versionados. Ainda não há cenários executáveis porque não foi aprovado o schema.


`seed42-v1.csv` é fixture versionada gerada com `os_llm_workload --seed 42 --count 4 --arrival-span 10 --min-tokens 2 --max-tokens 8`. A CLI aceita o cabeçalho `# os-llm-scenario v1` e continua aceitando CSV legado sem cabeçalho.
