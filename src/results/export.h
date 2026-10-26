#ifndef OS_EXPORT_H
#define OS_EXPORT_H

#include "virtual.h"
#include "trace.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define OS_RUN_EXPORT_VERSION 1u

/* Input and result PCBs must refer to the same ordered requests. The caller
 * owns all memory. Source is recorded as a category, not a machine-specific
 * path, so the output is stable across directories and machines. */
typedef struct {
    const OsRequest *input;
    const OsRequest *result;
    size_t count;
    const OsVirtualConfig *config;
    const OsRunSummary *metrics;
    const OsEventTrace *trace;
    bool finished;
    bool scenario_source;
    bool has_seed;
    uint64_t seed;
} OsRunArtifact;

/* Versioned, deterministic representations of the SAME artifact.
 * JSON preserves exact integer types; CSV uses section,index,field,value
 * (long format). Neither function closes the supplied stream. */
bool os_export_json(FILE *out, const OsRunArtifact *artifact);
bool os_export_csv(FILE *out, const OsRunArtifact *artifact);

#endif
