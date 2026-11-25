# Gateway

Valida cargas NEW com IDs únicos e admite pedidos na ordem de chegada. Não escolhe a política de escalonamento.

Contrato: `gateway.h` (`os_gateway_validate`, `os_gateway_admit`, `os_gateway_next_arrival`). Teste: `gateway_events`. O IPC por pipes é uma demonstração separada ([src/ipc](../ipc/README.md)); não há gateway HTTP.
