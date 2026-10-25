#include "summary.h"

OsRunSummary os_metrics_summarize(const OsRequest *requests, size_t count,
                                  uint64_t makespan_ticks) {
    OsRunSummary result = {.makespan_ticks = makespan_ticks};
    if (requests == NULL) return result;
    for (size_t i = 0; i < count; ++i) {
        const OsRequest *r = &requests[i];
        if (r->state != OS_TERMINATED || r->first_run_tick == OS_REQUEST_NEVER ||
            r->finished_tick == OS_REQUEST_NEVER) continue;
        ++result.completed;
        result.average_wait_ticks += (double)r->waiting_ticks;
        result.average_response_ticks += (double)(r->first_run_tick - r->arrival_tick);
        result.average_turnaround_ticks += (double)(r->finished_tick - r->arrival_tick);
    }
    if (result.completed != 0) {
        result.average_wait_ticks /= (double)result.completed;
        result.average_response_ticks /= (double)result.completed;
        result.average_turnaround_ticks /= (double)result.completed;
    }
    if (makespan_ticks != 0) result.throughput_per_tick = (double)result.completed / (double)makespan_ticks;
    return result;
}
