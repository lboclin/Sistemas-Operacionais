# ADR 0002 — Foco em um servidor do Jev (TypeSafe AI)

**Estado:** aceita pelo grupo em 04/11/2026.

## Contexto
O escopo original simulava requisições de um LLM **generativo** e media o progresso em tokens de **saída**. Para o SJF, isso exige prever o tamanho da resposta antes de executá-la. Em um modelo generativo, o tamanho da entrada não determina o tamanho da saída: um prompt curto pode gerar uma resposta longa e vice-versa. Um preditor útil seria um projeto de aprendizado de máquina à parte e tiraria o foco de sistemas operacionais. Um preditor simples, baseado no tamanho da entrada, teria pouca qualidade. Antes desta decisão, o grupo implementou o SJF por média exponencial (EMA), um preditor offline de regressão ridge e um adaptador SJF-IA (bloco G). O preditor foi treinado com uma fixture de 16 linhas sintéticas e, como o próprio README registrava, demonstrava funcionamento e não qualidade real de previsão.

O Jev, da TypeSafe AI (lançado em 15/09/2026), é um modelo de **decisão**: devolve uma categoria, uma nota ou uma probabilidade, e não texto livre. A saída é pequena e de tamanho praticamente fixo, então o custo de uma requisição depende da **entrada**, cujo tamanho é conhecido na chegada.

## Alternativas
- **Manter o LLM generativo e treinar um preditor:** alto custo e desvio de foco.
- **Manter o LLM generativo e usar média exponencial:** a técnica clássica supõe bursts repetidos de um mesmo processo; requisições independentes não têm esse histórico.
- **Simular um servidor do Jev:** o tamanho do job é conhecido na chegada, sem previsão.

## Decisão
O simulador modela um servidor que atende requisições do Jev.

- **1 tick virtual = 1 token de entrada processado.** O campo `total_tokens` passa a significar tokens de entrada; `generated_tokens` foi renomeado para `processed_tokens`.
- O **SJF** ordena pelo tamanho da entrada. As variantes "SJF estimado" e "SJF oráculo" foram unificadas: conhecer o tamanho deixou de ser vazamento de informação.
- Foram removidos: `predicted_tokens`, a política SJF-EMA e suas opções de CLI, o SJF-oráculo, o preditor e o adaptador SJF-IA (`tools/predictor/`), e `src/prediction/`.
- Ficam todos os outros trabalhos: painel web, SIGUSR1, laboratório de deadlock com pthreads, IPC por pipes (`os_llm_ipc_runtime`, com formato de mensagem v2), experimentos e análise. A coleta de workloads (`tools/workload_capture`) passa a gerar CSV v2 a partir dos tokens de **entrada** (`usage.prompt_tokens`).
- O cenário CSV passa ao formato **v2**: `id,arrival_tick,input_tokens,priority`. O formato v1 (cinco colunas) é rejeitado. A exportação passa ao esquema `os-llm-run` **v2**.
- Os demais requisitos de SO continuam: FCFS, SJF, Round Robin, prioridade com aging, workers POSIX, sincronização, deadlock, paginação, IPC e sinais.

## Consequências
- O escopo é **direcionado**, não reduzido: sai o subprojeto de aprendizado de máquina (F14) e o foco volta ao escalonamento e aos recursos do SO. A remoção do bloco G foi combinada com o autor dele antes de ser feita.
- A proposta original listava SJF por média exponencial e SJF com preditor de IA. Essa mudança precisa ser explicada na apresentação.
- Tratar o custo como proporcional ao tamanho da entrada é uma **simplificação** do modelo de tempo, adequada a um simulador didático.
- Os dados sobre o Jev vêm de material de lançamento e cobertura de imprensa; os números de desempenho divulgados são da própria TypeSafe. O simulador não chama o Jev nem depende de suas métricas.

## Validação
macOS: 48 de 64 testes do CTest passam, incluindo os testes Python e JavaScript do painel, dos experimentos e da coleta. Os 16 restantes falham do mesmo modo na `main` anterior a esta mudança: são testes POSIX (`pthread_barrier_t`, `sem_init`) que não compilam no macOS, mais dois testes Python que dependem desses binários. Eles rodam no CI Linux.
