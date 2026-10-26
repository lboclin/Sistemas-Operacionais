#ifndef OS_SUMMARY_H
#define OS_SUMMARY_H

#include "request.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t completed;
    uint64_t makespan_ticks;
    double average_wait_ticks;
    double average_response_ticks;
    double average_turnaround_ticks;
    double throughput_per_tick;
    /* Zero when memory simulation is disabled. */
    uint64_t page_hits;
    uint64_t page_faults;
    uint64_t page_evictions;
    size_t peak_resident_frames;
    double p50_wait_ticks;
    double p95_wait_ticks;
    double p50_response_ticks;
    double p95_response_ticks;
    double p50_turnaround_ticks;
    double p95_turnaround_ticks;
    uint64_t max_wait_ticks;
    double worker_utilization; /* tokens / (makespan * worker_count) */
    uint64_t total_blocked_ticks; /* sum across requests, including active waits */
    uint64_t preemptions;
    size_t starvation_count; /* completed requests exceeding configured threshold */
} OsRunSummary;

OsRunSummary os_metrics_summarize(const OsRequest *requests, size_t count,
                                  uint64_t makespan_ticks);

#endif
