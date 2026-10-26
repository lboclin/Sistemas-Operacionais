#include "session.h"
#include "frames.h"
#include "request.h"
#include <assert.h>
#include <stdint.h>

static void physical_slot_contract(void) {
    OsFrameCache cache = {0};
    assert(os_frames_init(&cache, 1, OS_PAGE_FIFO));
    OsPageReference sentinel = {.request_id = 77, .page_id = 88};
    assert(!os_frames_page_at(&cache, 0, &sentinel));
    assert(sentinel.request_id == 77 && sentinel.page_id == 88);
    OsFrameAccess access;
    const OsPageReference first = {.request_id = 1, .page_id = 0};
    assert(os_frames_access(&cache, first, &access));
    assert(os_frames_page_at(&cache, 0, &sentinel));
    assert(sentinel.request_id == 1 && sentinel.page_id == 0);
    assert(!os_frames_page_at(&cache, 1, &sentinel));
    assert(!os_frames_page_at(&cache, 0, NULL));
    assert(os_frames_release(&cache, 1) == 1);
    assert(!os_frames_page_at(&cache, 0, &sentinel));
    os_frames_destroy(&cache);
}

static void internal_fragmentation_golden(void) {
    OsRequest r;
    assert(os_request_init(&r, 5, 0, 4, 4, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .memory_enabled = true, .frame_capacity = 2, .tokens_per_page = 2,
        .page_policy = OS_PAGE_FIFO};
    OsVirtualSession *s = NULL;
    assert(os_session_create(&r, 1, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    const uint64_t expected_waste[] = {1, 0, 1, 0};
    const size_t expected_free[] = {1, 1, 0, 2};
    for (size_t i = 0; i < 4; ++i) {
        assert(os_session_step(s) == OS_RUN_OK);
        OsSessionSnapshot snap;
        assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
        assert(snap.internal_fragmentation_tokens == expected_waste[i]);
        assert(snap.free_frames == expected_free[i]);
        assert(snap.metrics.page_evictions == 0);
    }
    os_session_destroy(s);
}

static void eviction_under_pressure(void) {
    OsRequest r[2];
    assert(os_request_init(&r[0], 1, 0, 2, 2, 0));
    assert(os_request_init(&r[1], 2, 0, 2, 2, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 2,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 2,
        .page_policy = OS_PAGE_FIFO};
    OsVirtualSession *s = NULL;
    assert(os_session_create(r, 2, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    assert(os_session_step(s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.metrics.page_faults == 2 && snap.metrics.page_evictions == 1);
    assert(snap.resident_frames == 1 && snap.free_frames == 0);
    assert(snap.internal_fragmentation_tokens == 1);
    assert(os_session_run(s) == OS_RUN_OK);
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.finished && snap.free_frames == 1 &&
           snap.internal_fragmentation_tokens == 0);
    os_session_destroy(s);
}

static void reserved_page_during_io(void) {
    OsRequest r;
    assert(os_request_init(&r, 9, 0, 1, 1, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
        .memory_enabled = true, .frame_capacity = 1, .tokens_per_page = 3,
        .page_policy = OS_PAGE_LRU, .page_fault_latency_ticks = 2};
    OsVirtualSession *s = NULL;
    assert(os_session_create(&r, 1, &cfg, NULL, NULL, &s) == OS_RUN_OK);
    assert(os_session_step(s) == OS_RUN_OK);
    OsSessionSnapshot snap;
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.blocked == 1 && snap.internal_fragmentation_tokens == 3);
    assert(snap.free_frames == 0);
    assert(os_session_run(s) == OS_RUN_OK);
    assert(os_session_snapshot(s, &snap, NULL, 0, NULL, 0));
    assert(snap.finished && snap.internal_fragmentation_tokens == 0);
    os_session_destroy(s);
}

int main(void) {
    physical_slot_contract();
    internal_fragmentation_golden();
    eviction_under_pressure();
    reserved_page_during_io();
    return 0;
}
