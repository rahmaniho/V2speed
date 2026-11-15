#ifndef V2SPEED_SMART_CONNECT_PROBE_PLAN_H
#define V2SPEED_SMART_CONNECT_PROBE_PLAN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V2S_MAX_PROBE_PLAN_CANDIDATES 256
#define V2S_MAX_PROBE_CONCURRENCY 16

typedef enum V2SProbePlanStatus {
    V2S_PROBE_PLAN_OK = 0,
    V2S_PROBE_PLAN_INVALID_ARGUMENT = 1,
    V2S_PROBE_PLAN_INVALID_STATE = 2,
    V2S_PROBE_PLAN_BUSY = 3,
    V2S_PROBE_PLAN_COMPLETE = 4,
    V2S_PROBE_PLAN_DEADLINE_REACHED = 5,
    V2S_PROBE_PLAN_TIMED_OUT = 6,
    V2S_PROBE_PLAN_OUTPUT_TOO_SMALL = 7,
    V2S_PROBE_PLAN_CANCELLED = 8
} V2SProbePlanStatus;

typedef enum V2SProbeTaskState {
    V2S_PROBE_TASK_PENDING = 0,
    V2S_PROBE_TASK_IN_FLIGHT = 1,
    V2S_PROBE_TASK_SUCCEEDED = 2,
    V2S_PROBE_TASK_FAILED = 3,
    V2S_PROBE_TASK_TIMED_OUT = 4,
    V2S_PROBE_TASK_NOT_STARTED = 5,
    V2S_PROBE_TASK_CANCELLED = 6
} V2SProbeTaskState;

typedef struct V2SProbeTask {
    uint64_t started_at_ms;
    V2SProbeTaskState state;
} V2SProbeTask;

typedef struct V2SProbePlan {
    V2SProbeTask tasks[V2S_MAX_PROBE_PLAN_CANDIDATES];
    size_t candidate_count;
    size_t next_candidate;
    size_t concurrency_limit;
    size_t in_flight_count;
    uint64_t timeout_ms;
    uint64_t deadline_ms;
    uint64_t created_at_ms;
    uint8_t cancelled;
} V2SProbePlan;

/* Pure scheduling state only: performs no network I/O, clock reads or cancellation. */
V2SProbePlanStatus v2s_probe_plan_init(
    V2SProbePlan *plan,
    size_t candidate_count,
    size_t concurrency_limit,
    uint64_t per_probe_timeout_ms,
    uint64_t created_at_ms,
    uint64_t deadline_ms);

/* Returns the next sequential candidate index; output is unchanged on failure. */
V2SProbePlanStatus v2s_probe_plan_next(
    V2SProbePlan *plan,
    uint64_t now_ms,
    size_t *out_candidate_index);

/* Completes a dispatched task. A late completion records timeout, never success. */
V2SProbePlanStatus v2s_probe_plan_complete(
    V2SProbePlan *plan,
    size_t candidate_index,
    uint64_t now_ms,
    uint8_t succeeded);

/* Expire per-probe/global deadlines. Returns expired in-flight indices in order.
 * At the global deadline, unstarted tasks become NOT_STARTED, not failed probes.
 * Output storage and count are unchanged if validation/capacity checks fail. */
V2SProbePlanStatus v2s_probe_plan_expire(
    V2SProbePlan *plan,
    uint64_t now_ms,
    size_t *out_expired_indices,
    size_t expired_capacity,
    size_t *out_expired_count);

/* Cancel pending/in-flight tasks; returns in-flight indices for caller cancellation.
 * Repeated cancellation is idempotent and returns an empty index list. */
V2SProbePlanStatus v2s_probe_plan_cancel(
    V2SProbePlan *plan,
    size_t *out_in_flight_indices,
    size_t index_capacity,
    size_t *out_in_flight_count);

V2SProbePlanStatus v2s_probe_plan_task_state(
    const V2SProbePlan *plan,
    size_t candidate_index,
    V2SProbeTaskState *out_state);

#ifdef __cplusplus
}
#endif

#endif
