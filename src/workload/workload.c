#include "workload.h"
#include <inttypes.h>
#include <limits.h>

static uint64_t next_random(uint64_t *state) {
    *state += UINT64_C(0x9e3779b97f4a7c15);
    uint64_t z = *state;
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

static bool valid(const OsWorkloadConfig *config) {
    return config != NULL && config->count > 0 &&
           config->count <= OS_WORKLOAD_MAX_REQUESTS &&
           config->min_tokens > 0 && config->max_tokens >= config->min_tokens &&
           config->distribution >= OS_WORKLOAD_UNIFORM &&
           config->distribution <= OS_WORKLOAD_STAGGERED;
}

const char *os_workload_distribution_name(OsWorkloadDistribution distribution) {
    switch (distribution) {
        case OS_WORKLOAD_UNIFORM: return "uniform";
        case OS_WORKLOAD_BURST: return "burst";
        case OS_WORKLOAD_STAGGERED: return "staggered";
        default: return "invalid";
    }
}

bool os_workload_generate(const OsWorkloadConfig *config,
                          OsRequest *requests, size_t capacity) {
    if (!valid(config) || requests == NULL || capacity < config->count)
        return false;
    uint64_t state = config->seed;
    const uint64_t token_range = (uint64_t)config->max_tokens -
                                 (uint64_t)config->min_tokens + 1;
    for (size_t i = 0; i < config->count; ++i) {
        uint64_t arrival = 0;
        if (config->distribution == OS_WORKLOAD_UNIFORM) {
            arrival = next_random(&state) % ((uint64_t)config->arrival_span + 1);
        } else if (config->distribution == OS_WORKLOAD_BURST) {
            const uint64_t cluster = next_random(&state) & 1u;
            const uint64_t jitter = next_random(&state) %
                                    ((uint64_t)config->arrival_span / 8 + 1);
            arrival = cluster ? (uint64_t)config->arrival_span - jitter : jitter;
        } else if (config->count > 1) {
            arrival = ((uint64_t)i * config->arrival_span) / (config->count - 1);
        }
        const uint32_t tokens = config->min_tokens +
                                (uint32_t)(next_random(&state) % token_range);
        const int priority = (int)(next_random(&state) % 6);
        const uint32_t error = (uint32_t)(next_random(&state) %
                                         ((uint64_t)tokens / 5 + 1));
        const bool plus = (next_random(&state) & 1u) != 0;
        uint64_t predicted = plus ? (uint64_t)tokens + error :
                            (tokens > error ? (uint64_t)tokens - error : 1);
        if (predicted > UINT32_MAX) predicted = UINT32_MAX;
        if (!os_request_init(&requests[i], (uint32_t)(i + 1), arrival, tokens,
                             (uint32_t)predicted, priority)) return false;
    }
    return true;
}

bool os_workload_write_csv(FILE *output, const OsWorkloadConfig *config,
                           const OsRequest *requests, size_t count) {
    if (output == NULL || !valid(config) || requests == NULL ||
        count != config->count) return false;
    for (size_t i = 0; i < count; ++i)
        if (!os_request_is_fresh(&requests[i]) || requests[i].id != i + 1)
            return false;
    if (fprintf(output, "# os-llm-scenario v%u seed=%" PRIu64
                " distribution=%s count=%zu\n",
                OS_WORKLOAD_FORMAT_VERSION, config->seed,
                os_workload_distribution_name(config->distribution), count) < 0)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const OsRequest *r = &requests[i];
        if (fprintf(output, "%" PRIu32 ",%" PRIu64 ",%" PRIu32
                    ",%" PRIu32 ",%d\n", r->id, r->arrival_tick,
                    r->total_tokens, r->predicted_tokens, r->priority) < 0)
            return false;
    }
    return !ferror(output);
}
