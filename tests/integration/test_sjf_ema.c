#include "request.h"
#include "scheduler.h"
#include "virtual.h"

#include <assert.h>
#include <stdint.h>

static void prepare(OsRequest *r) {
    assert(os_request_init(&r[0], 1, 0, 12, 1, 1));
    assert(os_request_init(&r[1], 2, 1, 2, 99, 2));
    assert(os_request_init(&r[2], 3, 1, 3, 99, 1));
    assert(os_request_init(&r[3], 4, 1, 7, 1, 2));
}

static void run_and_compare(void) {
    OsRequest r[4];
    OsRunSummary m;
    OsVirtualConfig cfg = {.policy = OS_SJF_EMA, .worker_count = 1,
                           .ema_alpha = 0.5, .ema_initial_tokens = 6};
    prepare(r);
    assert(os_virtual_run(r, 4, &cfg, &m) == OS_RUN_OK);
    assert(m.completed == 4 && m.makespan_ticks == 24);
    assert(r[0].first_run_tick == 0 && r[0].finished_tick == 12);
    assert(r[1].first_run_tick == 12 && r[1].finished_tick == 14);
    assert(r[3].first_run_tick == 14 && r[3].finished_tick == 21);
    assert(r[2].first_run_tick == 21 && r[2].finished_tick == 24);
    prepare(r);
    cfg.policy = OS_SJF_ESTIMATED;
    assert(os_virtual_run(r, 4, &cfg, &m) == OS_RUN_OK);
    assert(r[3].first_run_tick == 12); /* Synthetic predicted_tokens differ. */
    prepare(r);
    cfg.policy = OS_SJF_ORACLE;
    assert(os_virtual_run(r, 4, &cfg, &m) == OS_RUN_OK);
    assert(r[1].first_run_tick == 12 && r[2].first_run_tick == 14);
    prepare(r);
    cfg.policy = OS_SJF_EMA;
    cfg.ema_alpha = 0.0;
    assert(os_virtual_run(r, 4, &cfg, &m) == OS_RUN_OK);
    assert(r[2].first_run_tick == 14); /* Alpha 0 keeps the initial tie. */
    prepare(r);
    cfg.ema_alpha = 1.0;
    assert(os_virtual_run(r, 4, &cfg, &m) == OS_RUN_OK);
    assert(r[3].first_run_tick == 14); /* Alpha 1 tracks the last completion. */
}

static void online_history_and_limits(void) {
    OsRequest r[4];
    prepare(r);
    OsEmaEstimator ema = {0};
    assert(!os_ema_init(&ema, r, 4, -0.01, 6));
    assert(!os_ema_init(&ema, r, 4, 1.01, 6));
    assert(!os_ema_init(&ema, r, 4, 0.5, 0));
    assert(os_ema_init(&ema, r, 4, 0.5, 6));
    assert(os_ema_predict(&ema, 1) == 6.0);
    assert(os_ema_predict(&ema, 2) == 6.0);
    assert(os_ema_predict(&ema, 12345) == 0.0);
    assert(!os_ema_observe(&ema, &r[0])); /* NEW requests cannot train. */
    assert(os_request_transition(&r[0], OS_READY));
    assert(os_request_transition(&r[0], OS_RUNNING));
    r[0].generated_tokens = 12;
    assert(os_request_transition(&r[0], OS_TERMINATED));
    assert(os_ema_observe(&ema, &r[0]));
    assert(os_ema_predict(&ema, 1) == 9.0);
    assert(os_ema_predict(&ema, 2) == 6.0);
    for (size_t i = 1; i < 4; ++i) {
        assert(os_request_transition(&r[i], OS_READY));
        r[i].ready_order = (uint64_t)i;
    }
    assert(os_scheduler_select_ema(r, 4, &ema) == 1);
    r[1].total_tokens = 1000; r[1].predicted_tokens = 1;
    r[2].total_tokens = 1; r[2].predicted_tokens = 1;
    r[3].total_tokens = 1; r[3].predicted_tokens = 999;
    assert(os_scheduler_select_ema(r, 4, &ema) == 1); /* No target leakage. */
    assert(os_scheduler_select(r, 4, OS_SJF_EMA, 12, 0) == SIZE_MAX);
    os_ema_destroy(&ema);
    assert(os_ema_predict(&ema, 1) == 0.0);
}

static void invalid_configuration(void) {
    OsRequest r;
    assert(os_request_init(&r, 10, 0, 1, 1, 0));
    OsRunSummary m;
    OsVirtualConfig cfg = {.policy = OS_SJF_EMA, .worker_count = 1,
                           .ema_alpha = 0.5, .ema_initial_tokens = 0};
    assert(os_virtual_run(&r, 1, &cfg, &m) == OS_RUN_INVALID);
    cfg.ema_initial_tokens = 2;
    cfg.ema_alpha = -0.1;
    assert(os_virtual_run(&r, 1, &cfg, &m) == OS_RUN_INVALID);
    cfg.ema_alpha = 1.1;
    assert(os_virtual_run(&r, 1, &cfg, &m) == OS_RUN_INVALID);
}

int main(void) {
    run_and_compare();
    online_history_and_limits();
    invalid_configuration();
    return 0;
}
