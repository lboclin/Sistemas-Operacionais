#ifndef OS_VIRTUAL_H
#define OS_VIRTUAL_H

#include "request.h"
#include "scheduler.h"
#include "summary.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    OsSchedulingPolicy policy;
    size_t worker_count;
    uint32_t quantum_tokens;
    uint64_t aging_interval;
} OsVirtualConfig;

typedef enum {
    OS_RUN_OK,
    OS_RUN_INVALID,
    OS_RUN_OVERFLOW,
    OS_RUN_ALLOC_FAILURE
} OsRunStatus;

/* Requests are mutated in place; every valid request must start in OS_NEW.
 * One virtual tick produces one token per occupied worker. */
OsRunStatus os_virtual_run(OsRequest *requests, size_t count,
                           const OsVirtualConfig *config, OsRunSummary *summary);

#endif
