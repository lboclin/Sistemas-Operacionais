#include "export.h"
#include "request.h"
#include "trace.h"
#include "virtual.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_stream(FILE *f) {
    assert(fflush(f) == 0);
    assert(fseek(f, 0, SEEK_END) == 0);
    long n = ftell(f);
    assert(n >= 0);
    assert(fseek(f, 0, SEEK_SET) == 0);
    char *data = malloc((size_t)n + 1);
    assert(data != NULL);
    assert(fread(data, 1, (size_t)n, f) == (size_t)n);
    data[n] = '\0';
    return data;
}

static void test_export(void) {
    OsRequest input[2], output[2];
    assert(os_request_init(&input[0], 10, 0, 3, 3, 2));
    assert(os_request_init(&input[1], 20, 1, 2, 2, 1));
    memcpy(output, input, sizeof input);
    const OsVirtualConfig cfg = {.policy = OS_ROUND_ROBIN, .worker_count = 1,
        .quantum_tokens = 1, .memory_enabled = true, .frame_capacity = 2,
        .tokens_per_page = 2, .page_policy = OS_PAGE_LRU,
        .page_fault_latency_ticks = 1, .starvation_threshold_ticks = 2};
    OsEventTrace trace;
    os_trace_init(&trace);
    OsRunSummary summary;
    assert(os_virtual_run_observed(output, 2, &cfg, &summary,
                                   os_trace_capture, &trace) == OS_RUN_OK);
    assert(!trace.failed && trace.count > 4 && summary.completed == 2);
    const OsRunArtifact a = {.input = input, .result = output, .count = 2,
        .config = &cfg, .metrics = &summary, .trace = &trace, .finished = true,
        .scenario_source = true, .has_seed = true, .seed = 42};
    FILE *json = tmpfile();
    FILE *csv = tmpfile();
    assert(json != NULL && csv != NULL);
    assert(os_export_json(json, &a));
    assert(os_export_csv(csv, &a));
    char *j = read_stream(json);
    char *c = read_stream(csv);
    assert(strstr(j, "\"schema\":\"os-llm-run\"") != NULL);
    assert(strstr(j, "\"version\":1") != NULL);
    assert(strstr(j, "\"seed\":42") != NULL);
    assert(strstr(j, "\"page_fault_latency_ticks\":1") != NULL);
    assert(strstr(j, "\"trace_fingerprint\":\"") != NULL);
    assert(strstr(j, "\"events\":[{") != NULL);
    assert(strstr(j, "\"completed\":2") != NULL);
    assert(strstr(c, "section,index,field,value\n") == c);
    assert(strstr(c, "meta,,seed,42\n") != NULL);
    assert(strstr(c, "config,,page_policy,lru\n") != NULL);
    assert(strstr(c, "input,0,id,10\n") != NULL);
    assert(strstr(c, "request,1,state,TERMINATED\n") != NULL ||
           strstr(c, "request,1,state,terminated\n") != NULL);
    assert(strstr(c, "event,0,sequence,0\n") != NULL);
    assert(strstr(c, "metric,,completed,2\n") != NULL);
    FILE *json2 = tmpfile();
    assert(json2 != NULL && os_export_json(json2, &a));
    char *j2 = read_stream(json2);
    assert(strcmp(j, j2) == 0);
    OsRunArtifact invalid = a;
    invalid.trace = NULL;
    assert(!os_export_json(json2, &invalid));
    invalid = a;
    invalid.has_seed = false;
    FILE *missing_seed = tmpfile();
    assert(missing_seed != NULL && os_export_json(missing_seed, &invalid));
    char *j3 = read_stream(missing_seed);
    assert(strstr(j3, "\"seed\":null") != NULL);
    free(j3);
    free(j2);
    free(j);
    free(c);
    fclose(missing_seed);
    fclose(json2);
    fclose(json);
    fclose(csv);
    os_trace_destroy(&trace);
}

int main(void) {
    test_export();
    return 0;
}
