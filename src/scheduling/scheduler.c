#include "scheduler.h"

#include <stdint.h>

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
        default: return "unknown";
    }
}
