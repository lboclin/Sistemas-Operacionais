#include "trace.h"
#include "replay.h"
#include "request.h"
#include <assert.h>
#include <stdint.h>

static void golden_trace(void) {
    OsRequest req;
    assert(os_request_init(&req, 9, 0, 2, 2, 0));
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1};
    OsEventTrace trace;
    os_trace_init(&trace);
    OsVirtualSession *session = NULL;
    assert(os_session_create(&req, 1, &cfg, os_trace_capture, &trace, &session) == OS_RUN_OK);
    assert(os_session_run(session) == OS_RUN_OK);
    assert(trace.count == 3 && !trace.failed);
    assert(trace.events[0].type == OS_EVENT_ARRIVED && trace.events[0].tick == 0);
    assert(trace.events[1].type == OS_EVENT_DISPATCHED && trace.events[1].tick == 0);
    assert(trace.events[2].type == OS_EVENT_COMPLETED && trace.events[2].tick == 2);
    for (size_t i = 0; i < trace.count; ++i)
        assert(trace.events[i].sequence == i);
    assert(os_trace_fingerprint(&trace) == UINT64_C(0xac9ed82a221116e5));
    OsTraceVerification result;
    assert(os_trace_verify_replay(&req, 1, &cfg, &trace, &result) == OS_RUN_OK);
    assert(result.matched && result.first_difference == SIZE_MAX);
    assert(result.expected_events == 3 && result.actual_events == 3);
    trace.events[2].tick = 3;
    assert(os_trace_verify_replay(&req, 1, &cfg, &trace, &result) == OS_RUN_OK);
    assert(!result.matched && result.first_difference == 2);
    trace.events[2].tick = 2;
    os_session_destroy(session);
    os_trace_destroy(&trace);
}

static void replay_with_faults(void) {
    OsRequest req[2];
    assert(os_request_init(&req[0], 11, 0, 3, 3, 0));
    assert(os_request_init(&req[1], 22, 1, 2, 2, 0));
    OsVirtualConfig cfg = {.policy = OS_ROUND_ROBIN, .worker_count = 2,
        .quantum_tokens = 1, .memory_enabled = true, .frame_capacity = 1,
        .tokens_per_page = 2, .page_policy = OS_PAGE_LRU,
        .page_fault_latency_ticks = 1};
    OsEventTrace first, second;
    os_trace_init(&first);
    os_trace_init(&second);
    OsVirtualSession *s = NULL;
    assert(os_session_create(req, 2, &cfg, os_trace_capture, &first, &s) == OS_RUN_OK);
    assert(os_session_run(s) == OS_RUN_OK);
    os_session_destroy(s);
    assert(os_session_create(req, 2, &cfg, os_trace_capture, &second, &s) == OS_RUN_OK);
    assert(os_session_run(s) == OS_RUN_OK);
    os_session_destroy(s);
    assert(!first.failed && !second.failed && first.count > 10);
    assert(os_trace_equal(&first, &second));
    assert(os_trace_fingerprint(&first) == os_trace_fingerprint(&second));
    OsTraceVerification verification;
    assert(os_trace_verify_replay(req, 2, &cfg, &first, &verification) == OS_RUN_OK);
    assert(verification.matched);
    cfg.page_fault_latency_ticks = 0;
    assert(os_trace_verify_replay(req, 2, &cfg, &first, &verification) == OS_RUN_OK);
    assert(!verification.matched && verification.first_difference < first.count);
    assert(os_trace_append(&second, &first.events[0]));
    assert(!os_trace_equal(&first, &second));
    os_trace_destroy(&first);
    os_trace_destroy(&second);
}

static void invalid_inputs(void) {
    OsEventTrace trace;
    os_trace_init(&trace);
    assert(os_trace_fingerprint(NULL) == 0);
    assert(!os_trace_equal(NULL, &trace));
    assert(!os_trace_append(NULL, NULL));
    OsTraceVerification result;
    assert(os_trace_verify_replay(NULL, 0, NULL, &trace, &result) == OS_RUN_INVALID);
    trace.count = 1;
    assert(os_trace_fingerprint(&trace) == 0);
    assert(os_trace_verify_replay(NULL, 0, NULL, &trace, &result) == OS_RUN_INVALID);
    trace.count = 0;
    os_trace_destroy(&trace);
}

int main(void) {
    golden_trace();
    replay_with_faults();
    invalid_inputs();
    return 0;
}
