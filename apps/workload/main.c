#include "workload.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_u64(const char *text, uint64_t *value) {
    if (text == NULL || *text == '\0' || *text == '-') return false;
    char *end = NULL;
    errno = 0;
    const uintmax_t parsed = strtoumax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT64_MAX)
        return false;
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_u32(const char *text, uint32_t *value) {
    uint64_t number;
    if (!parse_u64(text, &number) || number > UINT32_MAX) return false;
    *value = (uint32_t)number;
    return true;
}

static void usage(void) {
    puts("Usage: os_llm_workload [--seed N] [--count N] [--distribution uniform|burst|staggered]");
    puts("                       [--arrival-span N] [--min-tokens N] [--max-tokens N] [--output FILE]");
    puts("Emits versioned CSV v1 compatible with os_llm_cli --scenario.");
}

int main(int argc, char **argv) {
    OsWorkloadConfig config = {.seed = 1, .count = 16, .arrival_span = 20,
        .min_tokens = 1, .max_tokens = 32, .distribution = OS_WORKLOAD_UNIFORM};
    const char *output_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        if (i + 1 >= argc) { usage(); return 2; }
        const char *key = argv[i];
        const char *value = argv[++i];
        if (strcmp(key, "--seed") == 0) {
            if (!parse_u64(value, &config.seed)) return 2;
        } else if (strcmp(key, "--count") == 0) {
            uint64_t n;
            if (!parse_u64(value, &n) || n > OS_WORKLOAD_MAX_REQUESTS) return 2;
            config.count = (size_t)n;
        } else if (strcmp(key, "--arrival-span") == 0) {
            if (!parse_u32(value, &config.arrival_span)) return 2;
        } else if (strcmp(key, "--min-tokens") == 0) {
            if (!parse_u32(value, &config.min_tokens)) return 2;
        } else if (strcmp(key, "--max-tokens") == 0) {
            if (!parse_u32(value, &config.max_tokens)) return 2;
        } else if (strcmp(key, "--distribution") == 0) {
            if (strcmp(value, "uniform") == 0) config.distribution = OS_WORKLOAD_UNIFORM;
            else if (strcmp(value, "burst") == 0) config.distribution = OS_WORKLOAD_BURST;
            else if (strcmp(value, "staggered") == 0) config.distribution = OS_WORKLOAD_STAGGERED;
            else return 2;
        } else if (strcmp(key, "--output") == 0) {
            output_path = value;
        } else {
            usage();
            return 2;
        }
    }
    OsRequest requests[OS_WORKLOAD_MAX_REQUESTS];
    if (!os_workload_generate(&config, requests, OS_WORKLOAD_MAX_REQUESTS)) {
        fputs("Invalid workload configuration\n", stderr);
        return 2;
    }
    FILE *output = stdout;
    if (output_path != NULL) {
        output = fopen(output_path, "w");
        if (output == NULL) { perror(output_path); return 1; }
    }
    const bool ok = os_workload_write_csv(output, &config, requests, config.count);
    if (output_path != NULL && fclose(output) != 0) {
        perror(output_path);
        return 1;
    }
    if (!ok) { fputs("Could not write workload\n", stderr); return 1; }
    return 0;
}
