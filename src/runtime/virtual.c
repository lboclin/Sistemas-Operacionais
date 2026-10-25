#include "virtual.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    size_t request_index;
    uint32_t quantum_used;
} OsVirtualWorker;

static bool admit_arrivals(OsRequest *requests, size_t count,
                           uint64_t now, uint64_t *order) {
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].state != OS_NEW || requests[i].arrival_tick > now) continue;
        if (*order == UINT64_MAX || !os_request_transition(&requests[i], OS_READY)) return false;
        requests[i].ready_tick = requests[i].arrival_tick;
        requests[i].ready_order = (*order)++;
    }
    return true;
}

OsRunStatus os_virtual_run(OsRequest *requests, size_t count,
                           const OsVirtualConfig *config, OsRunSummary *summary) {
    if (requests == NULL || config == NULL || summary == NULL || count == 0 ||
        config->worker_count == 0 || config->worker_count > 4096 ||
        config->policy < OS_FCFS || config->policy > OS_PRIORITY_AGING ||
        (config->policy == OS_ROUND_ROBIN && config->quantum_tokens == 0)) return OS_RUN_INVALID;
    uint64_t total_tokens = 0;
    uint64_t max_arrival = 0;
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].state != OS_NEW || requests[i].total_tokens == 0 ||
            requests[i].predicted_tokens == 0 || requests[i].arrival_tick == UINT64_MAX ||
            requests[i].generated_tokens != 0) return OS_RUN_INVALID;
        for (size_t j = 0; j < i; ++j) if (requests[i].id == requests[j].id) return OS_RUN_INVALID;
        if (UINT64_MAX - total_tokens < requests[i].total_tokens) return OS_RUN_OVERFLOW;
        total_tokens += requests[i].total_tokens;
        if (requests[i].arrival_tick > max_arrival) max_arrival = requests[i].arrival_tick;
    }
    if (UINT64_MAX - total_tokens < max_arrival) return OS_RUN_OVERFLOW;
    OsVirtualWorker *workers = calloc(config->worker_count, sizeof(*workers));
    if (workers == NULL) return OS_RUN_ALLOC_FAILURE;
    for (size_t w = 0; w < config->worker_count; ++w) workers[w].request_index = SIZE_MAX;
    uint64_t now = 0, order = 0;
    size_t finished = 0;
    while (finished != count) {
        if (!admit_arrivals(requests, count, now, &order)) { free(workers); return OS_RUN_OVERFLOW; }
        for (size_t w = 0; w < config->worker_count; ++w) {
            if (workers[w].request_index != SIZE_MAX) continue;
            const size_t chosen = os_scheduler_select(requests, count, config->policy, now, config->aging_interval);
            if (chosen == SIZE_MAX) continue;
            OsRequest *r = &requests[chosen];
            if (!os_request_transition(r, OS_RUNNING)) { free(workers); return OS_RUN_INVALID; }
            r->waiting_ticks += now - r->ready_tick;
            if (r->first_run_tick == OS_REQUEST_NEVER) r->first_run_tick = now;
            workers[w].request_index = chosen;
            workers[w].quantum_used = 0;
        }
        bool busy = false;
        for (size_t w = 0; w < config->worker_count; ++w)
            if (workers[w].request_index != SIZE_MAX) busy = true;
        if (!busy) {
            uint64_t next_arrival = UINT64_MAX;
            for (size_t i = 0; i < count; ++i)
                if (requests[i].state == OS_NEW && requests[i].arrival_tick < next_arrival)
                    next_arrival = requests[i].arrival_tick;
            if (next_arrival == UINT64_MAX) { free(workers); return OS_RUN_INVALID; }
            now = next_arrival;
            continue;
        }
        if (now == UINT64_MAX) { free(workers); return OS_RUN_OVERFLOW; }
        ++now;
        /* Admit arrivals before requeuing expiring RR quanta at this tick. */
        if (!admit_arrivals(requests, count, now, &order)) { free(workers); return OS_RUN_OVERFLOW; }
        for (size_t w = 0; w < config->worker_count; ++w) {
            if (workers[w].request_index == SIZE_MAX) continue;
            OsVirtualWorker *worker = &workers[w];
            OsRequest *r = &requests[worker->request_index];
            ++r->generated_tokens;
            ++worker->quantum_used;
            if (r->generated_tokens == r->total_tokens) {
                if (!os_request_transition(r, OS_TERMINATED)) { free(workers); return OS_RUN_INVALID; }
                r->finished_tick = now;
                worker->request_index = SIZE_MAX;
                ++finished;
            } else if (config->policy == OS_ROUND_ROBIN &&
                       worker->quantum_used == config->quantum_tokens) {
                if (order == UINT64_MAX || !os_request_transition(r, OS_READY)) {
                    free(workers); return OS_RUN_OVERFLOW;
                }
                r->ready_tick = now;
                r->ready_order = order++;
                worker->request_index = SIZE_MAX;
            }
        }
    }
    *summary = os_metrics_summarize(requests, count, now);
    free(workers);
    return OS_RUN_OK;
}
