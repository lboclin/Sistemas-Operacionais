#include "events.h"

const char *os_event_type_name(OsEventType type) {
    switch (type) {
        case OS_EVENT_ARRIVED: return "arrived";
        case OS_EVENT_DISPATCHED: return "dispatched";
        case OS_EVENT_PREEMPTED: return "preempted";
        case OS_EVENT_COMPLETED: return "completed";
        case OS_EVENT_PAGE_HIT: return "page_hit";
        case OS_EVENT_PAGE_FAULT: return "page_fault";
        case OS_EVENT_PAGE_EVICTED: return "page_evicted";
        default: return "unknown";
    }
}
