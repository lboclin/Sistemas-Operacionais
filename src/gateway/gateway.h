#ifndef OS_GATEWAY_H
#define OS_GATEWAY_H

#include "request.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Validate a new workload before running it. IDs must be unique. */
bool os_gateway_validate(const OsRequest *requests, size_t count);
/* Admit one arrived request, assigning its stable ready-queue order. */
bool os_gateway_admit(OsRequest *request, uint64_t now, uint64_t order);
/* Returns OS_REQUEST_NEVER if the workload has no pending arrivals. */
uint64_t os_gateway_next_arrival(const OsRequest *requests, size_t count);

#endif
