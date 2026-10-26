#ifndef OS_REPLAY_H
#define OS_REPLAY_H

#include "session.h"
#include "trace.h"

typedef struct {
    bool matched;
    size_t expected_events;
    size_t actual_events;
    size_t first_difference; /* SIZE_MAX when matched */
} OsTraceVerification;

/* Deterministic replay reruns the virtual session from the original fresh
 * requests and configuration, comparing emitted events in order. It does NOT
 * infer or reconstruct hidden PCB states from the event log.
 * Returns OS_RUN_OK for a completed replay even if the trace differs. */
OsRunStatus os_trace_verify_replay(const OsRequest *requests, size_t count,
                                   const OsVirtualConfig *config,
                                   const OsEventTrace *expected,
                                   OsTraceVerification *result);

#endif
