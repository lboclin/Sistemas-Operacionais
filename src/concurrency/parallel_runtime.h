#ifndef OS_PARALLEL_RUNTIME_H
#define OS_PARALLEL_RUNTIME_H

#include "virtual.h"

/* POSIX execution adapter: the coordinator alone owns PCB state, scheduler,
 * page cache, virtual clock, metrics and event ordering. Pthreads acknowledge
 * one simulated token task each via a bounded queue. No actual LLM inference
 * takes place; this is a concurrency teaching model, not a speed benchmark.
 * Results and events match os_virtual_run_observed for the same inputs.
 * On failure, input requests and summary are not modified. */
OsRunStatus os_posix_run(OsRequest *requests, size_t count,
                         const OsVirtualConfig *config, OsRunSummary *summary,
                         OsEventCallback observer, void *observer_context);

#endif
