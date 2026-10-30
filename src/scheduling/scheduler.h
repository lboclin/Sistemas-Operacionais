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
    OS_PRIORITY_AGING,
    OS_SJF_EMA
} OsSchedulingPolicy;

/* EMA history is partitioned by priority, a provisional workload class.
 * Estimates start equal and change only when a request TERMINATES. */
typedef struct {
    int priority;
    double tau;
} OsEmaClass;
typedef struct {
    OsEmaClass *classes;
    size_t class_count;
    double alpha;
} OsEmaEstimator;

bool os_ema_init(OsEmaEstimator *ema, const OsRequest *requests, size_t count,
                 double alpha, uint32_t initial_tokens);
void os_ema_destroy(OsEmaEstimator *ema);
bool os_ema_observe(OsEmaEstimator *ema, const OsRequest *completed);
double os_ema_predict(const OsEmaEstimator *ema, int priority); /* 0 if unknown */

/* Returns SIZE_MAX if no request is ready. Policies are non-preemptive except RR.
 * OS_SJF_EMA requires the dedicated selector and its live history. */
size_t os_scheduler_select(const OsRequest *requests, size_t count,
                           OsSchedulingPolicy policy, uint64_t now,
                           uint64_t aging_interval);
size_t os_scheduler_select_ema(const OsRequest *requests, size_t count,
                               const OsEmaEstimator *ema);
const char *os_policy_name(OsSchedulingPolicy policy);

#endif
