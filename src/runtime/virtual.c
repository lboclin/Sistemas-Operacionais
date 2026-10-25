#include "virtual.h"
#include "gateway.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    size_t request_index;
    uint32_t quantum_used;
} OsVirtualWorker;

static void emit(OsEventCallback callback, void *context, OsEventType type,
                 uint64_t tick, uint32_t request_id, size_t worker, uint32_t page) {
    if (callback != NULL) {
        const OsEvent event = {.type = type, .tick = tick, .request_id = request_id,
                               .worker_index = worker, .page_id = page};
        callback(&event, context);
    }
}

static bool admit_arrivals(OsRequest *requests, size_t count, uint64_t now,
                           uint64_t *order, OsEventCallback callback, void *context) {
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].state != OS_NEW || requests[i].arrival_tick > now) continue;
        if (*order == UINT64_MAX || !os_gateway_admit(&requests[i], now, *order)) return false;
        ++(*order);
        emit(callback, context, OS_EVENT_ARRIVED, requests[i].arrival_tick,
             requests[i].id, SIZE_MAX, 0);
    }
    return true;
}

OsRunStatus os_virtual_run_observed(OsRequest *requests, size_t count,
                                    const OsVirtualConfig *config, OsRunSummary *summary,
                                    OsEventCallback observer, void *observer_context) {
    if (requests == NULL || config == NULL || summary == NULL || count == 0 ||
        config->worker_count == 0 || config->worker_count > 4096 ||
        config->policy < OS_FCFS || config->policy > OS_PRIORITY_AGING ||
        (config->policy == OS_ROUND_ROBIN && config->quantum_tokens == 0)) return OS_RUN_INVALID;
    uint64_t total_tokens = 0;
    uint64_t max_arrival = 0;
    if (!os_gateway_validate(requests, count)) return OS_RUN_INVALID;
    for (size_t i = 0; i < count; ++i) {
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
        if (!admit_arrivals(requests, count, now, &order, observer, observer_context)) { free(workers); return OS_RUN_OVERFLOW; }
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
            emit(observer, observer_context, OS_EVENT_DISPATCHED, now, r->id, w, 0);
        }
        bool busy = false;
        for (size_t w = 0; w < config->worker_count; ++w)
            if (workers[w].request_index != SIZE_MAX) busy = true;
        if (!busy) {
            const uint64_t next_arrival = os_gateway_next_arrival(requests, count);
            if (next_arrival == UINT64_MAX) { free(workers); return OS_RUN_INVALID; }
            now = next_arrival;
            continue;
        }
        if (now == UINT64_MAX) { free(workers); return OS_RUN_OVERFLOW; }
        ++now;
        /* Admit arrivals before requeuing expiring RR quanta at this tick. */
        if (!admit_arrivals(requests, count, now, &order, observer, observer_context)) { free(workers); return OS_RUN_OVERFLOW; }
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
                emit(observer, observer_context, OS_EVENT_COMPLETED, now, r->id, w, 0);
            } else if (config->policy == OS_ROUND_ROBIN &&
                       worker->quantum_used == config->quantum_tokens) {
                if (order == UINT64_MAX || !os_request_transition(r, OS_READY)) {
                    free(workers); return OS_RUN_OVERFLOW;
                }
                r->ready_tick = now;
                r->ready_order = order++;
                worker->request_index = SIZE_MAX;
                emit(observer, observer_context, OS_EVENT_PREEMPTED, now, r->id, w, 0);
            }
        }
    }
    *summary = os_metrics_summarize(requests, count, now);
    free(workers);
    return OS_RUN_OK;
}

OsRunStatus os_virtual_run(OsRequest *requests, size_t count,
                           const OsVirtualConfig *config, OsRunSummary *summary) {
    return os_virtual_run_observed(requests, count, config, summary, NULL, NULL);
}
