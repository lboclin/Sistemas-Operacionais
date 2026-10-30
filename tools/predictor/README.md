# Preditor offline de tokens de saída — G20

Ferramenta **opcional**, em Python 3.9+ e **somente biblioteca padrão**. Não altera o núcleo C11, o escalonador, a CLI ou o formato CSV v1. Este commit **não** integra as previsões ao runtime; essa etapa é posterior.

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
