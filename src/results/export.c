#include "export.h"

#include "scheduler.h"
#include "events.h"
#include "request.h"

#include <inttypes.h>
#include <stdio.h>

typedef struct {
    FILE *out;
    bool csv;
    const char *section;
    size_t index;
    bool indexed;
    bool first;
} Writer;

/* All keys and textual values below are fixed vocabulary, never raw CSV or
 * JSON supplied by the user. Numeric values use the C locale (default). */
static void field(Writer *w, const char *key, const char *value, bool string) {
    if (w->csv) {
        (void)fprintf(w->out, "%s,", w->section);
        if (w->indexed) (void)fprintf(w->out, "%zu", w->index);
        (void)fprintf(w->out, ",%s,%s\n", key, value);
    } else {
        (void)fprintf(w->out, "%s\"%s\":", w->first ? "" : ",", key);
        if (string) (void)fprintf(w->out, "\"%s\"", value);
        else (void)fputs(value, w->out);
    }
    w->first = false;
}

#define W_U64(w, key, val) do { \
    char number[32]; \
    (void)snprintf(number, sizeof number, "%" PRIu64, (uint64_t)(val)); \
    field((w), (key), number, false); \
} while (0)
#define W_SIZE(w, key, val) do { \
    char number[32]; \
    (void)snprintf(number, sizeof number, "%zu", (size_t)(val)); \
    field((w), (key), number, false); \
} while (0)
#define W_INT(w, key, val) do { \
    char number[32]; \
    (void)snprintf(number, sizeof number, "%d", (int)(val)); \
    field((w), (key), number, false); \
} while (0)
#define W_DOUBLE(w, key, val) do { \
    char number[64]; \
    (void)snprintf(number, sizeof number, "%.17g", (double)(val)); \
    field((w), (key), number, false); \
} while (0)

static void write_config(Writer *w, const OsVirtualConfig *c) {
    field(w, "policy", os_policy_name(c->policy), true);
    W_SIZE(w, "worker_count", c->worker_count);
    W_U64(w, "quantum_tokens", c->quantum_tokens);
    W_U64(w, "aging_interval", c->aging_interval);
    field(w, "memory_enabled", c->memory_enabled ? "true" : "false", false);
    W_SIZE(w, "frame_capacity", c->frame_capacity);
    W_U64(w, "tokens_per_page", c->tokens_per_page);
    field(w, "page_policy", c->page_policy == OS_PAGE_LRU ? "lru" :
          c->page_policy == OS_PAGE_FIFO ? "fifo" : "opt", true);
    W_U64(w, "page_fault_latency_ticks", c->page_fault_latency_ticks);
    W_U64(w, "starvation_threshold_ticks", c->starvation_threshold_ticks);
}

static void write_request(Writer *w, const OsRequest *r, bool result) {
    W_U64(w, "id", r->id);
    W_U64(w, "arrival_tick", r->arrival_tick);
    W_U64(w, "total_tokens", r->total_tokens);
    W_U64(w, "predicted_tokens", r->predicted_tokens);
    W_INT(w, "priority", r->priority);
    if (!result) return;
    W_U64(w, "generated_tokens", r->generated_tokens);
    field(w, "state", os_request_state_name(r->state), true);
    if (r->first_run_tick == OS_REQUEST_NEVER)
        field(w, "first_run_tick", w->csv ? "" : "null", false);
    else W_U64(w, "first_run_tick", r->first_run_tick);
    if (r->finished_tick == OS_REQUEST_NEVER)
        field(w, "finished_tick", w->csv ? "" : "null", false);
    else W_U64(w, "finished_tick", r->finished_tick);
    W_U64(w, "waiting_ticks", r->waiting_ticks);
}

static void write_event(Writer *w, const OsEvent *e) {
    W_U64(w, "sequence", e->sequence);
    W_U64(w, "tick", e->tick);
    field(w, "type", os_event_type_name(e->type), true);
    W_U64(w, "request_id", e->request_id);
    if (e->worker_index == SIZE_MAX)
        field(w, "worker_index", w->csv ? "" : "null", false);
    else W_SIZE(w, "worker_index", e->worker_index);
    if (e->type == OS_EVENT_PAGE_HIT || e->type == OS_EVENT_PAGE_FAULT ||
        e->type == OS_EVENT_PAGE_EVICTED)
        W_U64(w, "page_id", e->page_id);
    else field(w, "page_id", w->csv ? "" : "null", false);
}

