#include "session.h"
#include "gateway.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t request_index;
    uint32_t quantum_used;
} OsWorker;

struct OsVirtualSession {
    OsRequest *requests;
    OsWorker *workers;
    size_t count;
    OsVirtualConfig config;
    OsFrameCache cache;
    uint64_t tick;
    uint64_t ready_order;
    size_t completed;
    OsEventCallback observer;
    void *context;
};

static void emit(OsVirtualSession *s, OsEventType type, uint64_t at,
                 uint32_t request_id, size_t worker, uint32_t page_id) {
    if (s->observer == NULL) return;
    const OsEvent event = {.type = type, .tick = at, .request_id = request_id,
                            .worker_index = worker, .page_id = page_id};
    s->observer(&event, s->context);
}

static bool admit(OsVirtualSession *s) {
    for (size_t i = 0; i < s->count; ++i) {
        OsRequest *r = &s->requests[i];
        if (r->state != OS_NEW || r->arrival_tick > s->tick) continue;
        if (s->ready_order == UINT64_MAX ||
            !os_gateway_admit(r, s->tick, s->ready_order)) return false;
        ++s->ready_order;
        emit(s, OS_EVENT_ARRIVED, r->arrival_tick, r->id, SIZE_MAX, 0);
    }
    return true;
}

static bool dispatch(OsVirtualSession *s) {
    bool busy = false;
    for (size_t i = 0; i < s->config.worker_count; ++i) {
        OsWorker *w = &s->workers[i];
        if (w->request_index != SIZE_MAX) { busy = true; continue; }
        const size_t selected = os_scheduler_select(s->requests, s->count,
            s->config.policy, s->tick, s->config.aging_interval);
        if (selected == SIZE_MAX) continue;
        OsRequest *r = &s->requests[selected];
        if (!os_request_transition(r, OS_RUNNING) || s->tick < r->ready_tick ||
            UINT64_MAX - r->waiting_ticks < s->tick - r->ready_tick) return false;
        r->waiting_ticks += s->tick - r->ready_tick;
        if (r->first_run_tick == OS_REQUEST_NEVER) r->first_run_tick = s->tick;
        w->request_index = selected;
        w->quantum_used = 0;
        busy = true;
        emit(s, OS_EVENT_DISPATCHED, s->tick, r->id, i, 0);
    }
    return busy;
}

static bool access_pages(OsVirtualSession *s) {
    if (!s->config.memory_enabled) return true;
    for (size_t i = 0; i < s->config.worker_count; ++i) {
        const size_t index = s->workers[i].request_index;
        if (index == SIZE_MAX) continue;
        const OsRequest *r = &s->requests[index];
        const OsPageReference page = {.request_id = r->id,
                         .page_id = r->generated_tokens / s->config.tokens_per_page};
        OsFrameAccess access;
        if (!os_frames_access(&s->cache, page, &access)) return false;
        if (access.evicted)
            emit(s, OS_EVENT_PAGE_EVICTED, s->tick, access.evicted_page.request_id,
                 i, access.evicted_page.page_id);
        emit(s, access.hit ? OS_EVENT_PAGE_HIT : OS_EVENT_PAGE_FAULT,
             s->tick, r->id, i, page.page_id);
    }
    return true;
}

OsRunStatus os_session_create(const OsRequest *requests, size_t count,
                              const OsVirtualConfig *config,
                              OsEventCallback observer, void *context,
                              OsVirtualSession **out) {
    if (out == NULL) return OS_RUN_INVALID;
    *out = NULL;
    if (config == NULL || !os_gateway_validate(requests, count) ||
        config->worker_count == 0 || config->worker_count > 4096 ||
        config->policy < OS_FCFS || config->policy > OS_PRIORITY_AGING ||
        (config->policy == OS_ROUND_ROBIN && config->quantum_tokens == 0) ||
        (config->memory_enabled && (config->frame_capacity == 0 ||
         config->tokens_per_page == 0 ||
         (config->page_policy != OS_PAGE_FIFO && config->page_policy != OS_PAGE_LRU))))
        return OS_RUN_INVALID;
    if (count > SIZE_MAX / sizeof(OsRequest) ||
        config->worker_count > SIZE_MAX / sizeof(OsWorker)) return OS_RUN_ALLOC_FAILURE;
    uint64_t total = 0;
    uint64_t max_arrival = 0;
    for (size_t i = 0; i < count; ++i) {
        if (UINT64_MAX - total < requests[i].total_tokens) return OS_RUN_OVERFLOW;
        total += requests[i].total_tokens;
        if (max_arrival < requests[i].arrival_tick) max_arrival = requests[i].arrival_tick;
    }
    if (UINT64_MAX - total < max_arrival) return OS_RUN_OVERFLOW;
    OsVirtualSession *s = calloc(1, sizeof(*s));
    if (s == NULL) return OS_RUN_ALLOC_FAILURE;
    s->requests = malloc(count * sizeof(*s->requests));
    s->workers = malloc(config->worker_count * sizeof(*s->workers));
    if (s->requests == NULL || s->workers == NULL) {
        os_session_destroy(s);
        return OS_RUN_ALLOC_FAILURE;
    }
    memcpy(s->requests, requests, count * sizeof(*requests));
    for (size_t i = 0; i < config->worker_count; ++i)
        s->workers[i] = (OsWorker){.request_index = SIZE_MAX};
    s->count = count;
    s->config = *config;
    s->observer = observer;
    s->context = context;
    if (config->memory_enabled &&
        !os_frames_init(&s->cache, config->frame_capacity, config->page_policy)) {
        os_session_destroy(s);
        return OS_RUN_ALLOC_FAILURE;
    }
    *out = s;
    return OS_RUN_OK;
}

