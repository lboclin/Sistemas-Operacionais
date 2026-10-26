# Memória e paginação

`pager.c`: replay offline FIFO/LRU/OPT. `frames.c`: cache de frames FIFO/LRU online, agora utilizado pelo runtime virtual.

A página é o par (`request_id`, `page_id`), com frames compartilhados entre workers. Páginas residentes são liberadas ao concluir o pedido. Em memória cheia há evicção. O runtime pode simular bloqueio e custo virtual de I/O por fault quando `page_fault_latency_ticks > 0`; não há KV cache real de LLM.

A pressão de memória usa substituição FIFO/LRU quando todos os frames estão ocupados; não há espera por capacidade. Como frames têm tamanho fixo, não há fragmentação externa. `OsSessionSnapshot.internal_fragmentation_tokens` conta slots ainda não preenchidos em páginas residentes e `free_frames` conta frames livres. `os_frames_page_at` permite leitura de uma posição física sem modificar o cache.

A tabela de páginas de cada requisição é uma **visão derivada** dos frames residentes, não um segundo armazenamento mutável: `os_frames_lookup` resolve o par (`request_id`, `page_id`) para um índice físico e `os_frames_snapshot_request` enumera os mapeamentos de um único pedido. Isso evita inconsistência entre a tabela e o cache após FIFO/LRU, evicção ou liberação. As consultas não atualizam relógio nem contadores e snapshots com capacidade insuficiente não escrevem parcialmente. Os índices físicos são válidos somente até a próxima mutação do cache (especialmente liberação, que compacta os frames).

**Limite atual:** as consultas são da API de memória; o snapshot da sessão ainda informa somente a quantidade de frames residentes. Há page fault com latência opcional, mas não há pinning de frames, fragmentação nem memória física real. Páginas podem ser evictadas enquanto a requisição aguarda; no primeiro retry após wakeup, um miss é recarregado sem nova espera.

Testes: `pager`, `frames`, `page_tables` e `memory_runtime`.
