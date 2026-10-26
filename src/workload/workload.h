#ifndef OS_WORKLOAD_H
#define OS_WORKLOAD_H

#include "request.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define OS_WORKLOAD_FORMAT_VERSION 1u
#define OS_WORKLOAD_MAX_REQUESTS 256u

typedef enum {
    OS_WORKLOAD_UNIFORM,
    OS_WORKLOAD_BURST,
    OS_WORKLOAD_STAGGERED
} OsWorkloadDistribution;

typedef struct {
    uint64_t seed;
    size_t count;
    uint32_t arrival_span;
    uint32_t min_tokens;
    uint32_t max_tokens;
    OsWorkloadDistribution distribution;
} OsWorkloadConfig;

const char *os_workload_distribution_name(OsWorkloadDistribution distribution);
/* Deterministic SplitMix64. Same config yields same PCBs on all C11 targets.
 * Returns false without writing when configuration/capacity is invalid. */
bool os_workload_generate(const OsWorkloadConfig *config,
                          OsRequest *requests, size_t capacity);
/* Versioned CSV, compatible with the legacy scenario columns and CLI. */
bool os_workload_write_csv(FILE *output, const OsWorkloadConfig *config,
                           const OsRequest *requests, size_t count);

#endif
