#include "virtual.h"
#include "gateway.h"
#include "frames.h"

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
        (config->memory_enabled && (config->frame_capacity == 0 ||
         config->tokens_per_page == 0 ||
         (config->page_policy != OS_PAGE_FIFO && config->page_policy != OS_PAGE_LRU))) ||
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
    OsFrameCache cache = {0};
    if (config->memory_enabled && !os_frames_init(&cache, config->frame_capacity, config->page_policy)) {
        free(workers);
        return OS_RUN_ALLOC_FAILURE;
    }
    uint64_t now = 0, order = 0;
    size_t finished = 0;
    while (finished != count) {
        if (!admit_arrivals(requests, count, now, &order, observer, observer_context)) { os_frames_destroy(&cache); free(workers); return OS_RUN_OVERFLOW; }
        for (size_t w = 0; w < config->worker_count; ++w) {
            if (workers[w].request_index != SIZE_MAX) continue;
            const size_t chosen = os_scheduler_select(requests, count, config->policy, now, config->aging_interval);
            if (chosen == SIZE_MAX) continue;
            OsRequest *r = &requests[chosen];
            if (!os_request_transition(r, OS_RUNNING)) { os_frames_destroy(&cache); free(workers); return OS_RUN_INVALID; }
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
            if (next_arrival == UINT64_MAX) { os_frames_destroy(&cache); free(workers); return OS_RUN_INVALID; }
            now = next_arrival;
            continue;
        }
        if (now == UINT64_MAX) { os_frames_destroy(&cache); free(workers); return OS_RUN_OVERFLOW; }
        /* Each token touches one logical KV page before advancing the clock.
         * Page faults are instantaneous in this first memory model. */
        if (config->memory_enabled) {
            for (size_t w = 0; w < config->worker_count; ++w) {
                if (workers[w].request_index == SIZE_MAX) continue;
                const OsRequest *r = &requests[workers[w].request_index];
                const OsPageReference page = {.request_id = r->id,
                    .page_id = r->generated_tokens / config->tokens_per_page};
                OsFrameAccess access;
                if (!os_frames_access(&cache, page, &access)) {
                    os_frames_destroy(&cache); free(workers); return OS_RUN_OVERFLOW;
                }
                if (access.evicted)
                    emit(observer, observer_context, OS_EVENT_PAGE_EVICTED, now,
                         access.evicted_page.request_id, w, access.evicted_page.page_id);
                emit(observer, observer_context,
                     access.hit ? OS_EVENT_PAGE_HIT : OS_EVENT_PAGE_FAULT,
                     now, r->id, w, page.page_id);
            }
        }
        ++now;
        /* Admit arrivals before requeuing expiring RR quanta at this tick. */
        if (!admit_arrivals(requests, count, now, &order, observer, observer_context)) { os_frames_destroy(&cache); free(workers); return OS_RUN_OVERFLOW; }
        for (size_t w = 0; w < config->worker_count; ++w) {
            if (workers[w].request_index == SIZE_MAX) continue;
            OsVirtualWorker *worker = &workers[w];
            OsRequest *r = &requests[worker->request_index];
            ++r->generated_tokens;
            ++worker->quantum_used;
            if (r->generated_tokens == r->total_tokens) {
                if (!os_request_transition(r, OS_TERMINATED)) { os_frames_destroy(&cache); free(workers); return OS_RUN_INVALID; }
                r->finished_tick = now;
                if (config->memory_enabled) (void)os_frames_release(&cache, r->id);
                worker->request_index = SIZE_MAX;
                ++finished;
                emit(observer, observer_context, OS_EVENT_COMPLETED, now, r->id, w, 0);
            } else if (config->policy == OS_ROUND_ROBIN &&
                       worker->quantum_used == config->quantum_tokens) {
                if (order == UINT64_MAX || !os_request_transition(r, OS_READY)) {
                    os_frames_destroy(&cache); free(workers); return OS_RUN_OVERFLOW;
                }
                r->ready_tick = now;
                r->ready_order = order++;
                worker->request_index = SIZE_MAX;
                emit(observer, observer_context, OS_EVENT_PREEMPTED, now, r->id, w, 0);
            }
        }
    }
    *summary = os_metrics_summarize(requests, count, now);
    if (config->memory_enabled) {
        summary->page_hits = cache.stats.hits;
        summary->page_faults = cache.stats.faults;
        summary->page_evictions = cache.stats.evictions;
        summary->peak_resident_frames = cache.peak_used;
    }
    os_frames_destroy(&cache);
    free(workers);
    return OS_RUN_OK;
}

OsRunStatus os_virtual_run(OsRequest *requests, size_t count,
                           const OsVirtualConfig *config, OsRunSummary *summary) {
    return os_virtual_run_observed(requests, count, config, summary, NULL, NULL);
}
