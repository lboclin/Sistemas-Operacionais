#include "session.h"
#include "request.h"
#include <assert.h>
#include <stdint.h>

typedef struct {
    OsEventType types[128];
    uint64_t ticks[128];
    size_t count;
} Trace;

static void capture(const OsEvent *e, void *ctx) {
    Trace *trace = ctx;
    assert(trace->count < 128);
    trace->types[trace->count] = e->type;
    trace->ticks[trace->count++] = e->tick;
}

static void delayed_fault_and_wakeup(void) {
    OsRequest input;
    assert(os_request_init(&input, 7, 0, 1, 1, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 1,
        .page_policy = OS_PAGE_FIFO, .page_fault_latency_ticks = 2};
    OsVirtualSession *s = NULL;
    Trace trace = {0};
    assert(os_session_create(&input, 1, &cfg, capture, &trace, &s) == OS_RUN_OK);
    assert(os_session_step(s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    OsRequest current;
    OsWorkerSnapshot worker;
    assert(os_session_snapshot(s, &snap, &current, 1, &worker, 1));
    assert(snap.tick == 1 && snap.blocked == 1 && snap.running == 0);
    assert(!worker.occupied && current.generated_tokens == 0);
    assert(current.first_run_tick == OS_REQUEST_NEVER);
    assert(snap.metrics.page_faults == 1 && snap.next_wakeup_tick == 2);
    uint64_t wake = 123;
    assert(os_session_request_wakeup(s, 7, &wake) && wake == 2);
    assert(!os_session_request_wakeup(s, 8, &wake) && wake == 2);
    assert(!os_session_request_wakeup(s, 7, NULL));
    assert(os_session_step(s) == OS_RUN_OK);
    assert(os_session_snapshot(s, &snap, &current, 1, &worker, 1));
    assert(snap.tick == 3 && snap.completed == 1 && snap.finished);
    assert(snap.next_wakeup_tick == OS_REQUEST_NEVER);
    assert(current.first_run_tick == 2 && current.finished_tick == 3);
    assert(snap.metrics.page_faults == 1 && snap.metrics.page_hits == 1);
    assert(os_session_request_wakeup(s, 7, &wake) && wake == OS_REQUEST_NEVER);
    assert(trace.count == 8);
    const OsEventType expected[] = {
        OS_EVENT_ARRIVED, OS_EVENT_DISPATCHED, OS_EVENT_PAGE_FAULT,
        OS_EVENT_BLOCKED, OS_EVENT_WOKEN, OS_EVENT_DISPATCHED,
        OS_EVENT_PAGE_HIT, OS_EVENT_COMPLETED
    };
    const uint64_t expected_ticks[] = {0, 0, 0, 0, 2, 2, 2, 3};
    for (size_t i = 0; i < 8; ++i) {
        assert(trace.types[i] == expected[i] && trace.ticks[i] == expected_ticks[i]);
    }
    assert(os_session_step(s) == OS_RUN_OK);
    os_session_destroy(s);
}

static void zero_cost_and_validation(void) {
    OsRequest input;
    assert(os_request_init(&input, 3, 0, 1, 1, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 1,
        .page_policy = OS_PAGE_FIFO};
    OsVirtualSession *s = NULL;
    assert(os_session_create(&input, 1, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    assert(os_session_step(s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.tick == 1 && snap.finished && snap.blocked == 0);
    assert(snap.metrics.page_faults == 1 && snap.metrics.page_hits == 0);
    os_session_destroy(s);
    cfg.memory_enabled = false;
    cfg.page_fault_latency_ticks = 1;
    assert(os_session_create(&input, 1, &cfg, NULL, NULL, &s) == OS_RUN_INVALID);
}

static void contention_finishes(void) {
    OsRequest req[2];
    assert(os_request_init(&req[0], 1, 0, 2, 2, 0));
    assert(os_request_init(&req[1], 2, 0, 2, 2, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 2,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 2,
        .page_policy = OS_PAGE_LRU, .page_fault_latency_ticks = 1};
    OsVirtualSession *s = NULL;
    assert(os_session_create(req, 2, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    assert(os_session_step(s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.blocked == 2 && snap.tick == 1);
    for (int i = 0; i < 8 && !snap.finished; ++i) {
        assert(os_session_step(s) == OS_RUN_OK);
        assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    }
    assert(snap.finished && snap.completed == 2 && snap.resident_frames == 0);
    os_session_destroy(s);
}

static void overflow(void) {
    OsRequest input;
    assert(os_request_init(&input, 1, 0, 1, 1, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 1,
        .page_policy = OS_PAGE_FIFO, .page_fault_latency_ticks = UINT64_MAX};
    OsVirtualSession *s = NULL;
    assert(os_session_create(&input, 1, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    assert(os_session_step(s) == OS_RUN_OVERFLOW);
    os_session_destroy(s);
}

int main(void) {
    delayed_fault_and_wakeup();
    zero_cost_and_validation();
    contention_finishes();
    overflow();
    return 0;
}
