#include "request.h"
#include "scheduler.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

int main(void) {
    OsRequest a;
    assert(!os_request_init(NULL, 1, 0, 1, 1, 0));
    assert(!os_request_init(&a, 1, 0, 0, 1, 0));
    assert(os_request_init(&a, 1, 0, 5, 4, 1));
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
    assert(os_request_init(&r[0], 1, 0, 10, 7, 0));
    assert(os_request_init(&r[1], 2, 1, 2, 3, 1));
    assert(os_request_transition(&r[0], OS_READY));
    assert(os_request_transition(&r[1], OS_READY));
    r[0].ready_tick = 0; r[0].ready_order = 0;
    r[1].ready_tick = 1; r[1].ready_order = 1;
    assert(os_scheduler_select(r, 2, OS_FCFS, 10, 2) == 0);
    assert(os_scheduler_select(r, 2, OS_SJF_ESTIMATED, 10, 2) == 1);
    assert(os_scheduler_select(r, 2, OS_SJF_ORACLE, 10, 2) == 1);
    assert(os_scheduler_select(r, 2, OS_ROUND_ROBIN, 10, 2) == 0);
    assert(os_scheduler_select(r, 2, OS_PRIORITY_AGING, 10, 2) == 0);
    assert(os_scheduler_select(r, 2, OS_PRIORITY_AGING, 10, 0) == 1);
    assert(os_scheduler_select(r, 2, OS_PRIORITY_AGING, 1000000000000ULL, 1) == 0);
    assert(os_request_transition(&r[0], OS_RUNNING));
    assert(os_request_transition(&r[1], OS_RUNNING));
    assert(os_scheduler_select(r, 2, OS_FCFS, 10, 2) == SIZE_MAX);
    return 0;
}
