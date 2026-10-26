# Exportação reproduzível de execuções virtuais

O módulo `src/results/export.{h,c}` recebe uma execução já concluída ou pausada:
PCBs originais, PCBs finais, configuração, métricas e trace capturado por
`os_trace_capture`. Ele não reexecuta nem altera o motor de simulação.

```sh
./build/os_llm_cli --scenario scenarios/seed42-v1.csv --policy rr --quantum 2 \
  --frames 2 --page-tokens 2 --page-policy lru \
  --export-json run.json --export-csv run.csv
```

- JSON: `schema=os-llm-run`, `version=1`, `engine=virtual`, `source`,
  `seed` (número ou `null`), `config`, `input`, `requests`, `events`,
  `metrics`, `trace_fingerprint` e `finished`.
- CSV: formato longo `section,index,field,value`. Seções: `meta`,
  `config`, `input`, `request`, `event`, `metric`. Uma linha por campo;
  o índice de requisição/evento é baseado em zero. Campos opcionais vazios.
- Seed só é conhecida quando o arquivo de cenário contém cabeçalho v1; CSV
  legado e carga padrão são registrados com seed desconhecida. O cenário de
  entrada está integralmente no artefato, independentemente da seed.
- Fingerprint FNV-1a64 versionado, **não criptográfico**. Tempos são ticks
  virtuais, não tempos reais. Nenhum timestamp de execução é inserido para
  preservar a reprodução byte a byte. Números decimais pressupõem locale C.
- Saídas parciais de `--steps` são permitidas e trazem `finished=false`.
  O escritor exige um trace válido e não faz escrita em caso de argumentos
  inválidos. Falhas de I/O podem deixar saída parcial em streams fornecidos
  pela biblioteca; a CLI remove arquivos cujo fechamento falhar.

Teste de contrato: `ctest --test-dir build -R '^export$' --output-on-failure`.
