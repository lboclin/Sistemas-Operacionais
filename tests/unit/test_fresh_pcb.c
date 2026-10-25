#include "gateway.h"
#include "request.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static OsRequest fresh(void) {
    OsRequest r;
    assert(os_request_init(&r, 19, 5, 8, 7, 2));
    return r;
}

#define REJECT_CORRUPTION(field, value) do { \
    OsRequest bad = fresh(); \
    bad.field = value; \
    assert(!os_request_is_fresh(&bad)); \
    assert(!os_gateway_validate(&bad, 1)); \
    OsRequest before = bad; \
    assert(!os_gateway_admit(&bad, 5, 0)); \
    assert(memcmp(&bad, &before, sizeof bad) == 0); \
} while (0)

int main(void) {
    OsRequest r = fresh();
    assert(!os_request_is_fresh(NULL));
    assert(os_request_is_fresh(&r));
    assert(os_gateway_validate(&r, 1));
    assert(!os_request_init(NULL, 1, 0, 1, 1, 0));
    assert(!os_request_init(&r, 1, OS_REQUEST_NEVER, 1, 1, 0));
    assert(!os_request_init(&r, 1, 0, 0, 1, 0));
    assert(!os_request_init(&r, 1, 0, 1, 0, 0));
    REJECT_CORRUPTION(state, OS_READY);
    REJECT_CORRUPTION(generated_tokens, 1);
    REJECT_CORRUPTION(total_tokens, 0);
    REJECT_CORRUPTION(predicted_tokens, 0);
    REJECT_CORRUPTION(arrival_tick, OS_REQUEST_NEVER);
    REJECT_CORRUPTION(ready_tick, 5);
    REJECT_CORRUPTION(ready_order, 1);
    REJECT_CORRUPTION(first_run_tick, 0);
    REJECT_CORRUPTION(finished_tick, 5);
    REJECT_CORRUPTION(waiting_ticks, 1);
    r = fresh();
    OsRequest second = fresh();
    second.id = 20;
    OsRequest valid[] = {r, second};
    assert(os_gateway_validate(valid, 2));
    valid[1].id = r.id;
    assert(!os_gateway_validate(valid, 2));
    assert(!os_gateway_validate(NULL, 2));
    assert(!os_gateway_validate(valid, 0));
    assert(!os_gateway_admit(&r, 4, 0));
    assert(!os_gateway_admit(&r, 5, OS_REQUEST_NEVER));
    assert(os_gateway_admit(&r, 7, 12));
    assert(r.state == OS_READY && r.ready_tick == 5 && r.ready_order == 12);
    assert(!os_gateway_admit(&r, 7, 13));
    assert(!os_request_is_fresh(&r));
    return 0;
}
