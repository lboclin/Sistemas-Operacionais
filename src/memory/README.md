# Memória e paginação

`pager.c`: replay offline FIFO/LRU/OPT. `frames.c`: cache de frames FIFO/LRU online, agora utilizado pelo runtime virtual.

A página é o par (`request_id`, `page_id`), com frames compartilhados entre workers. Páginas residentes são liberadas ao concluir o pedido. Em memória cheia há evicção, sem bloqueio ou custo adicional de IO; não há KV cache real de LLM.

Testes: `pager`, `frames` e `memory_runtime`.
