# Integração de previsões — G21

O adaptador externo [`tools/predictor/compare_sjf.py`](../../tools/predictor/compare_sjf.py) aplica estimativas produzidas pelo modelo offline G20 ao campo `predicted_tokens` do cenário CSV v1, antes de executar o motor virtual C11 com `--policy sjf`. A CLI e o core C não importam Python e não acessam o modelo.

O adaptador compara FCFS, SJF-EMA, SJF com previsões do modelo (rotulado **AI** no relatório) e SJF-oráculo sobre a mesma carga. A inferência recebe **apenas** `prompt_tokens`, `max_new_tokens` e `task_category` de um CSV de metadados pré-geração; `total_tokens` é lido depois e serve somente para a simulação e avaliação. Os resultados exportados são verificáveis e determinísticos para entradas fixas.

Consulte [instruções e contratos](../../tools/predictor/README.md#comparação-sjf-ia-com-o-simulador-g21). Não há inferência de LLM nem previsão online dentro do processo C. Sem modelo válido, a comparação falha explicitamente; FCFS e EMA continuam disponíveis diretamente na CLI.
