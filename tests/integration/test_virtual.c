#include "request.h"
#include "virtual.h"

#include <assert.h>
#include <stdint.h>

static void fill(OsRequest *req) {
    assert(os_request_init(&req[0], 1, 0, 6, 1));
    assert(os_request_init(&req[1], 2, 0, 2, 3));
    assert(os_request_init(&req[2], 3, 2, 3, 2));
}

int main(void) {
    OsRequest r[3];
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
                           .quantum_tokens = 2, .aging_interval = 2};
    OsRunSummary m;
    fill(r);
    assert(os_virtual_run(r, 3, &cfg, &m) == OS_RUN_OK);
    assert(m.completed == 3 && m.makespan_ticks == 11);
    assert(r[0].first_run_tick == 0 && r[0].finished_tick == 6);
    assert(r[1].first_run_tick == 6 && r[1].finished_tick == 8);
    assert(r[2].first_run_tick == 8 && r[2].finished_tick == 11);
    assert(r[0].waiting_ticks == 0 && r[1].waiting_ticks == 6 && r[2].waiting_ticks == 6);
    assert(m.average_wait_ticks == 4.0);
    assert(os_virtual_run(r, 3, &cfg, &m) == OS_RUN_INVALID); /* no implicit reset */

    fill(r);
    cfg.policy = OS_SJF;
    assert(os_virtual_run(r, 3, &cfg, &m) == OS_RUN_OK);
    assert(r[1].first_run_tick == 0 && r[1].finished_tick == 2);
    assert(r[2].first_run_tick == 2 && r[2].finished_tick == 5);
    assert(r[0].finished_tick == 11);

    fill(r);
    cfg.policy = OS_ROUND_ROBIN;
    cfg.quantum_tokens = 2;
    assert(os_virtual_run(r, 3, &cfg, &m) == OS_RUN_OK);
    assert(m.completed == 3 && m.makespan_ticks == 11);
    assert(r[0].processed_tokens == 6 && r[1].processed_tokens == 2);
    assert(r[1].first_run_tick > 0 && r[1].first_run_tick < 6);

    fill(r);
    cfg.worker_count = 2;
    cfg.policy = OS_FCFS;
    assert(os_virtual_run(r, 3, &cfg, &m) == OS_RUN_OK);
    assert(m.completed == 3 && m.makespan_ticks == 6);
    assert(r[1].first_run_tick == 0);

    /* HRRN: order A, C, B, D. FCFS gives A, B, C, D and SJF gives A, C, D, B. */
    OsRequest h[4];
    assert(os_request_init(&h[0], 1, 0, 8, 0));
    assert(os_request_init(&h[1], 2, 0, 10, 0));
    assert(os_request_init(&h[2], 3, 4, 4, 0));
    assert(os_request_init(&h[3], 4, 10, 2, 0));
    cfg.worker_count = 1;
    cfg.policy = OS_HRRN;
    assert(os_virtual_run(h, 4, &cfg, &m) == OS_RUN_OK);
    assert(h[0].first_run_tick == 0 && h[0].finished_tick == 8);
    assert(h[2].first_run_tick == 8 && h[2].finished_tick == 12);  /* 2.0 vs 1.8 */
    assert(h[1].first_run_tick == 12 && h[1].finished_tick == 22); /* 2.2 vs 2.0 */
    assert(h[3].first_run_tick == 22 && h[3].finished_tick == 24);
    assert(m.completed == 4 && m.preemptions == 0);

    OsRequest late;
    assert(os_request_init(&late, 99, 20, 1, 0));
    cfg.worker_count = 1;
    assert(os_virtual_run(&late, 1, &cfg, &m) == OS_RUN_OK);
    assert(late.first_run_tick == 20 && late.finished_tick == 21);
    assert(m.makespan_ticks == 21 && late.waiting_ticks == 0);

    OsRequest duplicates[2];
    assert(os_request_init(&duplicates[0], 4, 0, 1, 0));
    assert(os_request_init(&duplicates[1], 4, 0, 1, 0));
    assert(os_virtual_run(duplicates, 2, &cfg, &m) == OS_RUN_INVALID);

    fill(r);
    cfg.policy = (OsSchedulingPolicy)(OS_HRRN + 1);
    assert(os_virtual_run(r, 3, &cfg, &m) == OS_RUN_INVALID);
    cfg.policy = OS_ROUND_ROBIN;
    cfg.quantum_tokens = 0;
    assert(os_virtual_run(r, 3, &cfg, &m) == OS_RUN_INVALID);
    return 0;
}
