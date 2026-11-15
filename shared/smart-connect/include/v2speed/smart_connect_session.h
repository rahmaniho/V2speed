#ifndef V2SPEED_SMART_CONNECT_SESSION_H
#define V2SPEED_SMART_CONNECT_SESSION_H

#include "v2speed/connection_state.h"
#include "v2speed/fallback_planner.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum V2SSessionStatus {
    V2S_SESSION_OK = 0,
    V2S_SESSION_INVALID_ARGUMENT = 1,
    V2S_SESSION_INVALID_STATE = 2,
    V2S_SESSION_STALE_GENERATION = 3,
    V2S_SESSION_INVALID_MEASUREMENTS = 4,
    V2S_SESSION_NO_CANDIDATES = 5,
    V2S_SESSION_BUSY = 6,
    V2S_SESSION_EXHAUSTED = 7
} V2SSessionStatus;

typedef struct V2SSmartConnectSession {
    V2SConnectionMachine connection;
    V2SFallbackPlanner fallback;
    char server_ids[V2S_MAX_CANDIDATES][V2S_SERVER_ID_CAPACITY];
    size_t input_count;
    size_t ranked_count;
    size_t max_attempts;
} V2SSmartConnectSession;

V2SSessionStatus v2s_smart_session_init(
    V2SSmartConnectSession *session,
    size_t max_attempts);
V2SSessionStatus v2s_smart_session_begin(
    V2SSmartConnectSession *session,
    uint64_t *out_generation);
V2SSessionStatus v2s_smart_session_fetch_completed(
    V2SSmartConnectSession *session,
    uint64_t generation);
V2SSessionStatus v2s_smart_session_benchmark_completed(
    V2SSmartConnectSession *session,
    uint64_t generation,
    const V2SServerMeasurement *candidates,
    size_t candidate_count,
    const uint8_t custom_weights[V2S_SMART_CONNECT_METRIC_COUNT]);
V2SSessionStatus v2s_smart_session_next_candidate(
    V2SSmartConnectSession *session,
    uint64_t generation,
    char out_server_id[V2S_SERVER_ID_CAPACITY]);
V2SSessionStatus v2s_smart_session_complete_attempt(
    V2SSmartConnectSession *session,
    uint64_t generation,
    V2SAttemptOutcome outcome);
V2SSessionStatus v2s_smart_session_disconnect(
    V2SSmartConnectSession *session,
    uint64_t generation);

#ifdef __cplusplus
}
#endif

#endif
