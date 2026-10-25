#include "events.h"
#include "gateway.h"
#include "request.h"
#include "virtual.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    OsEvent events[64];
    size_t count;
} Captured;

static void capture(const OsEvent *event, void *context) {
    Captured *captured = context;
    assert(event != NULL && captured->count < 64);
    captured->events[captured->count++] = *event;
}

int main(void) {
    OsRequest r[2];
    assert(os_request_init(&r[0], 10, 0, 3, 3, 0));
    assert(os_request_init(&r[1], 11, 1, 2, 2, 0));
    assert(os_gateway_validate(r, 2));
    assert(os_gateway_next_arrival(r, 2) == 0);
    assert(!os_gateway_admit(&r[1], 0, 1));
    assert(os_gateway_admit(&r[1], 1, 1));
    assert(os_gateway_next_arrival(r, 2) == 0);
    assert(!os_gateway_validate(r, 2));
    assert(os_request_init(&r[1], 11, 1, 2, 2, 0));
    OsVirtualConfig cfg = {.policy = OS_ROUND_ROBIN, .worker_count = 1, .quantum_tokens = 1};
    OsRunSummary summary;
    Captured events = {0};
    assert(os_virtual_run_observed(r, 2, &cfg, &summary, capture, &events) == OS_RUN_OK);
    assert(summary.completed == 2 && summary.makespan_ticks == 5);
    assert(events.count == 2 + 5 + 3 + 2); /* arrived, dispatch, preempt, completed */
    assert(events.events[0].type == OS_EVENT_ARRIVED && events.events[0].tick == 0);
    assert(events.events[1].type == OS_EVENT_DISPATCHED && events.events[1].request_id == 10);
    for (size_t i = 1; i < events.count; ++i) {
        assert(events.events[i].tick >= events.events[i-1].tick);
        assert(events.events[i].type != OS_EVENT_PAGE_FAULT);
    }
    assert(os_event_type_name(OS_EVENT_PREEMPTED)[0] == 'p');
    return 0;
}
