#ifndef V2SPEED_FALLBACK_PLANNER_H
#define V2SPEED_FALLBACK_PLANNER_H

#include "v2speed/smart_connect.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V2S_MAX_CONNECT_ATTEMPTS 8

typedef enum V2SFallbackStatus {
    V2S_FALLBACK_OK = 0,
    V2S_FALLBACK_INVALID_ARGUMENT = 1,
    V2S_FALLBACK_INVALID_PLAN = 2,
    V2S_FALLBACK_DUPLICATE_CANDIDATE = 3,
    V2S_FALLBACK_BUSY = 4,
    V2S_FALLBACK_EXHAUSTED = 5,
    V2S_FALLBACK_FINISHED = 6
} V2SFallbackStatus;

typedef enum V2SAttemptOutcome {
    V2S_ATTEMPT_TUNNEL_VERIFIED = 0,
    V2S_ATTEMPT_FAILED = 1,
    V2S_ATTEMPT_CANCELLED = 2
} V2SAttemptOutcome;

typedef enum V2SFallbackResult {
    V2S_FALLBACK_RESULT_ACTIVE = 0,
    V2S_FALLBACK_RESULT_CONNECTED = 1,
    V2S_FALLBACK_RESULT_CANCELLED = 2,
    V2S_FALLBACK_RESULT_EXHAUSTED = 3
} V2SFallbackResult;

typedef struct V2SFallbackPlanner {
    size_t candidate_indices[V2S_MAX_CANDIDATES];
    size_t ranked_count;
    size_t next_position;
    size_t attempts_started;
    size_t max_attempts;
    uint8_t in_flight;
    V2SFallbackResult result;
} V2SFallbackPlanner;

/* Copies a validated, unique ranked candidate-index plan into caller-owned state. */
V2SFallbackStatus v2s_fallback_init(
    V2SFallbackPlanner *planner,
    const V2SRankedCandidate *ranked,
    size_t ranked_count,
    size_t max_attempts);

/* Returns the next ranked input index. Only one attempt may be in flight. */
V2SFallbackStatus v2s_fallback_begin_next(
    V2SFallbackPlanner *planner,
    size_t *out_input_index);

/* Report success only after the OS and core confirm the tunnel is active. */
V2SFallbackStatus v2s_fallback_complete(
    V2SFallbackPlanner *planner,
    V2SAttemptOutcome outcome);

#ifdef __cplusplus
}
#endif

#endif
