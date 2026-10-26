#ifndef OS_TRACE_H
#define OS_TRACE_H

#include "events.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OS_TRACE_VERSION 1u

/* Owned, append-only capture. Initialize once, then destroy. The callback
 * records allocation failure in failed; it never silently drops an event. */
typedef struct {
    OsEvent *events;
    size_t count;
    size_t capacity;
    bool failed;
} OsEventTrace;

void os_trace_init(OsEventTrace *trace);
void os_trace_destroy(OsEventTrace *trace);
bool os_trace_append(OsEventTrace *trace, const OsEvent *event);
void os_trace_capture(const OsEvent *event, void *context);
bool os_trace_equal(const OsEventTrace *a, const OsEventTrace *b);

/* Stable FNV-1a64 digest over a versioned, little-endian field encoding:
 * version:u32, count:u64, then each event's type:u32, sequence:u64,
 * tick:u64, request_id:u32, worker_index:u64, page_id:u32.
 * SIZE_MAX worker sentinel is normalized to UINT64_MAX.
 * A failed/invalid trace returns zero. This is NOT a cryptographic hash. */
uint64_t os_trace_fingerprint(const OsEventTrace *trace);

#endif
