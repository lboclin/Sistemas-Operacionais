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
} OsRunSummary;

OsRunSummary os_metrics_summarize(const OsRequest *requests, size_t count,
                                  uint64_t makespan_ticks);

#endif
