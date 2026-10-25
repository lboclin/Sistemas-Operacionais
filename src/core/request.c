#include "request.h"

#include <stddef.h>

bool os_request_init(OsRequest *request, uint32_t id, uint64_t arrival_tick,
                     uint32_t total_tokens, uint32_t predicted_tokens, int priority) {
    if (request == NULL || total_tokens == 0 || predicted_tokens == 0 ||
        arrival_tick == OS_REQUEST_NEVER) {
        return false;
    }
    *request = (OsRequest){
        .id = id, .arrival_tick = arrival_tick, .total_tokens = total_tokens,
        .predicted_tokens = predicted_tokens, .priority = priority,
        .first_run_tick = OS_REQUEST_NEVER, .finished_tick = OS_REQUEST_NEVER,
        .state = OS_NEW
    };
    return true;
}

bool os_request_transition(OsRequest *request, OsRequestState next) {
    if (request == NULL) return false;
    const OsRequestState previous = request->state;
    const bool valid =
        (previous == OS_NEW && next == OS_READY) ||
        (previous == OS_READY && next == OS_RUNNING) ||
        (previous == OS_RUNNING && (next == OS_READY || next == OS_BLOCKED || next == OS_TERMINATED)) ||
        (previous == OS_BLOCKED && next == OS_READY);
    if (!valid) return false;
    request->state = next;
    return true;
}

const char *os_request_state_name(OsRequestState state) {
    switch (state) {
        case OS_NEW: return "NEW";
        case OS_READY: return "READY";
        case OS_RUNNING: return "RUNNING";
        case OS_BLOCKED: return "BLOCKED";
        case OS_TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}