OsRunStatus os_session_step(OsVirtualSession *s) {
    if (s == NULL) return OS_RUN_INVALID;
    if (s->completed == s->count) return OS_RUN_OK;
    if (!admit(s)) return OS_RUN_OVERFLOW;
    bool busy = dispatch(s);
    if (!busy) {
        const uint64_t next = os_gateway_next_arrival(s->requests, s->count);
        if (next == OS_REQUEST_NEVER || next <= s->tick) return OS_RUN_INVALID;
        s->tick = next;
        if (!admit(s)) return OS_RUN_OVERFLOW;
        busy = dispatch(s);
        if (!busy) return OS_RUN_INVALID;
    }
    if (s->tick == UINT64_MAX) return OS_RUN_OVERFLOW;
    if (!access_pages(s)) return OS_RUN_OVERFLOW;
    ++s->tick;
    /* Preserve legacy ordering: arrivals at tick T precede expiring quanta at T. */
    if (!admit(s)) return OS_RUN_OVERFLOW;
    for (size_t i = 0; i < s->config.worker_count; ++i) {
        OsWorker *w = &s->workers[i];
        if (w->request_index == SIZE_MAX) continue;
        OsRequest *r = &s->requests[w->request_index];
        ++r->generated_tokens;
        ++w->quantum_used;
        if (r->generated_tokens == r->total_tokens) {
            if (!os_request_transition(r, OS_TERMINATED)) return OS_RUN_INVALID;
            r->finished_tick = s->tick;
            if (s->config.memory_enabled) (void)os_frames_release(&s->cache, r->id);
            w->request_index = SIZE_MAX;
            ++s->completed;
            emit(s, OS_EVENT_COMPLETED, s->tick, r->id, i, 0);
        } else if (s->config.policy == OS_ROUND_ROBIN &&
                   w->quantum_used == s->config.quantum_tokens) {
            if (s->ready_order == UINT64_MAX ||
                !os_request_transition(r, OS_READY)) return OS_RUN_OVERFLOW;
            r->ready_tick = s->tick;
            r->ready_order = s->ready_order++;
            w->request_index = SIZE_MAX;
            emit(s, OS_EVENT_PREEMPTED, s->tick, r->id, i, 0);
        }
    }
    return OS_RUN_OK;
}

OsRunStatus os_session_run(OsVirtualSession *s) {
    if (s == NULL) return OS_RUN_INVALID;
    while (s->completed != s->count) {
        const OsRunStatus status = os_session_step(s);
        if (status != OS_RUN_OK) return status;
    }
    return OS_RUN_OK;
}

bool os_session_snapshot(const OsVirtualSession *s,
                         OsSessionSnapshot *snapshot,
                         OsRequest *requests, size_t request_capacity,
                         OsWorkerSnapshot *workers, size_t worker_capacity) {
    if (s == NULL || snapshot == NULL ||
        (requests == NULL && request_capacity != 0) ||
        (workers == NULL && worker_capacity != 0) ||
        (requests != NULL && request_capacity < s->count) ||
        (workers != NULL && worker_capacity < s->config.worker_count)) return false;
    OsSessionSnapshot result = {.tick = s->tick, .request_count = s->count,
        .worker_count = s->config.worker_count,
        .finished = s->completed == s->count};
    for (size_t i = 0; i < s->count; ++i) {
        switch (s->requests[i].state) {
            case OS_NEW: ++result.pending; break;
            case OS_READY: ++result.ready; break;
            case OS_RUNNING: ++result.running; break;
            case OS_BLOCKED: ++result.blocked; break;
            case OS_TERMINATED: ++result.completed; break;
            default: return false;
        }
    }
    result.metrics = os_metrics_summarize(s->requests, s->count, s->tick);
    result.metrics.page_hits = s->cache.stats.hits;
    result.metrics.page_faults = s->cache.stats.faults;
    result.metrics.page_evictions = s->cache.stats.evictions;
    result.metrics.peak_resident_frames = s->cache.peak_used;
    result.resident_frames = s->cache.used;
    if (requests != NULL) memcpy(requests, s->requests, s->count * sizeof(*requests));
    if (workers != NULL) {
        for (size_t i = 0; i < s->config.worker_count; ++i) {
            const OsWorker *w = &s->workers[i];
            workers[i] = (OsWorkerSnapshot){
                .occupied = w->request_index != SIZE_MAX,
                .request_id = w->request_index == SIZE_MAX ? 0 : s->requests[w->request_index].id,
                .quantum_used = w->quantum_used
            };
        }
    }
    *snapshot = result;
    return true;
}

void os_session_destroy(OsVirtualSession *s) {
    if (s == NULL) return;
    os_frames_destroy(&s->cache);
    free(s->workers);
    free(s->requests);
    free(s);
}