static void write_metrics(Writer *w, const OsRunSummary *m) {
    W_SIZE(w, "completed", m->completed);
    W_U64(w, "makespan_ticks", m->makespan_ticks);
    W_DOUBLE(w, "average_wait_ticks", m->average_wait_ticks);
    W_DOUBLE(w, "average_response_ticks", m->average_response_ticks);
    W_DOUBLE(w, "average_turnaround_ticks", m->average_turnaround_ticks);
    W_DOUBLE(w, "throughput_per_tick", m->throughput_per_tick);
    W_U64(w, "page_hits", m->page_hits);
    W_U64(w, "page_faults", m->page_faults);
    W_U64(w, "page_evictions", m->page_evictions);
    W_SIZE(w, "peak_resident_frames", m->peak_resident_frames);
    W_DOUBLE(w, "p50_wait_ticks", m->p50_wait_ticks);
    W_DOUBLE(w, "p95_wait_ticks", m->p95_wait_ticks);
    W_DOUBLE(w, "p50_response_ticks", m->p50_response_ticks);
    W_DOUBLE(w, "p95_response_ticks", m->p95_response_ticks);
    W_DOUBLE(w, "p50_turnaround_ticks", m->p50_turnaround_ticks);
    W_DOUBLE(w, "p95_turnaround_ticks", m->p95_turnaround_ticks);
    W_U64(w, "max_wait_ticks", m->max_wait_ticks);
    W_DOUBLE(w, "worker_utilization", m->worker_utilization);
    W_U64(w, "total_blocked_ticks", m->total_blocked_ticks);
    W_U64(w, "preemptions", m->preemptions);
    W_SIZE(w, "starvation_count", m->starvation_count);
}

static bool valid(const OsRunArtifact *a) {
    return a != NULL && a->input != NULL && a->result != NULL &&
           a->count != 0 && a->config != NULL && a->metrics != NULL &&
           a->trace != NULL && !a->trace->failed &&
           (a->trace->count == 0 || a->trace->events != NULL);
}

bool os_export_json(FILE *out, const OsRunArtifact *a) {
    if (out == NULL || !valid(a)) return false;
    (void)fprintf(out, "{\n\"schema\":\"os-llm-run\",\"version\":%u,"
                  "\"engine\":\"virtual\",\"source\":\"%s\",\"seed\":",
                  OS_RUN_EXPORT_VERSION, a->scenario_source ? "scenario" : "builtin");
    if (a->has_seed) (void)fprintf(out, "%" PRIu64, a->seed);
    else (void)fputs("null", out);
    (void)fputs(",\"config\":{", out);
    Writer w = {.out = out, .section = "config", .first = true};
    write_config(&w, a->config);
    (void)fputs("},\"input\":[", out);
    for (size_t i = 0; i < a->count; ++i) {
        if (i != 0) (void)fputc(',', out);
        (void)fputc('{', out);
        w = (Writer){.out = out, .section = "input", .index = i, .indexed = true, .first = true};
        write_request(&w, &a->input[i], false);
        (void)fputc('}', out);
    }
    (void)fputs("],\"requests\":[", out);
    for (size_t i = 0; i < a->count; ++i) {
        if (i != 0) (void)fputc(',', out);
        (void)fputc('{', out);
        w = (Writer){.out = out, .section = "request", .index = i, .indexed = true, .first = true};
        write_request(&w, &a->result[i], true);
        (void)fputc('}', out);
    }
    (void)fputs("],\"events\":[", out);
    for (size_t i = 0; i < a->trace->count; ++i) {
        if (i != 0) (void)fputc(',', out);
        (void)fputc('{', out);
        w = (Writer){.out = out, .section = "event", .index = i, .indexed = true, .first = true};
        write_event(&w, &a->trace->events[i]);
        (void)fputc('}', out);
    }
    (void)fputs("],\"metrics\":{", out);
    w = (Writer){.out = out, .section = "metric", .first = true};
    write_metrics(&w, a->metrics);
    (void)fprintf(out, "},\"trace_fingerprint\":\"%016" PRIx64
                  "\",\"finished\":%s}\n",
                  os_trace_fingerprint(a->trace), a->finished ? "true" : "false");
    return !ferror(out);
}

bool os_export_csv(FILE *out, const OsRunArtifact *a) {
    if (out == NULL || !valid(a)) return false;
    (void)fputs("section,index,field,value\n", out);
    Writer w = {.out = out, .csv = true, .section = "meta"};
    field(&w, "schema", "os-llm-run", true);
    W_U64(&w, "version", OS_RUN_EXPORT_VERSION);
    field(&w, "engine", "virtual", true);
    field(&w, "source", a->scenario_source ? "scenario" : "builtin", true);
    if (a->has_seed) W_U64(&w, "seed", a->seed);
    else field(&w, "seed", "", false);
    field(&w, "finished", a->finished ? "true" : "false", false);
    char fingerprint[32];
    (void)snprintf(fingerprint, sizeof fingerprint, "%016" PRIx64,
                   os_trace_fingerprint(a->trace));
    field(&w, "trace_fingerprint", fingerprint, true);
    w.section = "config";
    write_config(&w, a->config);
    for (size_t i = 0; i < a->count; ++i) {
        w = (Writer){.out = out, .csv = true, .section = "input", .index = i, .indexed = true};
        write_request(&w, &a->input[i], false);
        w.section = "request";
        write_request(&w, &a->result[i], true);
    }
    for (size_t i = 0; i < a->trace->count; ++i) {
        w = (Writer){.out = out, .csv = true, .section = "event", .index = i, .indexed = true};
        write_event(&w, &a->trace->events[i]);
    }
    w = (Writer){.out = out, .csv = true, .section = "metric"};
    write_metrics(&w, a->metrics);
    return !ferror(out);
}
