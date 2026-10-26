#include "replay.h"

typedef struct {
    const OsEventTrace *expected;
    size_t count;
    size_t first_difference;
} Verifier;

static void compare_event(const OsEvent *event, void *context) {
    Verifier *v = context;
    const size_t index = v->count++;
    if (v->first_difference != SIZE_MAX) return;
    if (index >= v->expected->count) {
        v->first_difference = index;
        return;
    }
    const OsEvent *e = &v->expected->events[index];
    if (e->type != event->type || e->sequence != event->sequence ||
        e->tick != event->tick || e->request_id != event->request_id ||
        e->worker_index != event->worker_index || e->page_id != event->page_id)
        v->first_difference = index;
}

OsRunStatus os_trace_verify_replay(const OsRequest *requests, size_t count,
                                   const OsVirtualConfig *config,
                                   const OsEventTrace *expected,
                                   OsTraceVerification *result) {
    if (expected == NULL || result == NULL || expected->failed ||
        (expected->count != 0 && expected->events == NULL))
        return OS_RUN_INVALID;
    Verifier v = {.expected = expected, .first_difference = SIZE_MAX};
    OsVirtualSession *session = NULL;
    const OsRunStatus created = os_session_create(requests, count, config,
                                                  compare_event, &v, &session);
    if (created != OS_RUN_OK) return created;
    const OsRunStatus ran = os_session_run(session);
    os_session_destroy(session);
    if (ran != OS_RUN_OK) return ran;
    if (v.first_difference == SIZE_MAX && v.count != expected->count)
        v.first_difference = v.count < expected->count ? v.count : expected->count;
    *result = (OsTraceVerification){
        .matched = v.first_difference == SIZE_MAX,
        .expected_events = expected->count,
        .actual_events = v.count,
        .first_difference = v.first_difference
    };
    return OS_RUN_OK;
}
