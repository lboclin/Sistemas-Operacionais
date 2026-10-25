#include "request.h"
#include "scheduler.h"
#include "virtual.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REQUESTS 256

static void usage(const char *app) {
    printf("Usage: %s [--policy fcfs|sjf|oracle|rr|priority] [--workers N] [--quantum N]\n"
           "       [--scenario FILE] [--frames N --page-tokens N --page-policy fifo|lru] [--trace]\n", app);
    puts("Input CSV (without header): id,arrival_tick,total_tokens,predicted_tokens,priority");
    puts("Virtual simulator only. Each token costs one virtual tick.");
    puts("Online paging is optional; page faults have no additional time cost.");
    puts("OPT page replacement is available only in offline trace replay.");
}

static bool parse_uint(const char *s, uint32_t *out) {
    if (s == NULL || *s == '\0' || *s == '-') return false;
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || end == s || *end || v > UINT32_MAX) return false;
    *out = (uint32_t)v;
    return true;
}

static bool load_csv(const char *path, OsRequest *requests, size_t *count) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) { perror(path); return false; }
    char line[512];
    size_t n = 0;
    unsigned line_number = 0;
    bool valid = true;
    while (fgets(line, sizeof line, fp)) {
        ++line_number;
        if (strchr(line, '\n') == NULL && !feof(fp)) { valid = false; break; }
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        uint32_t id, arrival, total, predicted;
        int priority;
        char extra;
        if (sscanf(line, " %" SCNu32 " , %" SCNu32 " , %" SCNu32 " , %" SCNu32 " , %d %c",
                   &id, &arrival, &total, &predicted, &priority, &extra) != 5 ||
            n == MAX_REQUESTS || !os_request_init(&requests[n], id, arrival, total, predicted, priority)) {
            valid = false; break;
        }
        ++n;
    }
    if (ferror(fp) || n == 0) valid = false;
    fclose(fp);
    if (!valid) fprintf(stderr, "Invalid scenario at or near line %u (max %u requests)\n", line_number, MAX_REQUESTS);
    *count = n;
    return valid;
}

static void print_event(const OsEvent *event, void *context) {
    (void)context;
    printf("event tick=%" PRIu64 " type=%s request=%" PRIu32,
           event->tick, os_event_type_name(event->type), event->request_id);
    if (event->worker_index != SIZE_MAX) printf(" worker=%zu", event->worker_index);
    if (event->type == OS_EVENT_PAGE_HIT || event->type == OS_EVENT_PAGE_FAULT ||
        event->type == OS_EVENT_PAGE_EVICTED)
        printf(" page=%" PRIu32, event->page_id);
    putchar('\n');
}

int main(int argc, char **argv) {
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
                           .quantum_tokens = 2, .aging_interval = 2};
    const char *scenario = NULL;
    bool trace = false;
    bool page_option = false;
    cfg.tokens_per_page = 2;
    cfg.page_policy = OS_PAGE_FIFO;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        if (!strcmp(argv[i], "--policy") && i + 1 < argc) {
            const char *value = argv[++i];
            if (!strcmp(value, "fcfs")) cfg.policy = OS_FCFS;
            else if (!strcmp(value, "sjf")) cfg.policy = OS_SJF_ESTIMATED;
            else if (!strcmp(value, "oracle")) cfg.policy = OS_SJF_ORACLE;
            else if (!strcmp(value, "rr")) cfg.policy = OS_ROUND_ROBIN;
            else if (!strcmp(value, "priority")) cfg.policy = OS_PRIORITY_AGING;
            else { fprintf(stderr, "Unknown policy: %s\n", value); return 2; }
        } else if (!strcmp(argv[i], "--workers") && i + 1 < argc) {
            uint32_t workers;
            if (!parse_uint(argv[++i], &workers) || workers == 0 || workers > 4096) return 2;
            cfg.worker_count = workers;
        } else if (!strcmp(argv[i], "--quantum") && i + 1 < argc) {
            if (!parse_uint(argv[++i], &cfg.quantum_tokens) || cfg.quantum_tokens == 0) return 2;
        } else if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            scenario = argv[++i];
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            uint32_t frames;
            if (!parse_uint(argv[++i], &frames) || frames == 0 || frames > 4096) return 2;
            cfg.memory_enabled = true;
            cfg.frame_capacity = frames;
        } else if (!strcmp(argv[i], "--page-tokens") && i + 1 < argc) {
            if (!parse_uint(argv[++i], &cfg.tokens_per_page) || cfg.tokens_per_page == 0) return 2;
            page_option = true;
        } else if (!strcmp(argv[i], "--page-policy") && i + 1 < argc) {
            const char *value = argv[++i];
            if (!strcmp(value, "fifo")) cfg.page_policy = OS_PAGE_FIFO;
            else if (!strcmp(value, "lru")) cfg.page_policy = OS_PAGE_LRU;
            else { fprintf(stderr, "Unknown online page policy: %s\n", value); return 2; }
            page_option = true;
        } else if (!strcmp(argv[i], "--trace")) {
            trace = true;
        } else { fprintf(stderr, "Invalid argument: %s\n", argv[i]); usage(argv[0]); return 2; }
    }
    if (page_option && !cfg.memory_enabled) {
        fputs("--page-tokens and --page-policy require --frames\n", stderr);
        return 2;
    }
    OsRequest requests[MAX_REQUESTS];
    size_t count = 3;
    if (scenario != NULL) {
        if (!load_csv(scenario, requests, &count)) return 2;
    } else {
        os_request_init(&requests[0], 1, 0, 6, 5, 1);
        os_request_init(&requests[1], 2, 0, 2, 2, 3);
        os_request_init(&requests[2], 3, 2, 3, 4, 2);
    }
    OsRunSummary summary;
    const OsRunStatus status = os_virtual_run_observed(requests, count, &cfg, &summary,
                                                        trace ? print_event : NULL, NULL);
    if (status != OS_RUN_OK) { fprintf(stderr, "Simulation failed (status=%d)\n", status); return 1; }
    printf("Policy: %s | Workers: %zu | Virtual makespan: %" PRIu64 " ticks\n",
           os_policy_name(cfg.policy), cfg.worker_count, summary.makespan_ticks);
    puts("ID  Arrival  Tokens  First  Finish  Wait  State");
    for (size_t i = 0; i < count; ++i) {
        OsRequest *r = &requests[i];
        printf("%-3" PRIu32 " %-8" PRIu64 " %-7" PRIu32 " %-6" PRIu64 " %-7" PRIu64 " %-5" PRIu64 " %s\n",
               r->id, r->arrival_tick, r->total_tokens, r->first_run_tick,
               r->finished_tick, r->waiting_ticks, os_request_state_name(r->state));
    }
    printf("Average wait=%.2f, response=%.2f, turnaround=%.2f, throughput=%.4f requests/tick\n",
           summary.average_wait_ticks, summary.average_response_ticks,
           summary.average_turnaround_ticks, summary.throughput_per_tick);
    if (cfg.memory_enabled)
        printf("Memory: frames=%zu, tokens/page=%" PRIu32 ", accesses=%" PRIu64
               ", hits=%" PRIu64 ", faults=%" PRIu64 ", evictions=%" PRIu64
               ", peak-resident=%zu\n", cfg.frame_capacity, cfg.tokens_per_page,
               summary.page_hits + summary.page_faults, summary.page_hits,
               summary.page_faults, summary.page_evictions, summary.peak_resident_frames);
    return 0;
}
