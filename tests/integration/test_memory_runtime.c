#include "request.h"
#include "virtual.h"

#include <assert.h>
#include <stdint.h>

static void init(OsRequest *r) {
    assert(os_request_init(&r[0], 10, 0, 3, 3, 0));
    assert(os_request_init(&r[1], 11, 0, 3, 3, 0));
}

int main(void) {
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 2,
        .memory_enabled = true, .frame_capacity = 2, .tokens_per_page = 2,
        .page_policy = OS_PAGE_FIFO};
    OsRunSummary summary;
    OsRequest r[2];
    init(r);
    assert(os_virtual_run(r, 2, &cfg, &summary) == OS_RUN_OK);
    assert(summary.completed == 2 && summary.makespan_ticks == 3);
    assert(summary.page_hits == 2 && summary.page_faults == 4 &&
           summary.page_evictions == 2 && summary.peak_resident_frames == 2);
    cfg.page_policy = OS_PAGE_LRU;
    init(r);
    assert(os_virtual_run(r, 2, &cfg, &summary) == OS_RUN_OK);
    assert(summary.page_hits == 2 && summary.page_faults == 4);
    cfg.frame_capacity = 4;
    init(r);
    assert(os_virtual_run(r, 2, &cfg, &summary) == OS_RUN_OK);
    assert(summary.page_evictions == 0 && summary.peak_resident_frames == 4);
    cfg.page_policy = OS_PAGE_OPT;
    init(r);
    assert(os_virtual_run(r, 2, &cfg, &summary) == OS_RUN_INVALID);
    cfg.page_policy = OS_PAGE_FIFO;
    cfg.tokens_per_page = 0;
    init(r);
    assert(os_virtual_run(r, 2, &cfg, &summary) == OS_RUN_INVALID);
    cfg.memory_enabled = false;
    init(r);
    assert(os_virtual_run(r, 2, &cfg, &summary) == OS_RUN_OK);
    assert(summary.page_faults == 0 && summary.peak_resident_frames == 0);
    return 0;
}
