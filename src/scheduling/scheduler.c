#include "scheduler.h"

#include <stdint.h>
#include <stdlib.h>

static uint64_t ready_wait(const OsRequest *request, uint64_t now) {
    return now >= request->ready_tick ? now - request->ready_tick : 0;
}

static int compare(const OsRequest *candidate, const OsRequest *best,
                   OsSchedulingPolicy policy, uint64_t now, uint64_t aging_interval) {
    if (policy == OS_SJF_ESTIMATED || policy == OS_SJF_ORACLE) {
        const uint32_t ac = policy == OS_SJF_ORACLE ? candidate->total_tokens : candidate->predicted_tokens;
        const uint32_t ab = policy == OS_SJF_ORACLE ? best->total_tokens : best->predicted_tokens;
        if (ac != ab) return ac < ab ? -1 : 1;
    }
    if (policy == OS_PRIORITY_AGING) {
        const uint64_t wc = aging_interval ? ready_wait(candidate, now) / aging_interval : 0;
        const uint64_t wb = aging_interval ? ready_wait(best, now) / aging_interval : 0;
        /* Avoid signed overflow: cap bonuses far beyond realistic simulation length. */
        const uint64_t max_bonus = (uint64_t)(INT64_MAX - INT32_MAX);
        const uint64_t bonus_c = wc > max_bonus ? max_bonus : wc;
        const uint64_t bonus_b = wb > max_bonus ? max_bonus : wb;
        const int64_t pc = (int64_t)candidate->priority + (int64_t)bonus_c;
        const int64_t pb = (int64_t)best->priority + (int64_t)bonus_b;
        if (pc != pb) return pc > pb ? -1 : 1;
    }
    if (policy == OS_FCFS) {
        if (candidate->arrival_tick != best->arrival_tick)
            return candidate->arrival_tick < best->arrival_tick ? -1 : 1;
    } else if (candidate->ready_order != best->ready_order) {
        return candidate->ready_order < best->ready_order ? -1 : 1;
    }
    return candidate->id < best->id ? -1 : candidate->id > best->id ? 1 : 0;
}

size_t os_scheduler_select(const OsRequest *requests, size_t count,
                           OsSchedulingPolicy policy, uint64_t now,
                           uint64_t aging_interval) {
    if (requests == NULL || policy < OS_FCFS || policy > OS_PRIORITY_AGING) return SIZE_MAX;
    size_t best = SIZE_MAX;
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].state != OS_READY) continue;
        if (best == SIZE_MAX || compare(&requests[i], &requests[best], policy, now, aging_interval) < 0)
            best = i;
    }
    return best;
}

const char *os_policy_name(OsSchedulingPolicy policy) {
    switch (policy) {
        case OS_FCFS: return "FCFS";
        case OS_SJF_ESTIMATED: return "SJF-estimated";
        case OS_SJF_ORACLE: return "SJF-oracle";
        case OS_ROUND_ROBIN: return "Round-Robin";
        case OS_PRIORITY_AGING: return "Priority-aging";
        case OS_SJF_EMA: return "SJF-EMA";
        default: return "unknown";
    }
}

/* A priority is a provisional workload class, not an execution preference
 * in EMA mode. Register classes without reading their output token counts. */
bool os_ema_init(OsEmaEstimator *ema, const OsRequest *requests, size_t count,
                 double alpha, uint32_t initial_tokens) {
    if (ema == NULL || ema->classes != NULL || ema->class_count != 0 ||
        requests == NULL || count == 0 || count > SIZE_MAX / sizeof(OsEmaClass) ||
        !(alpha >= 0.0 && alpha <= 1.0) || initial_tokens == 0) return false;
    OsEmaClass *classes = calloc(count, sizeof(*classes));
    if (classes == NULL) return false;
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t j = 0;
        for (; j < used; ++j)
            if (classes[j].priority == requests[i].priority) break;
        if (j == used) {
            classes[used].priority = requests[i].priority;
            classes[used].tau = (double)initial_tokens;
            ++used;
        }
    }
    ema->classes = classes;
    ema->class_count = used;
    ema->alpha = alpha;
    return true;
}

void os_ema_destroy(OsEmaEstimator *ema) {
    if (ema == NULL) return;
    free(ema->classes);
    *ema = (OsEmaEstimator){0};
}

double os_ema_predict(const OsEmaEstimator *ema, int priority) {
    if (ema == NULL || ema->classes == NULL) return 0.0;
    for (size_t i = 0; i < ema->class_count; ++i)
        if (ema->classes[i].priority == priority) return ema->classes[i].tau;
    return 0.0;
}

bool os_ema_observe(OsEmaEstimator *ema, const OsRequest *completed) {
    if (ema == NULL || ema->classes == NULL || completed == NULL ||
        completed->state != OS_TERMINATED || completed->generated_tokens == 0) return false;
    for (size_t i = 0; i < ema->class_count; ++i) {
        if (ema->classes[i].priority != completed->priority) continue;
        const double previous = ema->classes[i].tau;
        ema->classes[i].tau = ema->alpha * (double)completed->generated_tokens +
                              (1.0 - ema->alpha) * previous;
        return true;
    }
    return false;
}

/* Never consult total_tokens or predicted_tokens for READY requests.
 * Equal EMA estimates use the same stable ready_order / ID tie-break. */
size_t os_scheduler_select_ema(const OsRequest *requests, size_t count,
                               const OsEmaEstimator *ema) {
    if (requests == NULL || ema == NULL || ema->classes == NULL) return SIZE_MAX;
    size_t best = SIZE_MAX;
    double best_tau = 0.0;
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].state != OS_READY) continue;
        const double tau = os_ema_predict(ema, requests[i].priority);
        if (tau <= 0.0) return SIZE_MAX;
        if (best == SIZE_MAX || tau < best_tau ||
            (tau == best_tau &&
             (requests[i].ready_order < requests[best].ready_order ||
              (requests[i].ready_order == requests[best].ready_order &&
               requests[i].id < requests[best].id)))) {
            best = i;
            best_tau = tau;
        }
    }
    return best;
}
