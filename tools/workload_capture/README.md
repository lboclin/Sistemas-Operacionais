# G22 — Coleta opcional de tamanhos de entrada e geração de workloads

`capture.py` usa **Python 3.9+ e somente biblioteca padrão**. Gera cenários no formato CSV v2 do simulador (servidor do Jev: 1 tick = 1 token de entrada). A ferramenta possui duas etapas independentes: `collect` (requisições **reais**, somente mediante opt-in explícito, a um endpoint OpenAI-compatible) e `generate` (reamostragem determinística de metadados previamente coletados). **Nenhum LLM foi executado durante o desenvolvimento deste commit**; os testes usam um servidor HTTP mock local. Sem `--capture`, o gerador usa uma fixture **inteiramente sintética**.

## Uso offline, sem modelo

Na raiz do repositório:

```bash
python3 tools/workload_capture/capture.py generate \
  --seed 42 --count 12 --distribution burst --profile memory-pressure \
  --output-dir /tmp/osllm-g22-synthetic
./build/os_llm_cli --scenario /tmp/osllm-g22-synthetic/scenario.csv --policy sjf
python3 -m unittest discover -s tools/workload_capture -p 'test_capture.py' -v
# Opcional, após cmake -S . -B build && cmake --build build:
ctest --test-dir build -R workload_capture_py --output-on-failure
```

Saídas: `scenario.csv` (cabeçalho `# os-llm-scenario v2 seed=N distribution=... count=N` e linhas `id,arrival_tick,input_tokens,priority`) e `scenario.meta.json` (schema `os-llm-workload-scenario/v1`, origem, SHA-256 do arquivo de captura, seed, distribuição, perfil e mapa de amostras). O diretório de saída **não pode existir**, para impedir sobrescritas acidentais.

Os `input_tokens` do cenário vêm de `usage.prompt_tokens` **observado**, quando houver coleta real, ou de valores **fictícios** da fixture. Um registro com zero tokens de entrada é rejeitado. Os tokens de saída continuam registrados no sidecar só como metadado: no modelo do Jev a resposta tem tamanho fixo e não entra no custo. Não há previsão (veja a [ADR 0002](../../docs/adr/0002-foco-typesafe-jev.md)). Latências observadas são wall time e ficam apenas no sidecar; o simulador usa ticks virtuais.

## Coleta opt-in com LLM pequeno local

Prepare um JSONL **fora do repositório** com um objeto por linha:

```json
{"request_id":1,"prompt":"Explique em uma frase o que é uma fila.","task_category":"qa","max_new_tokens":64,"priority":1}
```

Com um servidor local que exponha **`/v1/chat/completions`** e retorne `usage.prompt_tokens`, `usage.completion_tokens` e `choices[0].message.content`:

```bash
python3 tools/workload_capture/capture.py collect \
  --prompts /tmp/osllm-prompts.jsonl \
  --endpoint http://127.0.0.1:8080/v1/chat/completions \
  --model nome-do-modelo-local --max-prompts 16 --timeout 15 \
  --output /tmp/osllm-capture.json
python3 tools/workload_capture/capture.py generate \
  --capture /tmp/osllm-capture.json --seed 42 --count 32 \
  --distribution burst --profile high-concurrency \
  --output-dir /tmp/osllm-g22-observed
```

A coleta faz requisições sequenciais, com `temperature=0`, `stream=false` e `max_tokens` limitado por prompt. `--seed N` **opcional** é enviado ao servidor, mas não garante determinismo da inferência. `--max-prompts` (1–256, padrão 16) limita chamadas; cada prompt tem no máximo 4096 caracteres e `max_new_tokens` de 1 a 4096; `--timeout` (0.1–120 s, padrão 15) vale **por chamada**. Falhas HTTP, timeout, JSON inválido, ausência de `usage` ou contagens incompatíveis interrompem a coleta **sem gravar um dataset parcial**. Alguns servidores não fornecem `usage` compatível: nesse caso a coleta falha explicitamente, sem estimar tokens por caracteres.

Por padrão só são aceitos endpoints de loopback (`localhost`, `127.0.0.1`, `::1`). Um serviço remoto exige **`--allow-remote` e HTTPS**; use `--api-key-env NOME_DA_VARIAVEL` para ler uma chave do ambiente, nunca a inclua na URL ou em arquivos de prompt. O cliente não salva prompts, respostas, cabeçalhos ou chaves; salva **somente metadados**: IDs, categoria, prioridade, caracteres de prompt/resposta, tokens de prompt/saída, limite, motivo de término, modelo informado e latência. A proveniência (`os-llm-workload-capture/v1`) registra modelo solicitado, tipo de endpoint, UTC de coleta, parâmetros e hash **somente dos metadados de entrada**. O conteúdo dos prompts **é enviado ao endpoint escolhido**, portanto use apenas textos públicos ou autorizados; IDs, categorias e contagens também podem ser sensíveis e não devem ser versionados sem revisão.

## Perfis e limitações

`--distribution uniform|burst|staggered` controla chegadas virtuais; `--arrival-span N` (padrão 20) define o horizonte. `--profile mixed|short|long|memory-pressure|high-concurrency|priority` controla a seleção das amostras: `short`/`long` selecionam metade inferior/superior dos tamanhos de entrada, `memory-pressure` seleciona entradas longas com chegadas concentradas em até 2 ticks, `high-concurrency` concentra chegadas em até 2 ticks, e `priority` gera prioridades 0–5 pela seed. A geração amostra **com reposição**, de modo que pode repetir o mesmo registro de origem, mas cria novos IDs únicos; mesma captura + mesmos parâmetros + mesma versão de Python produzem os mesmos dois arquivos.

A fixture `synthetic_fixture.json` é rotulada `source=synthetic_fixture`, com latência zero **fictícia**. Ela não contém observações de LLM e não serve para afirmar realismo ou qualidade preditiva. Coletas reais são apenas uma amostra do modelo, prompt-set e configurações usados; não representam distribuição universal de respostas. Para benchmarks, separe os prompts/conversas usados no treinamento do preditor daqueles usados na avaliação (inclusive entre datasets amostrados), documente hardware/modelo/configuração e não interprete a latência real como tick virtual.

**Testes:** `test_capture.py` valida mock HTTP local, contagens e privacidade dos arquivos, erros HTTP/timeout/JSON/usage, rejeição de endpoints remotos inseguros, entradas malformadas, compatibilidade do CSV v2 e determinismo por seed. Quando `OSLLM_CLI` está disponível, testa também o carregamento do cenário gerado com FCFS, SJF, RR e prioridade. Nenhum endpoint real é contatado nos testes.
