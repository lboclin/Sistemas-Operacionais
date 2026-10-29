# IPC POSIX: pipes entre processos

O módulo os_ipc implementa transporte didático de mensagens enquadradas por pipes POSIX. O executável os_llm_ipc_demo usa dois processos reais (fork): o gateway envia "hello ipc" e o worker responde "HELLO IPC" por dois pipes unidirecionais.

## Contrato

- Header de 4 bytes big-endian, seguido de 0 a 65536 bytes de payload.
- os_ipc_write_frame e os_ipc_read_frame não alocam memória, tentam novamente após EINTR e não interpretam payload.
- OS_IPC_EOF significa fim de stream antes do cabeçalho; OS_IPC_TRUNCATED significa EOF dentro de um frame. OS_IPC_TOO_LARGE rejeita payload acima do limite ou buffer insuficiente. out_length só muda em OS_IPC_OK.
- Após erro de framing ou buffer insuficiente, feche o canal: a implementação não drena o payload restante.
- O chamador gerencia descritores e SIGPIPE; escrita sem leitor pode encerrar o processo se SIGPIPE não for tratado. Múltiplos escritores no mesmo descritor exigem sincronização externa.

## Verificação

Compile com CMake e execute os_llm_ipc_demo. Rode CTest com os casos pipe_channel e ipc_demo. O teste cobre frames consecutivos, frame vazio, EOF, truncamento, limites e argumentos inválidos.

## Demonstração integrada com PCBs e pthreads (commit 17)

`os_llm_ipc_runtime` usa dois pipes, um `fork` e o transporte
`os_ipc_write_frame/os_ipc_read_frame`. O gateway envia três PCBs
iniciais com `id`, `arrival_tick`, `total_tokens`,
`predicted_tokens` e `priority`, mais política (FCFS/RR), workers e
quantum. O filho reconstrói cada PCB com `os_request_init`, rejeita
campos inválidos e IDs repetidos, e executa `os_posix_run` (pthread
pool com ACKs de tokens simulados). O retorno informa status, contagem
concluída, makespan e dados finais de cada PCB. O pai compara o retorno
com `os_virtual_run` determinístico.

Formato interno v1: frame existente de 4 bytes de comprimento, seguido
de magic `OLI1` ou `OLR1` e inteiros big-endian de 32/64 bits.
Nenhum ponteiro ou `struct` é transmitido. Há limite de oito PCBs e
prioridades não negativas nesta demonstração. Não há sessão remota
persistente nem tratamento próprio de SIGPIPE; o chamador controla
descriptors e ciclo de vida dos processos.

```bash
./build/os_llm_ipc_runtime --rr
./build/os_llm_ipc_runtime --fcfs
./build/os_llm_ipc_runtime --malformed
ctest --test-dir build -R '^ipc_runtime_' --output-on-failure
```

O modo `--malformed` verifica a rejeição de magic inválida pelo filho.
Este exemplo **não** implementa inferência LLM, cache KV remoto,
transmissão de prompts ou um serviço de múltiplos clientes. O
laboratório SIGUSR1 permanece independente.

