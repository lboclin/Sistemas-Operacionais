#include "session.h"
#include "request.h"
#include <assert.h>
#include <stdint.h>

static void nearest_rank_golden(void) {
    OsRequest r[3];
    assert(os_request_init(&r[0], 1, 0, 1, 1, 0));
    assert(os_request_init(&r[1], 2, 0, 2, 2, 0));
    assert(os_request_init(&r[2], 3, 0, 4, 4, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .starvation_threshold_ticks = 2};
    OsVirtualSession *s = NULL;
    assert(os_session_create(r, 3, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    assert(os_session_run(s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.finished && snap.tick == 7);
    assert(snap.metrics.p50_wait_ticks == 1 && snap.metrics.p95_wait_ticks == 3);
    assert(snap.metrics.p50_response_ticks == 1 && snap.metrics.p95_response_ticks == 3);
    assert(snap.metrics.p50_turnaround_ticks == 3 && snap.metrics.p95_turnaround_ticks == 7);
    assert(snap.metrics.max_wait_ticks == 3 && snap.metrics.starvation_count == 1);
    assert(snap.metrics.worker_utilization == 1.0);
    assert(snap.metrics.preemptions == 0 && snap.metrics.total_blocked_ticks == 0);
    os_session_destroy(s);
}

static void starvation_and_preemption(void) {
    OsRequest r[2];
    assert(os_request_init(&r[0], 1, 0, 2, 2, 0));
    assert(os_request_init(&r[1], 2, 0, 2, 2, 0));
    OsVirtualConfig cfg = {.policy = OS_ROUND_ROBIN, .worker_count = 1,
        .quantum_tokens = 1, .starvation_threshold_ticks = 1};
    OsVirtualSession *s = NULL;
    assert(os_session_create(r, 2, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.metrics.p95_wait_ticks == 0 && snap.metrics.worker_utilization == 0);
    assert(os_session_step(s) == OS_RUN_OK);
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.tick == 1 && snap.starving_ready == 1 &&
           snap.max_ready_age_ticks == 1 && snap.metrics.preemptions == 1);
    assert(os_session_run(s) == OS_RUN_OK);
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.metrics.preemptions == 2 && snap.metrics.worker_utilization == 1.0);
    assert(snap.metrics.starvation_count == 2);
    os_session_destroy(s);
}

static void blocked_time_and_utilization(void) {
    OsRequest r;
    assert(os_request_init(&r, 1, 0, 1, 1, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 1,
        .page_policy = OS_PAGE_FIFO, .page_fault_latency_ticks = 2};
    OsVirtualSession *s = NULL;
    assert(os_session_create(&r, 1, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    assert(os_session_step(s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.blocked == 1 && snap.metrics.total_blocked_ticks == 1);
    assert(snap.metrics.worker_utilization == 0);
    assert(os_session_run(s) == OS_RUN_OK);
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.tick == 3 && snap.metrics.total_blocked_ticks == 2);
    assert(snap.metrics.worker_utilization > 0.333 &&
           snap.metrics.worker_utilization < 0.334);
    assert(snap.metrics.p50_response_ticks == 2 &&
           snap.metrics.p95_turnaround_ticks == 3);
    os_session_destroy(s);
}

int main(void) {
    nearest_rank_golden();
    starvation_and_preemption();
    blocked_time_and_utilization();
    return 0;
}
