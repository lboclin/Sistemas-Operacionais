#ifndef OS_REQUEST_H
#define OS_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

#define OS_REQUEST_NEVER UINT64_MAX

typedef enum {
    OS_NEW,
    OS_READY,
    OS_RUNNING,
    OS_BLOCKED,
    OS_TERMINATED
} OsRequestState;

typedef struct {
    uint32_t id;
    uint64_t arrival_tick;
    uint32_t total_tokens;
    uint32_t predicted_tokens;
    int priority;
    uint32_t generated_tokens;
    uint64_t ready_tick;
    uint64_t ready_order;
    uint64_t first_run_tick;
    uint64_t finished_tick;
    uint64_t waiting_ticks;
    OsRequestState state;
} OsRequest;

bool os_request_init(OsRequest *request, uint32_t id, uint64_t arrival_tick,
                     uint32_t total_tokens, uint32_t predicted_tokens, int priority);
/* A freshly initialized PCB is the only kind accepted at the gateway boundary.
 * Keep this invariant in core, rather than duplicating field checks in adapters. */
bool os_request_is_fresh(const OsRequest *request);
bool os_request_transition(OsRequest *request, OsRequestState next);
const char *os_request_state_name(OsRequestState state);

#endif
