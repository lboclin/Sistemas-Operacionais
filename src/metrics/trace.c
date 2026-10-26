#include "trace.h"
#include <stdlib.h>

void os_trace_init(OsEventTrace *trace) {
    if (trace != NULL) *trace = (OsEventTrace){0};
}

void os_trace_destroy(OsEventTrace *trace) {
    if (trace == NULL) return;
    free(trace->events);
    *trace = (OsEventTrace){0};
}

bool os_trace_append(OsEventTrace *trace, const OsEvent *event) {
    if (trace == NULL || event == NULL || trace->failed) return false;
    if (trace->count == trace->capacity) {
        const size_t max = SIZE_MAX / sizeof(*trace->events);
        if (trace->capacity == max) {
            trace->failed = true;
            return false;
        }
        size_t next = trace->capacity == 0 ? 16 : trace->capacity;
        if (next > max / 2) next = max;
        else if (trace->capacity != 0) next *= 2;
        if (next > max) next = max;
        if (next <= trace->count) {
            trace->failed = true;
            return false;
        }
        OsEvent *grown = realloc(trace->events, next * sizeof(*trace->events));
        if (grown == NULL) {
            trace->failed = true;
            return false;
        }
        trace->events = grown;
        trace->capacity = next;
    }
    trace->events[trace->count++] = *event;
    return true;
}

void os_trace_capture(const OsEvent *event, void *context) {
    if (context != NULL) (void)os_trace_append((OsEventTrace *)context, event);
}

static bool same_event(const OsEvent *a, const OsEvent *b) {
    return a->type == b->type && a->sequence == b->sequence &&
           a->tick == b->tick && a->request_id == b->request_id &&
           a->worker_index == b->worker_index && a->page_id == b->page_id;
}

bool os_trace_equal(const OsEventTrace *a, const OsEventTrace *b) {
    if (a == NULL || b == NULL || a->failed || b->failed ||
        (a->count != 0 && a->events == NULL) ||
        (b->count != 0 && b->events == NULL) || a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; ++i)
        if (!same_event(&a->events[i], &b->events[i])) return false;
    return true;
}

static void hash_le(uint64_t *hash, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        *hash ^= value & UINT64_C(255);
        *hash *= UINT64_C(1099511628211);
        value >>= 8;
    }
}

uint64_t os_trace_fingerprint(const OsEventTrace *trace) {
    if (trace == NULL || trace->failed ||
        (trace->count != 0 && trace->events == NULL)) return 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    hash_le(&hash, OS_TRACE_VERSION, 4);
    hash_le(&hash, (uint64_t)trace->count, 8);
    for (size_t i = 0; i < trace->count; ++i) {
        const OsEvent *e = &trace->events[i];
        hash_le(&hash, (uint32_t)e->type, 4);
        hash_le(&hash, e->sequence, 8);
        hash_le(&hash, e->tick, 8);
        hash_le(&hash, e->request_id, 4);
        const uint64_t worker = e->worker_index == SIZE_MAX ?
                                UINT64_MAX : (uint64_t)e->worker_index;
        hash_le(&hash, worker, 8);
        hash_le(&hash, e->page_id, 4);
    }
    return hash;
}
