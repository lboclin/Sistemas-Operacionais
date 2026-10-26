#ifndef OS_EVENTS_H
#define OS_EVENTS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    OS_EVENT_ARRIVED,
    OS_EVENT_DISPATCHED,
    OS_EVENT_PREEMPTED,
    OS_EVENT_COMPLETED,
    OS_EVENT_PAGE_HIT,
    OS_EVENT_PAGE_FAULT,
    OS_EVENT_PAGE_EVICTED,
    OS_EVENT_BLOCKED,
    OS_EVENT_WOKEN
} OsEventType;

typedef struct {
    OsEventType type;
    uint64_t tick; /* virtual tick, not wall-clock time */
    uint32_t request_id;
    size_t worker_index; /* SIZE_MAX for events not tied to a worker */
    uint32_t page_id;    /* meaningful only for OS_EVENT_PAGE_* */
} OsEvent;

/* Synchronous callback: data is temporary and must be copied if retained.
 * Observers must not mutate running requests or recursively run the simulator. */
typedef void (*OsEventCallback)(const OsEvent *event, void *context);

const char *os_event_type_name(OsEventType type);

#endif
