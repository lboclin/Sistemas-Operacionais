#include "session.h"
#include "request.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static void prepare(OsRequest *r) {
    assert(os_request_init(&r[0], 1, 0, 3, 3, 0));
    assert(os_request_init(&r[1], 2, 0, 3, 3, 0));
    assert(os_request_init(&r[2], 3, 5, 1, 1, 0));
}

static void controls_and_snapshots(void) {
    OsRequest input[3];
    prepare(input);
    const OsVirtualConfig cfg = {.policy = OS_ROUND_ROBIN, .worker_count = 2,
        .quantum_tokens = 2, .memory_enabled = true, .frame_capacity = 2,
        .tokens_per_page = 2, .page_policy = OS_PAGE_LRU};
    OsVirtualSession *session = NULL;
    assert(os_session_create(input, 3, &cfg, NULL, NULL, &session) == OS_RUN_OK);
    OsSessionSnapshot snap = {0};
    OsRequest view[3];
    OsWorkerSnapshot workers[2];
    assert(os_session_snapshot(session, &snap, view, 3, workers, 2));
    assert(snap.tick == 0 && snap.pending == 3 && snap.completed == 0);
    assert(snap.worker_count == 2 && !workers[0].occupied && !workers[1].occupied);
    assert(input[0].state == OS_NEW);

    OsSessionSnapshot untouched = {.tick = 1234};
    OsRequest unchanged[3];
    memset(unchanged, 0x4f, sizeof unchanged);
    OsRequest copy[3];
    memcpy(copy, unchanged, sizeof copy);
    assert(!os_session_snapshot(session, &untouched, unchanged, 2, workers, 2));
    assert(untouched.tick == 1234 && memcmp(copy, unchanged, sizeof copy) == 0);
    assert(!os_session_snapshot(session, &untouched, unchanged, 3, workers, 1));
    assert(!os_session_snapshot(session, &untouched, NULL, 1, NULL, 0));
    assert(!os_session_snapshot(session, &untouched, NULL, 0, NULL, 1));
    assert(!os_session_snapshot(session, NULL, NULL, 0, NULL, 0));
    assert(untouched.tick == 1234);

    assert(os_session_step(session) == OS_RUN_OK);
    assert(os_session_snapshot(session, &snap, view, 3, workers, 2));
    assert(snap.tick == 1 && snap.pending == 1 && snap.running == 2);
    assert(snap.metrics.page_faults == 2 && snap.resident_frames == 2);
    assert(workers[0].occupied && workers[0].request_id == 1 && workers[0].quantum_used == 1);
    assert(workers[1].occupied && workers[1].request_id == 2 && workers[1].quantum_used == 1);
    assert(view[0].generated_tokens == 1 && view[1].generated_tokens == 1);
    assert(input[0].generated_tokens == 0);

    assert(os_session_step(session) == OS_RUN_OK);
    assert(os_session_snapshot(session, &snap, view, 3, workers, 2));
    assert(snap.tick == 2 && snap.ready == 2 && snap.running == 0 && snap.pending == 1);
    assert(snap.metrics.page_hits == 2);
    assert(!workers[0].occupied && !workers[1].occupied);

    assert(os_session_run(session) == OS_RUN_OK);
    assert(os_session_snapshot(session, &snap, view, 3, workers, 2));
    assert(snap.finished && snap.completed == 3 && snap.tick == 6);
    assert(snap.metrics.page_faults == 5 && snap.metrics.page_hits == 2);
    assert(snap.resident_frames == 0 && snap.metrics.completed == 3);
    assert(os_session_step(session) == OS_RUN_OK);
    assert(os_session_run(session) == OS_RUN_OK);
    OsSessionSnapshot again;
    assert(os_session_snapshot(session, &again, NULL, 0, NULL, 0));
    assert(again.tick == snap.tick && again.metrics.page_faults == snap.metrics.page_faults);
    os_session_destroy(session);

    OsRequest batch[3];
    prepare(batch);
    OsRunSummary summary;
    assert(os_virtual_run(batch, 3, &cfg, &summary) == OS_RUN_OK);
    assert(summary.makespan_ticks == snap.tick && summary.completed == snap.completed);
    assert(summary.page_hits == snap.metrics.page_hits && summary.page_faults == snap.metrics.page_faults);
    for (size_t i = 0; i < 3; ++i)
        assert(batch[i].generated_tokens == view[i].generated_tokens && batch[i].finished_tick == view[i].finished_tick);
}

static void idle_jump_and_errors(void) {
    OsRequest late;
    assert(os_request_init(&late, 42, 10, 1, 1, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1};
    OsVirtualSession *session = NULL;
    assert(os_session_create(&late, 1, &cfg, NULL, NULL, &session) == OS_RUN_OK);
    assert(os_session_step(session) == OS_RUN_OK);
    OsSessionSnapshot snap;
    OsRequest view;
    assert(os_session_snapshot(session, &snap, &view, 1, NULL, 0));
    assert(snap.tick == 11 && snap.finished && view.first_run_tick == 10);
    assert(os_session_step(session) == OS_RUN_OK);
    assert(os_session_snapshot(session, &snap, NULL, 0, NULL, 0) && snap.tick == 11);
    os_session_destroy(session);

    assert(os_session_step(NULL) == OS_RUN_INVALID);
    assert(os_session_run(NULL) == OS_RUN_INVALID);
    assert(!os_session_snapshot(NULL, &snap, NULL, 0, NULL, 0));
    assert(os_session_create(&late, 1, &cfg, NULL, NULL, NULL) == OS_RUN_INVALID);
    session = (OsVirtualSession *)(uintptr_t)1;
    assert(os_session_create(NULL, 1, &cfg, NULL, NULL, &session) == OS_RUN_INVALID && session == NULL);
    assert(os_session_create(&late, 0, &cfg, NULL, NULL, &session) == OS_RUN_INVALID && session == NULL);
    cfg.worker_count = 0;
    assert(os_session_create(&late, 1, &cfg, NULL, NULL, &session) == OS_RUN_INVALID);
    cfg.worker_count = 1;
    cfg.policy = OS_ROUND_ROBIN;
    assert(os_session_create(&late, 1, &cfg, NULL, NULL, &session) == OS_RUN_INVALID);
    cfg.quantum_tokens = 1;
    cfg.memory_enabled = true;
    cfg.frame_capacity = 1;
    cfg.tokens_per_page = 1;
    cfg.page_policy = OS_PAGE_OPT;
    assert(os_session_create(&late, 1, &cfg, NULL, NULL, &session) == OS_RUN_INVALID);
    cfg.page_policy = OS_PAGE_FIFO;
    cfg.tokens_per_page = 0;
    assert(os_session_create(&late, 1, &cfg, NULL, NULL, &session) == OS_RUN_INVALID);
    cfg.memory_enabled = false;
    cfg.policy = OS_FCFS;
    OsRequest dup[2] = {late, late};
    assert(os_session_create(dup, 2, &cfg, NULL, NULL, &session) == OS_RUN_INVALID);
    OsRequest huge;
    assert(os_request_init(&huge, 9, UINT64_MAX - 1, 2, 2, 0));
    assert(os_session_create(&huge, 1, &cfg, NULL, NULL, &session) == OS_RUN_OVERFLOW);
    os_session_destroy(NULL);
}

int main(void) {
    controls_and_snapshots();
    idle_jump_and_errors();
    return 0;
}
