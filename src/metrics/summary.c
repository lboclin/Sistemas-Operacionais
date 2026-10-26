#include "summary.h"

typedef enum { WAIT, RESPONSE, TURNAROUND } Metric;

static bool completed(const OsRequest *r) {
    return r->state == OS_TERMINATED &&
           r->first_run_tick != OS_REQUEST_NEVER &&
           r->finished_tick != OS_REQUEST_NEVER;
}

static uint64_t value(const OsRequest *r, Metric metric) {
    switch (metric) {
        case WAIT: return r->waiting_ticks;
        case RESPONSE: return r->first_run_tick - r->arrival_tick;
        case TURNAROUND: return r->finished_tick - r->arrival_tick;
        default: return 0;
    }
}

/* Nearest-rank percentile (ceil(p*n/100)), using bounded integer selection.
 * No allocation, platform-specific sort comparator or floating-point tie drift. */
static uint64_t percentile(const OsRequest *requests, size_t count,
                           size_t completed_count, unsigned p, Metric metric) {
    if (completed_count == 0) return 0;
    const size_t rank = (completed_count / 100) * p +
                        ((completed_count % 100) * p + 99) / 100;
    uint64_t high = 0;
    for (size_t i = 0; i < count; ++i)
        if (completed(&requests[i]) && value(&requests[i], metric) > high)
            high = value(&requests[i], metric);
    uint64_t low = 0;
    while (low < high) {
        const uint64_t mid = low + (high - low) / 2;
        size_t below = 0;
        for (size_t i = 0; i < count; ++i)
            if (completed(&requests[i]) && value(&requests[i], metric) <= mid)
                ++below;
        if (below >= rank) high = mid;
        else low = mid + 1;
    }
    return low;
}

OsRunSummary os_metrics_summarize(const OsRequest *requests, size_t count,
                                  uint64_t makespan_ticks) {
    OsRunSummary result = {.makespan_ticks = makespan_ticks};
    if (requests == NULL) return result;
    for (size_t i = 0; i < count; ++i) {
        const OsRequest *r = &requests[i];
        if (!completed(r)) continue;
        ++result.completed;
        result.average_wait_ticks += (double)r->waiting_ticks;
        result.average_response_ticks += (double)(r->first_run_tick - r->arrival_tick);
        result.average_turnaround_ticks += (double)(r->finished_tick - r->arrival_tick);
        if (r->waiting_ticks > result.max_wait_ticks)
            result.max_wait_ticks = r->waiting_ticks;
    }
    if (result.completed != 0) {
        result.average_wait_ticks /= (double)result.completed;
        result.average_response_ticks /= (double)result.completed;
        result.average_turnaround_ticks /= (double)result.completed;
        result.p50_wait_ticks = (double)percentile(requests, count, result.completed, 50, WAIT);
        result.p95_wait_ticks = (double)percentile(requests, count, result.completed, 95, WAIT);
        result.p50_response_ticks = (double)percentile(requests, count, result.completed, 50, RESPONSE);
        result.p95_response_ticks = (double)percentile(requests, count, result.completed, 95, RESPONSE);
        result.p50_turnaround_ticks = (double)percentile(requests, count, result.completed, 50, TURNAROUND);
        result.p95_turnaround_ticks = (double)percentile(requests, count, result.completed, 95, TURNAROUND);
    }
    if (makespan_ticks != 0)
        result.throughput_per_tick = (double)result.completed / (double)makespan_ticks;
    return result;
}
