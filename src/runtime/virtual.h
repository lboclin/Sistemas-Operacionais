#ifndef OS_VIRTUAL_H
#define OS_VIRTUAL_H

#include "request.h"
#include "scheduler.h"
#include "summary.h"
#include "events.h"
#include "pager.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    OsSchedulingPolicy policy;
    size_t worker_count;
    uint32_t quantum_tokens;
    uint64_t aging_interval;
    /* Optional online KV-page model. Zero-initialized memory_enabled=false
     * preserves the existing CPU-only model. */
    bool memory_enabled;
    size_t frame_capacity;
    uint32_t tokens_per_page;
    OsPagePolicy page_policy; /* FIFO/LRU only for online execution */
    uint64_t page_fault_latency_ticks; /* 0 preserves legacy zero-cost faults */
    uint64_t starvation_threshold_ticks; /* 0 disables threshold detection */
} OsVirtualConfig;

typedef enum {
    OS_RUN_OK,
    OS_RUN_INVALID,
    OS_RUN_OVERFLOW,
    OS_RUN_ALLOC_FAILURE,
    OS_RUN_EXEC_FAILURE
} OsRunStatus;

/* Requests are mutated in place; every valid request must start in OS_NEW.
 * One virtual tick produces one token per occupied worker. */
OsRunStatus os_virtual_run(OsRequest *requests, size_t count,
                           const OsVirtualConfig *config, OsRunSummary *summary);

/* Same execution with a synchronous, read-only event observer. No snapshot/step yet. */
OsRunStatus os_virtual_run_observed(OsRequest *requests, size_t count,
                                    const OsVirtualConfig *config, OsRunSummary *summary,
                                    OsEventCallback observer, void *observer_context);

#endif
