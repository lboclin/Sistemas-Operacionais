# Preditor offline de tokens de saída — G20

Ferramenta **opcional**, em Python 3.9+ e **somente biblioteca padrão**. Não altera o núcleo C11, o escalonador, a CLI ou o formato CSV v1. O G20 treina e exporta o modelo; o G21 integra as previsões ao simulador via adaptador externo, mantendo o núcleo C11 independente de Python.

## Contrato dos dados

CSV de treinamento (cabeçalho obrigatório): `request_id,prompt_tokens,max_new_tokens,task_category,output_tokens`.

- `request_id`: inteiro positivo único; `prompt_tokens`: inteiro não negativo; `max_new_tokens`: inteiro positivo; `task_category`: texto não vazio (até 80 caracteres); `output_tokens`: inteiro positivo, não maior que `max_new_tokens`.
- **Apenas** `prompt_tokens`, `max_new_tokens` e `task_category` são features de entrada. `output_tokens` é o alvo observado, usado **exclusivamente no treinamento e na avaliação**; o ID é identificador, não feature. Não se utiliza `total_tokens`, tamanho real da resposta ou estatísticas do conjunto de teste como entrada do modelo.
- Na predição, basta `request_id,prompt_tokens,max_new_tokens,task_category`; se `output_tokens` estiver presente, será ignorado. Categoria desconhecida usa vetor categórico zero (fallback documentado).
- A fixture `synthetic_fixture.csv` é **inteiramente sintética**, inventada para testes de contrato. Não é resultado de execução de LLM e não permite alegar qualidade preditiva em produção.

## Uso

Execute a partir da raiz do repositório:

```bash
python3 tools/predictor/predictor.py train \
  --data tools/predictor/synthetic_fixture.csv \
  --model /tmp/osllm-model.json --report /tmp/osllm-eval.json \
  --seed 42 --test-fraction 0.25 --ridge 1.0
python3 tools/predictor/predictor.py predict \
  --data tools/predictor/synthetic_fixture.csv \
  --model /tmp/osllm-model.json --output /tmp/osllm-predictions.json
python3 -m unittest discover -s tools/predictor -p 'test_*.py' -v
```

`train` embaralha por seed fixa e separa **IDs distintos** para treino/teste; ajusta médias, escalas, categorias e pesos **somente no treino**. Treina regressão linear ridge com solução Gauss-Jordan e regularização positiva. Avalia no teste isolado usando **MAE/RMSE** em tokens, incluindo baseline de média do treino. Exporta modelo `os-llm-output-predictor/v1` e relatório `os-llm-predictor-evaluation/v1`, com seed e IDs de cada split. `predict` exporta `os-llm-token-predictions/v1`, indexado por `request_id`; previsão arredondada e limitada ao intervalo `[1, max_new_tokens]`.

Para comparação válida em datasets reais, mantenha os dados de teste fora do treino e evite duplicações semânticas entre prompts. O split atual é por **requisição**, não por usuário/conversa/grupo: não garante ausência de vazamento se várias linhas derivarem do mesmo prompt. Não armazene prompts sensíveis. Este preditor é baseline didático, não LLM nem garantia de melhor escalonamento. Os testes verificam reprodutibilidade, separação dos IDs, entradas inválidas e ausência de uso do alvo na inferência.

## Comparação SJF-IA com o simulador (G21)

O adaptador `compare_sjf.py` conecta o preditor offline G20 à **CLI C11 existente**, sem alterar o núcleo ou o CSV v1. A opção **AI** é um rótulo do adaptador: o executável C recebe `--policy sjf` e `predicted_tokens` previamente calculados pelo modelo; **não** existe `--policy ai` no executável C.

Primeiro compile o simulador e treine o modelo **somente com dados de treino independentes**:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
python3 tools/predictor/predictor.py train \
  --data tools/predictor/synthetic_fixture.csv \
  --model /tmp/osllm-model.json --report /tmp/osllm-eval.json --seed 42
python3 tools/predictor/compare_sjf.py \
  --scenario scenarios/sjf-ai-demo.csv \
  --features tools/predictor/demo_features.csv \
  --model /tmp/osllm-model.json --cli ./build/os_llm_cli \
  --output-dir /tmp/osllm-sjf-g21 --workers 1 \
  --ema-alpha 0.5 --ema-initial 6
ctest --test-dir build -R '^(sjf_ai_comparison|sjf_ema)$' --output-on-failure
```

A fixture de treino (IDs 1–16), os metadados de inferência (IDs 101–105) e o cenário são **sintéticos**. Nenhum LLM foi executado. Para um experimento científico, separe treino e teste também por prompt/conversa, não só por ID; esse exemplo demonstra **funcionamento**, não qualidade real de previsão.

### Contratos e isolamento

- `--features` contém **exatamente** `request_id,prompt_tokens,max_new_tokens,task_category`, sem alvo de saída, `total_tokens` ou outras colunas. Os IDs devem corresponder exatamente aos do cenário CSV v1. `--model` deve seguir `os-llm-output-predictor/v1`.
- O adaptador calcula **todas** as previsões antes de carregar o cenário que contém `total_tokens`. A inferência usa apenas os metadados prévios; o alvo real é usado posteriormente **somente** na simulação e na medição de erro.
- A carga virtual (IDs, chegada, tokens reais, prioridades e previsão injetada) é idêntica nas quatro execuções: `fcfs`, `ema`, `ai` (SJF estimado com previsão do modelo) e `oracle` (usa o total verdadeiro apenas como limite experimental). Todas usam a mesma quantidade de workers; o EMA é aprendido online a partir de requisições concluídas.
- O diretório de saída precisa estar vazio. Saídas: `sjf-ai.csv` (cenário CSV v1), `predictions.json` (`os-llm-token-predictions/v1`), `fcfs.json`, `ema.json`, `ai.json`, `oracle.json` (exports existentes) e `comparison.json` (`os-llm-sjf-comparison/v1`). O relatório inclui métricas realmente exportadas pela CLI, MAE/RMSE sobre os alvos do cenário e SHA-256 dos arquivos de entrada. Os tempos são **ticks virtuais**, não latência de inferência.
- Modelo ausente/inválido, feature proibida, ID faltante, export incompleto ou falha do simulador **interrompem** a comparação; não há substituição silenciosa por previsões aleatórias. Para operar sem IA, use diretamente `./build/os_llm_cli --policy ema --scenario ...` ou FCFS. A ausência de fallback automático é intencional para não rotular um baseline como IA.

Teste do adaptador sem binário C: `python3 -m unittest discover -s tools/predictor -p 'test_compare_sjf.py' -v`. O CTest define `OSLLM_CLI` para também executar o teste ponta a ponta com o binário real. O adaptador não treina o modelo e não coleta workloads; essa coleta pertence ao G22.
