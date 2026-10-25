# Memória e paginação

`pager.c`: replay offline FIFO/LRU/OPT. `frames.c`: cache de frames FIFO/LRU online, agora utilizado pelo runtime virtual.

A página é o par (`request_id`, `page_id`), com frames compartilhados entre workers. Páginas residentes são liberadas ao concluir o pedido. Em memória cheia há evicção, sem bloqueio ou custo adicional de IO; não há KV cache real de LLM.

A tabela de páginas de cada requisição é uma **visão derivada** dos frames residentes, não um segundo armazenamento mutável: `os_frames_lookup` resolve o par (`request_id`, `page_id`) para um índice físico e `os_frames_snapshot_request` enumera os mapeamentos de um único pedido. Isso evita inconsistência entre a tabela e o cache após FIFO/LRU, evicção ou liberação. As consultas não atualizam relógio nem contadores e snapshots com capacidade insuficiente não escrevem parcialmente. Os índices físicos são válidos somente até a próxima mutação do cache (especialmente liberação, que compacta os frames).

**Limite atual:** as consultas são da API de memória; o snapshot da sessão ainda informa somente a quantidade de frames residentes. Não há page fault com latência ou bloqueio, nem memória física real.

Testes: `pager`, `frames`, `page_tables` e `memory_runtime`.
