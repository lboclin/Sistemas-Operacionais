#include "virtual.h"
#include "session.h"

#include <stdlib.h>

OsRunStatus os_virtual_run_observed(OsRequest *requests, size_t count,
                                    const OsVirtualConfig *config, OsRunSummary *summary,
                                    OsEventCallback observer, void *observer_context) {
    if (summary == NULL || requests == NULL) return OS_RUN_INVALID;
    OsVirtualSession *session = NULL;
    const OsRunStatus init = os_session_create(requests, count, config,
                                               observer, observer_context, &session);
    if (init != OS_RUN_OK) return init;
    const OsRunStatus status = os_session_run(session);
    if (status == OS_RUN_OK) {
        OsSessionSnapshot snapshot;
        if (!os_session_snapshot(session, &snapshot, requests, count, NULL, 0)) {
            os_session_destroy(session);
            return OS_RUN_INVALID;
        }
        *summary = snapshot.metrics;
    }
    os_session_destroy(session);
    return status;
}

OsRunStatus os_virtual_run(OsRequest *requests, size_t count,
                           const OsVirtualConfig *config, OsRunSummary *summary) {
    return os_virtual_run_observed(requests, count, config, summary, NULL, NULL);
}
