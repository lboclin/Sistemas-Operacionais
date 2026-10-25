#include "gateway.h"

bool os_gateway_validate(const OsRequest *requests, size_t count) {
    if (requests == NULL || count == 0) return false;
    for (size_t i = 0; i < count; ++i) {
        if (!os_request_is_fresh(&requests[i])) return false;
        for (size_t j = 0; j < i; ++j)
            if (r->id == requests[j].id) return false;
    }
    return true;
}

bool os_gateway_admit(OsRequest *request, uint64_t now, uint64_t order) {
    if (!os_request_is_fresh(request) || order == OS_REQUEST_NEVER ||
        request->arrival_tick > now || !os_request_transition(request, OS_READY)) return false;
    request->ready_tick = request->arrival_tick;
    request->ready_order = order;
    return true;
}

uint64_t os_gateway_next_arrival(const OsRequest *requests, size_t count) {
    uint64_t tick = OS_REQUEST_NEVER;
    if (requests == NULL) return tick;
    for (size_t i = 0; i < count; ++i)
        if (requests[i].state == OS_NEW && requests[i].arrival_tick < tick)
            tick = requests[i].arrival_tick;
    return tick;
}
