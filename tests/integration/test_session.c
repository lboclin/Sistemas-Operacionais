#include "session.h"
#include "gateway.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    OsEvent events[256];
    size_t count;
} Trace;

static void capture(const OsEvent *event, void *ctx) {
    Trace *t = ctx;
    assert(event != NULL && t->count < 256);
    t->events[t->count++] = *event;
}

static void prepare(OsRequest *r) {
    assert(os_request_init(&r[0], 11, 0, 5, 6, 0));
    assert(os_request_init(&r[1], 22, 0, 3, 1, 4));
    assert(os_request_init(&r[2], 33, 2, 4, 3, 2));
    assert(os_request_init(&r[3], 44, 12, 1, 1, 1));
}

static void exercise(OsSchedulingPolicy policy, bool use_memory) {
    OsRequest input[4];
    prepare(input);
    const OsVirtualConfig cfg = {.policy = policy, .worker_count = 2,
        .quantum_tokens = 2, .aging_interval = 2, .memory_enabled = use_memory,
        .frame_capacity = 2, .tokens_per_page = 2, .page_policy = OS_PAGE_LRU};
    OsVirtualSession *session = NULL;
    Trace stepped = {0}, complete = {0};
    assert(os_session_create(input, 4, &cfg, capture, &stepped, &session) == OS_RUN_OK);
    assert(input[0].state == OS_NEW); /* owned copy, no alias */
    OsSessionSnapshot snapshot;
    OsWorkerSnapshot workers[2];
    OsRequest viewed[4];
    assert(os_session_snapshot(session, &snapshot, viewed, 4, workers, 2));
    assert(snapshot.tick == 0 && snapshot.pending == 4 && !snapshot.finished);
    size_t guard = 0;
    while (!snapshot.finished) {
        const uint64_t before = snapshot.tick;
        assert(os_session_step(session) == OS_RUN_OK);
        assert(os_session_snapshot(session, &snapshot, viewed, 4, workers, 2));
        assert(snapshot.tick > before);
        assert(snapshot.pending + snapshot.ready + snapshot.running +
               snapshot.blocked + snapshot.completed == 4);
        assert(snapshot.ready + snapshot.running + snapshot.completed <= 4);
        assert(++guard < 50);
    }
    assert(snapshot.completed == 4 && snapshot.metrics.completed == 4);
    assert(os_session_step(session) == OS_RUN_OK); /* finished is idempotent */
    OsSessionSnapshot sentinel = {.tick = 999};
    assert(!os_session_snapshot(session, &sentinel, viewed, 3, workers, 2));
    assert(sentinel.tick == 999);
    assert(!os_session_snapshot(session, &sentinel, viewed, 4, workers, 1));
    assert(sentinel.tick == 999);
    assert(!os_session_snapshot(session, &sentinel, NULL, 2, NULL, 0));
    assert(sentinel.tick == 999);
    assert(os_session_snapshot(session, &snapshot, NULL, 0, NULL, 0));
    const uint64_t makespan = snapshot.tick;
    const OsRunSummary summary = snapshot.metrics;
    OsRequest batch[4];
    prepare(batch);
    OsRunSummary batch_metrics;
    assert(os_virtual_run_observed(batch, 4, &cfg, &batch_metrics, capture, &complete) == OS_RUN_OK);
    assert(makespan == batch_metrics.makespan_ticks);
    assert(summary.completed == batch_metrics.completed);
    assert(summary.average_wait_ticks == batch_metrics.average_wait_ticks);
    assert(summary.page_faults == batch_metrics.page_faults);
    assert(summary.page_hits == batch_metrics.page_hits);
    assert(summary.page_evictions == batch_metrics.page_evictions);
    assert(stepped.count == complete.count);
    for (size_t i = 0; i < stepped.count; ++i) {
        assert(stepped.events[i].tick == complete.events[i].tick);
        assert(stepped.events[i].type == complete.events[i].type);
        assert(stepped.events[i].request_id == complete.events[i].request_id);
        assert(stepped.events[i].worker_index == complete.events[i].worker_index);
        assert(stepped.events[i].page_id == complete.events[i].page_id);
    }
    for (size_t i = 0; i < 4; ++i) {
        assert(viewed[i].state == batch[i].state);
        assert(viewed[i].first_run_tick == batch[i].first_run_tick);
        assert(viewed[i].finished_tick == batch[i].finished_tick);
        assert(viewed[i].waiting_ticks == batch[i].waiting_ticks);
        assert(viewed[i].generated_tokens == batch[i].generated_tokens);
    }
    os_session_destroy(session);
}

static void snapshot_mid_run(void) {
    OsRequest req;
    assert(os_request_init(&req, 77, 0, 3, 3, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 2,
        .page_policy = OS_PAGE_FIFO};
    OsVirtualSession *session = NULL;
    Trace trace = {0};
    assert(os_session_create(&req, 1, &cfg, capture, &trace, &session) == OS_RUN_OK);
    OsSessionSnapshot snap; OsRequest current; OsWorkerSnapshot worker;
    assert(os_session_step(session) == OS_RUN_OK);
    assert(os_session_snapshot(session, &snap, &current, 1, &worker, 1));
    assert(snap.tick == 1 && snap.running == 1 && !snap.finished);
    assert(worker.occupied && worker.request_id == 77 && current.generated_tokens == 1);
    assert(snap.resident_frames == 1 && snap.metrics.page_faults == 1);
    assert(os_session_step(session) == OS_RUN_OK);
    assert(os_session_snapshot(session, &snap, &current, 1, &worker, 1));
    assert(snap.tick == 2 && snap.metrics.page_hits == 1 && current.generated_tokens == 2);
    assert(os_session_step(session) == OS_RUN_OK);
    assert(os_session_snapshot(session, &snap, &current, 1, &worker, 1));
    assert(snap.tick == 3 && snap.completed == 1 && snap.finished);
    assert(snap.metrics.page_faults == 2 && snap.metrics.page_evictions == 1);
    assert(snap.resident_frames == 0 && !worker.occupied);
    assert(trace.count == 7);
    os_session_destroy(session);
}

int main(void) {
    snapshot_mid_run();
    exercise(OS_FCFS, false);
    exercise(OS_SJF_ESTIMATED, true);
    exercise(OS_SJF_ORACLE, false);
    exercise(OS_ROUND_ROBIN, true);
    exercise(OS_PRIORITY_AGING, true);
    OsRequest invalid[4];
    prepare(invalid);
    invalid[1].id = invalid[0].id;
    const OsVirtualConfig cfg = {.policy = OS_ROUND_ROBIN, .worker_count = 1, .quantum_tokens = 1};
    OsVirtualSession *s = (OsVirtualSession *)(uintptr_t)1;
    assert(os_session_create(invalid, 2, &cfg, NULL, NULL, &s) == OS_RUN_INVALID && s == NULL);
    assert(os_session_step(NULL) == OS_RUN_INVALID);
    assert(os_session_run(NULL) == OS_RUN_INVALID);
    os_session_destroy(NULL);
    return 0;
}
