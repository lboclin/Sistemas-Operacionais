#include "scheduler.h"

#include <stdint.h>
#include <stdlib.h>

static uint64_t ready_wait(const OsRequest *request, uint64_t now) {
    return now >= request->ready_tick ? now - request->ready_tick : 0;
}

/* 96-bit product of a 64-bit wait and a 32-bit size, split so that no
 * intermediate exceeds uint64_t (C11 has no portable 128-bit integer). */
typedef struct {
    uint64_t high;
    uint32_t low;
} Product96;

static Product96 multiply(uint64_t wait, uint32_t size) {
    const uint64_t low = (wait & UINT32_MAX) * size;
    return (Product96){.high = (wait >> 32) * size + (low >> 32),
                       .low = (uint32_t)(low & UINT32_MAX)};
}

/* Response ratio is (wait + size) / size = 1 + wait / size, so comparing the
 * ratios reduces to wait_a * size_b against wait_b * size_a. */
static int compare_response_ratio(uint64_t wait_a, uint32_t size_a,
                                  uint64_t wait_b, uint32_t size_b) {
    const Product96 a = multiply(wait_a, size_b);
    const Product96 b = multiply(wait_b, size_a);
    if (a.high != b.high) return a.high > b.high ? 1 : -1;
    if (a.low != b.low) return a.low > b.low ? 1 : -1;
    return 0;
}

static int compare(const OsRequest *candidate, const OsRequest *best,
                   OsSchedulingPolicy policy, uint64_t now, uint64_t aging_interval) {
    /* Jev returns a fixed-size decision, so the job length is the input size,
     * known at arrival: SJF needs no prediction. */
    if (policy == OS_SJF && candidate->total_tokens != best->total_tokens)
        return candidate->total_tokens < best->total_tokens ? -1 : 1;
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
    if (policy == OS_HRRN) {
        const int ratio = compare_response_ratio(
            ready_wait(candidate, now), candidate->total_tokens,
            ready_wait(best, now), best->total_tokens);
        if (ratio != 0) return ratio > 0 ? -1 : 1;
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
    if (requests == NULL || policy < OS_FCFS || policy > OS_HRRN) return SIZE_MAX;
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
        case OS_SJF: return "SJF";
        case OS_ROUND_ROBIN: return "Round-Robin";
        case OS_PRIORITY_AGING: return "Priority-aging";
        case OS_HRRN: return "HRRN";
        default: return "unknown";
    }
}
