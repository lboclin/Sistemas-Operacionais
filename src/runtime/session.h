#ifndef OS_SESSION_H
#define OS_SESSION_H

#include "virtual.h"
#include "frames.h"

/* An opaque, single-threaded simulation. The session owns copies of the input
 * requests. Neither a callback nor an interface may mutate its state directly. */
typedef struct OsVirtualSession OsVirtualSession;

typedef struct {
    bool occupied;
    uint32_t request_id;     /* meaningful only when occupied */
    uint32_t quantum_used;
} OsWorkerSnapshot;

typedef struct {
    uint64_t tick;
    size_t request_count;
    size_t worker_count;
    size_t pending;
    size_t ready;
    size_t running;
    size_t blocked;
    size_t completed;
    size_t resident_frames;
    OsRunSummary metrics;    /* partial aggregate over completed requests */
    bool finished;
    uint64_t next_wakeup_tick; /* OS_REQUEST_NEVER if none blocked */
    size_t free_frames; /* 0 when paging is disabled */
    uint64_t internal_fragmentation_tokens; /* unused slots in resident fixed pages */
    size_t starving_ready; /* READY requests aged >= configured threshold */
    uint64_t max_ready_age_ticks;
} OsSessionSnapshot;

/* Observer is called synchronously from step; it must not call step/destroy.
 * The input requests may be freed or modified after successful creation. */
OsRunStatus os_session_create(const OsRequest *requests, size_t count,
                              const OsVirtualConfig *config,
                              OsEventCallback observer, void *context,
                              OsVirtualSession **out);

/* Each successful step processes one tick of work for each occupied worker.
 * An idle session jumps to its next arrival before dispatch. Completion is
 * idempotent; further steps return OS_RUN_OK without changing state. */
OsRunStatus os_session_step(OsVirtualSession *session);
OsRunStatus os_session_run(OsVirtualSession *session);

/* Read-only snapshot. Buffers may be NULL only when their capacities are zero;
 * set both capacities to zero to retrieve only totals. If buffers are supplied,
 * they must fit the *entire* request/worker collections. Failure writes nothing. */
bool os_session_snapshot(const OsVirtualSession *session,
                         OsSessionSnapshot *snapshot,
                         OsRequest *requests, size_t request_capacity,
                         OsWorkerSnapshot *workers, size_t worker_capacity);
/* Request-scoped unblock time; returns NEVER when request exists but is not blocked.
 * Invalid ID or NULL output returns false without writing. */
bool os_session_request_wakeup(const OsVirtualSession *session,
                               uint32_t request_id, uint64_t *wake_tick);
void os_session_destroy(OsVirtualSession *session);

#endif
