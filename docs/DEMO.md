# Roteiro da demonstração

Cada vídeo tem até 3 minutos. Os comandos são executados na raiz do repositório, em Linux.

## Preparação

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
python3 apps/visual/server.py --cli ./build/os_llm_cli --port 8765
```

Abra `http://127.0.0.1:8765`. O servidor encontra os laboratórios em `build/`. Encerre com CTRL+C.

## Vídeo A: starvation no SJF e HRRN (painel)

1. Em **Cenário de entrada**, clique em **Importar CSV** e escolha `scenarios/starvation.csv`. Aparece "Importadas 200 requisições". Deixe **Workers** em 1.
2. Em **Política**, escolha **SJF · Menor entrada** e clique em **Executar simulação**.
3. Em **Estado dos processos**, a linha do PCB 5 (o pedido longo) mostra início em 796, conclusão em 836 e **espera de 796**. No **Gantt e replay de eventos**, a barra P5 é a última (passe o mouse: `[796, 836)`).
4. Mude para **HRRN · Maior razão de resposta** e execute de novo. O PCB 5 começa em 120, termina em 160 e **espera 120**. No Gantt, P5 aparece no meio dos curtos (`[120, 160)`).
5. Em **Comparar e exportar**, as duas execuções aparecem lado a lado: espera média de 15,8 no SJF e 46,22 no HRRN.

O que falar: o SJF tem a menor média, mas deixa o longo para o fim de todo o fluxo. No HRRN a razão do longo cresce com a espera, então ele é atendido; em troca, os curtos esperam mais. O tempo total é 836 ticks nos dois casos.

Para mostrar o replay, use **Início**, **Reproduzir** e o controle deslizante. O replay só percorre os eventos já gravados; não pausa o motor.

## Vídeo B: laboratórios de corrida e deadlock (painel)

Em **Laboratórios controlados**, clique nos botões. Os modos perigosos pedem confirmação e rodam em subprocesso com watchdog.

| Botão | Resultado na tela |
| --- | --- |
| Corrida segura | `mode=safe expected=2 observed=2 lost=0 (código 0)` |
| Perda controlada (confirmar) | `mode=unsafe expected=2 observed=1 lost=1 (código 0)` |
| Ordem global | `mode=ordered completed=2 watchdog=none deadlock=no (código 0)` |
| Deadlock isolado (confirmar) | `mode=deadlock acquired=2 watchdog=SIGKILL deadlock=observed (código 0)`, após cerca de 2 segundos |

O que falar: na perda controlada, as duas threads leem o mesmo valor antes de escrever, e uma atualização se perde; a versão segura usa operação atômica. No deadlock, cada thread segura um mutex e espera o da outra, e o processo pai encerra o filho; com ordem global de aquisição não há ciclo. A barreira força a intercalação: o laboratório mostra o fenômeno, não a frequência dele.

## Vídeo C: comparação no terminal

```bash
printf '%-6s %6s %7s %7s\n' politica longo media maxima
for p in fcfs sjf hrrn; do
  ./build/os_llm_cli --policy $p --workers 1 --scenario scenarios/starvation.csv |
  awk -v p=$p '
    $1 == "5" && NF == 7 { longo = $6 }
    /average wait=/ { split($0, a, "average wait="); split(a[2], b, ","); media = b[1] }
    /^Metrics:/     { split($0, a, "max=");          split(a[2], b, ","); maxima = b[1] }
    END { printf "%-6s %6s %7s %7s\n", p, longo, media, maxima }'
done
```

Saída (espera em ticks: do pedido longo, média e máxima):

```text
politica  longo   media  maxima
fcfs       16   50.90      52
sjf       796   15.80     796
hrrn      120   46.22     120
```

O que falar: o FCFS atende o longo cedo porque ele chegou no início, mas todos os curtos que vêm depois esperam mais; o SJF tem a menor média e a pior espera máxima; o HRRN limita a espera do longo.

Mesmos laboratórios pelo terminal, se o painel não abrir:

```bash
./build/os_llm_race_lab safe
./build/os_llm_race_lab unsafe
./build/os_llm_deadlock_lab ordered
./build/os_llm_deadlock_lab deadlock
```
