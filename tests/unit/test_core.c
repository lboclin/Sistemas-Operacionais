#include "request.h"
#include "scheduler.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static OsRequest ready(uint32_t id, uint32_t size, uint64_t ready_tick, uint64_t order) {
    OsRequest r;
    assert(os_request_init(&r, id, ready_tick, size, 0));
    assert(os_request_transition(&r, OS_READY));
    r.ready_tick = ready_tick;
    r.ready_order = order;
    return r;
}

/* Returns the chosen ID and checks that array order is irrelevant. */
static uint32_t hrrn(OsRequest a, OsRequest b, uint64_t now) {
    const OsRequest ab[2] = {a, b}, ba[2] = {b, a};
    const uint32_t first = ab[os_scheduler_select(ab, 2, OS_HRRN, now, 0)].id;
    assert(ba[os_scheduler_select(ba, 2, OS_HRRN, now, 0)].id == first);
    return first;
}

static void test_hrrn(void) {
    /* 40/10 -> ratio 5.0 beats 6/2 -> ratio 4.0, although SJF picks the shorter. */
    const OsRequest pair[2] = {ready(1, 10, 0, 0), ready(2, 2, 34, 1)};
    assert(hrrn(pair[0], pair[1], 40) == 1);
    assert(os_scheduler_select(pair, 2, OS_SJF, 40, 0) == 1);
    /* With little waiting, the shorter one wins: 10/10 -> 2.0 vs 4/2 -> 3.0. */
    assert(hrrn(ready(1, 10, 0, 0), ready(2, 2, 6, 1), 10) == 2);
    /* Equal waits favour the shorter. */
    assert(hrrn(ready(1, 10, 0, 0), ready(2, 2, 0, 1), 10) == 2);
    /* Equal ratios (10/10 and 2/2): ready_order first, then ID. */
    assert(hrrn(ready(1, 10, 0, 5), ready(2, 2, 8, 4), 10) == 2);
    assert(hrrn(ready(1, 10, 0, 4), ready(2, 2, 8, 4), 10) == 1);
    /* No waiting at all (also a ready_tick in the future): ratio 1 for both. */
    assert(hrrn(ready(1, 10, 10, 0), ready(2, 2, 10, 1), 10) == 1);
    assert(hrrn(ready(1, 10, 99, 0), ready(2, 2, 10, 1), 10) == 1);
    /* 2^33 * 2^31 = 2^64 would wrap to zero in a 64-bit product. */
    const uint64_t now = UINT64_C(1) << 33;
    assert(hrrn(ready(1, 1, 0, 1), ready(2, UINT32_C(1) << 31, now - 1, 0), now) == 1);
    /* Largest wait and size: (wait + size) would overflow uint64_t too. */
    assert(hrrn(ready(1, UINT32_MAX, 0, 0), ready(2, UINT32_MAX - 1, 0, 1), UINT64_MAX) == 2);
    /* Differ by one tick above 2^53: indistinguishable in double. */
    assert(hrrn(ready(1, 3, 1, 0), ready(2, 3, 0, 1), (UINT64_C(1) << 60)) == 2);
}

int main(void) {
    test_hrrn();
    OsRequest a;
    assert(!os_request_init(NULL, 1, 0, 1, 0));
    assert(!os_request_init(&a, 1, 0, 0, 0));
    assert(os_request_init(&a, 1, 0, 5, 1));
    assert(a.state == OS_NEW && a.first_run_tick == OS_REQUEST_NEVER);
    assert(!os_request_transition(&a, OS_TERMINATED));
    assert(os_request_transition(&a, OS_READY));
    assert(os_request_transition(&a, OS_RUNNING));
    assert(os_request_transition(&a, OS_BLOCKED));
    assert(os_request_transition(&a, OS_READY));
    assert(os_request_transition(&a, OS_RUNNING));
    assert(os_request_transition(&a, OS_TERMINATED));
    assert(!os_request_transition(&a, OS_READY));

    OsRequest r[2];
    assert(os_request_init(&r[0], 1, 0, 10, 0));
    assert(os_request_init(&r[1], 2, 1, 2, 1));
    assert(os_request_transition(&r[0], OS_READY));
    assert(os_request_transition(&r[1], OS_READY));
    r[0].ready_tick = 0; r[0].ready_order = 0;
    r[1].ready_tick = 1; r[1].ready_order = 1;
    assert(os_scheduler_select(r, 2, OS_FCFS, 10, 2) == 0);
    assert(os_scheduler_select(r, 2, OS_SJF, 10, 2) == 1);
    assert(os_scheduler_select(r, 2, OS_ROUND_ROBIN, 10, 2) == 0);
    assert(os_scheduler_select(r, 2, OS_PRIORITY_AGING, 10, 2) == 0);
    assert(os_scheduler_select(r, 2, OS_PRIORITY_AGING, 10, 0) == 1);
    assert(os_scheduler_select(r, 2, OS_PRIORITY_AGING, 1000000000000ULL, 1) == 0);
    assert(os_scheduler_select(r, 2, (OsSchedulingPolicy)(OS_HRRN + 1), 10, 2) == SIZE_MAX);
    assert(os_request_transition(&r[0], OS_RUNNING));
    assert(os_request_transition(&r[1], OS_RUNNING));
    assert(os_scheduler_select(r, 2, OS_FCFS, 10, 2) == SIZE_MAX);
    return 0;
}
