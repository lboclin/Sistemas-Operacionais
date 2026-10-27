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

**Limitação:** é um laboratório separado. Não integra gateway/runtime/PCB/cache KV reais e não implementa SIGUSR1.
