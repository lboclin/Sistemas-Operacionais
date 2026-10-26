#include "workload.h"
#include "request.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool same_request(const OsRequest *a, const OsRequest *b) {
    return a->id == b->id && a->arrival_tick == b->arrival_tick &&
           a->total_tokens == b->total_tokens &&
           a->predicted_tokens == b->predicted_tokens &&
           a->priority == b->priority && a->state == b->state;
}

static void golden_seed(void) {
    OsWorkloadConfig cfg = {.seed = 42, .count = 4, .arrival_span = 10,
        .min_tokens = 2, .max_tokens = 8, .distribution = OS_WORKLOAD_UNIFORM};
    OsRequest a[4], b[4];
    assert(os_workload_generate(&cfg, a, 4));
    assert(os_workload_generate(&cfg, b, 4));
    for (size_t i = 0; i < 4; ++i) assert(same_request(&a[i], &b[i]));
    const uint64_t arrival[] = {9, 9, 2, 5};
    const uint32_t tokens[] = {7, 4, 8, 5};
    const uint32_t predicted[] = {7, 4, 7, 4};
    const int priority[] = {0, 2, 2, 3};
    for (size_t i = 0; i < 4; ++i) {
        assert(a[i].id == i + 1 && a[i].arrival_tick == arrival[i]);
        assert(a[i].total_tokens == tokens[i]);
        assert(a[i].predicted_tokens == predicted[i] && a[i].priority == priority[i]);
    }
    cfg.seed = 43;
    assert(os_workload_generate(&cfg, b, 4));
    bool different = false;
    for (size_t i = 0; i < 4; ++i)
        if (!same_request(&a[i], &b[i])) different = true;
    assert(different);
    cfg.seed = 42;
    FILE *file = tmpfile();
    assert(file != NULL);
    assert(os_workload_write_csv(file, &cfg, a, 4));
    rewind(file);
    char line[160];
    assert(fgets(line, sizeof line, file));
    assert(strcmp(line, "# os-llm-scenario v1 seed=42 distribution=uniform count=4\n") == 0);
    assert(fgets(line, sizeof line, file));
    assert(strcmp(line, "1,9,7,7,0\n") == 0);
    fclose(file);
}

static void distributions_and_bounds(void) {
    OsWorkloadConfig cfg = {.seed = 0, .count = 16, .arrival_span = 80,
        .min_tokens = 1, .max_tokens = 10};
    OsRequest req[16];
    for (int distribution = OS_WORKLOAD_UNIFORM;
         distribution <= OS_WORKLOAD_STAGGERED; ++distribution) {
        cfg.distribution = (OsWorkloadDistribution)distribution;
        assert(os_workload_generate(&cfg, req, 16));
        for (size_t i = 0; i < 16; ++i) {
            assert(req[i].id == i + 1 && req[i].arrival_tick <= 80);
            assert(req[i].total_tokens >= 1 && req[i].total_tokens <= 10);
            assert(req[i].predicted_tokens >= 1);
            assert(os_request_is_fresh(&req[i]));
            if (cfg.distribution == OS_WORKLOAD_BURST)
                assert(req[i].arrival_tick <= 10 || req[i].arrival_tick >= 70);
            if (cfg.distribution == OS_WORKLOAD_STAGGERED && i != 0)
                assert(req[i].arrival_tick >= req[i - 1].arrival_tick);
        }
    }
    cfg.count = 0;
    assert(!os_workload_generate(&cfg, req, 16));
    cfg.count = 17;
    assert(!os_workload_generate(&cfg, req, 16));
    cfg.count = 16;
    cfg.min_tokens = 0;
    assert(!os_workload_generate(&cfg, req, 16));
    cfg.min_tokens = 20;
    cfg.max_tokens = 10;
    assert(!os_workload_generate(&cfg, req, 16));
    assert(!os_workload_generate(NULL, req, 16));
}

int main(void) {
    golden_seed();
    distributions_and_bounds();
    return 0;
}
