#include "parallel_runtime.h"
#include "trace.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define REQUESTS 12u

static void make_requests(OsRequest *requests) {
    for (size_t i = 0; i < REQUESTS; ++i)
        assert(os_request_init(&requests[i], (uint32_t)(i + 1),
            (uint64_t)(i % 4), (uint32_t)(2 + i % 7),
            (uint32_t)(2 + i % 7), (int)(i % 3)));
}

static void compare_runs(size_t workers, OsSchedulingPolicy policy,
                         bool paging, uint64_t latency) {
    OsRequest virtual_requests[REQUESTS];
    OsRequest posix_requests[REQUESTS];
    make_requests(virtual_requests);
    make_requests(posix_requests);
    const OsVirtualConfig config = {
        .policy = policy, .worker_count = workers, .quantum_tokens = 2,
        .aging_interval = 2, .memory_enabled = paging,
        .frame_capacity = 3, .tokens_per_page = 2,
        .page_policy = OS_PAGE_LRU, .page_fault_latency_ticks = latency,
        .starvation_threshold_ticks = 3
    };
    OsRunSummary virtual_summary = {0};
    OsRunSummary posix_summary = {0};
    OsEventTrace virtual_trace;
    OsEventTrace posix_trace;
    os_trace_init(&virtual_trace);
    os_trace_init(&posix_trace);
    assert(os_virtual_run_observed(virtual_requests, REQUESTS, &config,
           &virtual_summary, os_trace_capture, &virtual_trace) == OS_RUN_OK);
    assert(os_posix_run(posix_requests, REQUESTS, &config, &posix_summary,
           os_trace_capture, &posix_trace) == OS_RUN_OK);
    assert(!virtual_trace.failed && !posix_trace.failed);
    assert(os_trace_equal(&virtual_trace, &posix_trace));
    assert(os_trace_fingerprint(&virtual_trace) == os_trace_fingerprint(&posix_trace));
    assert(posix_summary.completed == REQUESTS);
    assert(posix_summary.completed == virtual_summary.completed);
    assert(posix_summary.makespan_ticks == virtual_summary.makespan_ticks);
    assert(posix_summary.page_hits == virtual_summary.page_hits);
    assert(posix_summary.page_faults == virtual_summary.page_faults);
    assert(posix_summary.page_evictions == virtual_summary.page_evictions);
    assert(posix_summary.preemptions == virtual_summary.preemptions);
    assert(posix_summary.total_blocked_ticks == virtual_summary.total_blocked_ticks);
    assert(posix_summary.average_wait_ticks == virtual_summary.average_wait_ticks);
    assert(posix_summary.worker_utilization == virtual_summary.worker_utilization);
    for (size_t i = 0; i < REQUESTS; ++i) {
        assert(posix_requests[i].state == OS_TERMINATED);
        assert(posix_requests[i].id == virtual_requests[i].id);
        assert(posix_requests[i].generated_tokens == virtual_requests[i].generated_tokens);
        assert(posix_requests[i].first_run_tick == virtual_requests[i].first_run_tick);
        assert(posix_requests[i].finished_tick == virtual_requests[i].finished_tick);
        assert(posix_requests[i].waiting_ticks == virtual_requests[i].waiting_ticks);
    }
    os_trace_destroy(&virtual_trace);
    os_trace_destroy(&posix_trace);
}

int main(void) {
    assert(os_posix_run(NULL, 0, NULL, NULL, NULL, NULL) == OS_RUN_INVALID);
    for (size_t workers = 1; workers <= 4; workers *= 2) {
        compare_runs(workers, OS_FCFS, false, 0);
        compare_runs(workers, OS_ROUND_ROBIN, true, 0);
        compare_runs(workers, OS_PRIORITY_AGING, true, 2);
    }
    return 0;
}
