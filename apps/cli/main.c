#include "request.h"
#include "scheduler.h"
#include "session.h"
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
    puts("       [--steps N] [--snapshots]");
    puts("--steps N limits session advances; --snapshots shows the initial state and each step.");
    puts("An idle step may jump to the next arrival tick.");
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


static bool parse_u64(const char *s, uint64_t *out) {
    if (s == NULL || *s == '\0' || *s == '-') return false;
    char *end = NULL;
    errno = 0;
    const uintmax_t value = strtoumax(s, &end, 10);
    if (errno || end == s || *end || value > UINT64_MAX) return false;
    *out = (uint64_t)value;
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


static void print_snapshot(uint64_t step, const OsSessionSnapshot *snap,
                           const OsRequest *requests, const OsWorkerSnapshot *workers) {
    printf("snapshot step=%" PRIu64 " tick=%" PRIu64
           " pending=%zu ready=%zu running=%zu blocked=%zu completed=%zu"
           " resident=%zu finished=%s faults=%" PRIu64 " hits=%" PRIu64 " evictions=%" PRIu64 "\n",
           step, snap->tick, snap->pending, snap->ready, snap->running,
           snap->blocked, snap->completed, snap->resident_frames,
           snap->finished ? "yes" : "no", snap->metrics.page_faults,
           snap->metrics.page_hits, snap->metrics.page_evictions);
    for (size_t i = 0; i < snap->worker_count; ++i) {
        if (workers[i].occupied)
            printf("  worker=%zu request=%" PRIu32 " quantum=%" PRIu32 "\n",
                   i, workers[i].request_id, workers[i].quantum_used);
        else
            printf("  worker=%zu idle\n", i);
    }
    for (size_t i = 0; i < snap->request_count; ++i) {
        const OsRequest *r = &requests[i];
        printf("  pcb=%" PRIu32 " state=%s tokens=%" PRIu32 "/%" PRIu32 " wait=%" PRIu64 "\n",
               r->id, os_request_state_name(r->state), r->generated_tokens,
               r->total_tokens, r->waiting_ticks);
    }
}

static void format_tick(uint64_t tick, char *buffer, size_t capacity) {
    if (tick == OS_REQUEST_NEVER)
        (void)snprintf(buffer, capacity, "-");
    else
        (void)snprintf(buffer, capacity, "%" PRIu64, tick);
}

int main(int argc, char **argv) {
    OsVirtualConfig cfg = {.policy = OS_FCFS, .worker_count = 1,
                           .quantum_tokens = 2, .aging_interval = 2};
    const char *scenario = NULL;
    bool trace = false;
    bool page_option = false;
    bool snapshots = false;
    bool has_step_limit = false;
    uint64_t step_limit = 0;
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
        } else if (!strcmp(argv[i], "--snapshots")) {
            snapshots = true;
        } else if (!strcmp(argv[i], "--steps") && i + 1 < argc) {
            if (!parse_u64(argv[++i], &step_limit)) {
                fputs("Invalid --steps value\n", stderr);
                return 2;
            }
            has_step_limit = true;
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
    bool finished = true;
    if (has_step_limit || snapshots) {
        OsVirtualSession *session = NULL;
        OsRunStatus status = os_session_create(requests, count, &cfg,
                                               trace ? print_event : NULL, NULL, &session);
        if (status != OS_RUN_OK) {
            fprintf(stderr, "Session creation failed (status=%d)\n", status);
            return 1;
        }
        OsSessionSnapshot snap;
        OsWorkerSnapshot workers[4096];
        if (!os_session_snapshot(session, &snap, requests, count, workers, cfg.worker_count)) {
            fputs("Could not read initial snapshot\n", stderr);
            os_session_destroy(session);
            return 1;
        }
        uint64_t steps_done = 0;
        if (snapshots) print_snapshot(steps_done, &snap, requests, workers);
        while (!snap.finished && (!has_step_limit || steps_done < step_limit)) {
            status = os_session_step(session);
            if (status != OS_RUN_OK ||
                !os_session_snapshot(session, &snap, requests, count, workers, cfg.worker_count)) {
                fprintf(stderr, "Session step failed (status=%d)\n", status);
                os_session_destroy(session);
                return 1;
            }
            ++steps_done;
            if (snapshots) print_snapshot(steps_done, &snap, requests, workers);
        }
        summary = snap.metrics;
        finished = snap.finished;
        printf("Session steps executed: %" PRIu64 "\n", steps_done);
        os_session_destroy(session);
    } else {
        const OsRunStatus status = os_virtual_run_observed(requests, count, &cfg, &summary,
                                                            trace ? print_event : NULL, NULL);
        if (status != OS_RUN_OK) {
            fprintf(stderr, "Simulation failed (status=%d)\n", status);
            return 1;
        }
    }
    printf("Policy: %s | Workers: %zu | Virtual tick: %" PRIu64 " | Execution: %s\n",
           os_policy_name(cfg.policy), cfg.worker_count, summary.makespan_ticks,
           finished ? "completed" : "paused");
    puts("ID  Arrival  Tokens  First  Finish  Wait  State");
    for (size_t i = 0; i < count; ++i) {
        OsRequest *r = &requests[i];
        char first[32], finish[32];
        format_tick(r->first_run_tick, first, sizeof first);
        format_tick(r->finished_tick, finish, sizeof finish);
        printf("%-3" PRIu32 " %-8" PRIu64 " %-7" PRIu32 " %-6s %-7s %-5" PRIu64 " %s\n",
               r->id, r->arrival_tick, r->total_tokens, first,
               finish, r->waiting_ticks, os_request_state_name(r->state));
    }
    printf("Completed=%zu/%zu, average wait=%.2f, response=%.2f, turnaround=%.2f, throughput=%.4f requests/tick\n",
           summary.completed, count, summary.average_wait_ticks, summary.average_response_ticks,
           summary.average_turnaround_ticks, summary.throughput_per_tick);
    if (cfg.memory_enabled)
        printf("Memory: frames=%zu, tokens/page=%" PRIu32 ", accesses=%" PRIu64
               ", hits=%" PRIu64 ", faults=%" PRIu64 ", evictions=%" PRIu64
               ", peak-resident=%zu\n", cfg.frame_capacity, cfg.tokens_per_page,
               summary.page_hits + summary.page_faults, summary.page_hits,
               summary.page_faults, summary.page_evictions, summary.peak_resident_frames);
    return 0;
}
