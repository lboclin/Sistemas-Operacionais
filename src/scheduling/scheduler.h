#ifndef OS_SCHEDULER_H
#define OS_SCHEDULER_H

#include "request.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OS_FCFS,
    OS_SJF_ESTIMATED,
    OS_SJF_ORACLE,
    OS_ROUND_ROBIN,
    OS_PRIORITY_AGING
} OsSchedulingPolicy;

/* Returns SIZE_MAX if no request is ready. Policies are non-preemptive except RR. */
size_t os_scheduler_select(const OsRequest *requests, size_t count,
                           OsSchedulingPolicy policy, uint64_t now,
                           uint64_t aging_interval);
const char *os_policy_name(OsSchedulingPolicy policy);

#endif
