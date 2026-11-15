#include "v2speed/smart_connect_session.h"

#include <string.h>

static V2SSessionStatus map_connection_status(V2SConnectionStatus status) {
    switch (status) {
        case V2S_CONNECTION_STATUS_OK: return V2S_SESSION_OK;
        case V2S_CONNECTION_STATUS_INVALID_ARGUMENT: return V2S_SESSION_INVALID_ARGUMENT;
        case V2S_CONNECTION_STATUS_BUSY: return V2S_SESSION_BUSY;
        case V2S_CONNECTION_STATUS_STALE_GENERATION: return V2S_SESSION_STALE_GENERATION;
        default: return V2S_SESSION_INVALID_STATE;
    }
}

static V2SSessionStatus validate_generation(
    const V2SSmartConnectSession *session,
    uint64_t generation) {
    if (session == NULL) return V2S_SESSION_INVALID_ARGUMENT;
    if (session->max_attempts == 0 || session->max_attempts > V2S_MAX_CONNECT_ATTEMPTS) {
        return V2S_SESSION_INVALID_STATE;
    }
    if (generation != session->connection.generation) return V2S_SESSION_STALE_GENERATION;
    return V2S_SESSION_OK;
}

static V2SSessionStatus fail_connection(V2SSmartConnectSession *session) {
    return map_connection_status(v2s_connection_apply(
        &session->connection,
        session->connection.generation,
        V2S_EVENT_OPERATION_FAILED));
}

static V2SSessionStatus fail_with_reason(
    V2SSmartConnectSession *session,
    V2SSessionStatus reason) {
    const V2SSessionStatus transition_status = fail_connection(session);
    return transition_status == V2S_SESSION_OK ? reason : transition_status;
}

V2SSessionStatus v2s_smart_session_init(
    V2SSmartConnectSession *session,
    size_t max_attempts) {
    V2SFallbackStatus fallback_status;
    if (session == NULL) return V2S_SESSION_INVALID_ARGUMENT;
    if (max_attempts == 0 || max_attempts > V2S_MAX_CONNECT_ATTEMPTS) {
        return V2S_SESSION_INVALID_ARGUMENT;
    }

    (void)memset(session, 0, sizeof(*session));
    if (v2s_connection_init(&session->connection) != V2S_CONNECTION_STATUS_OK) {
        return V2S_SESSION_INVALID_STATE;
    }
    fallback_status = v2s_fallback_init(&session->fallback, NULL, 0, max_attempts);
    if (fallback_status != V2S_FALLBACK_OK) return V2S_SESSION_INVALID_STATE;
    session->max_attempts = max_attempts;
    return V2S_SESSION_OK;
}

V2SSessionStatus v2s_smart_session_begin(
    V2SSmartConnectSession *session,
    uint64_t *out_generation) {
    V2SConnectionStatus connection_status;
    V2SFallbackStatus fallback_status;
    if (session == NULL || out_generation == NULL) return V2S_SESSION_INVALID_ARGUMENT;
    if (session->max_attempts == 0 || session->max_attempts > V2S_MAX_CONNECT_ATTEMPTS) {
        return V2S_SESSION_INVALID_STATE;
    }

    connection_status = v2s_connection_begin_smart(&session->connection, out_generation);
    if (connection_status != V2S_CONNECTION_STATUS_OK) return map_connection_status(connection_status);

    fallback_status = v2s_fallback_init(&session->fallback, NULL, 0, session->max_attempts);
    if (fallback_status != V2S_FALLBACK_OK) {
        return fail_with_reason(session, V2S_SESSION_INVALID_STATE);
    }
    (void)memset(session->server_ids, 0, sizeof(session->server_ids));
    session->input_count = 0;
    session->ranked_count = 0;
    return V2S_SESSION_OK;
}

V2SSessionStatus v2s_smart_session_fetch_completed(
    V2SSmartConnectSession *session,
    uint64_t generation) {
    V2SSessionStatus valid = validate_generation(session, generation);
    if (valid != V2S_SESSION_OK) return valid;
    if (session->connection.state != V2S_CONNECTION_FETCHING) return V2S_SESSION_INVALID_STATE;
    return map_connection_status(v2s_connection_apply(
        &session->connection, generation, V2S_EVENT_FETCH_COMPLETED));
}

V2SSessionStatus v2s_smart_session_benchmark_completed(
    V2SSmartConnectSession *session,
    uint64_t generation,
    const V2SServerMeasurement *candidates,
    size_t candidate_count,
    const uint8_t custom_weights[V2S_SMART_CONNECT_METRIC_COUNT]) {
    V2SRankedCandidate ranked[V2S_MAX_CANDIDATES];
    size_t ranked_count = 0;
    size_t i;
    V2SStatus score_status;
    V2SFallbackStatus fallback_status;
    V2SConnectionStatus connection_status;
    V2SSessionStatus valid = validate_generation(session, generation);

    if (valid != V2S_SESSION_OK) return valid;
    if (session->connection.state != V2S_CONNECTION_BENCHMARKING) return V2S_SESSION_INVALID_STATE;
    if (candidate_count > V2S_MAX_CANDIDATES || (candidate_count != 0 && candidates == NULL)) {
        return fail_with_reason(session, V2S_SESSION_INVALID_MEASUREMENTS);
    }

    if (custom_weights == NULL) {
        score_status = v2s_smart_connect_rank(
            candidates, candidate_count, ranked, V2S_MAX_CANDIDATES, &ranked_count);
    } else {
        score_status = v2s_smart_connect_rank_with_weights(
            candidates, candidate_count, custom_weights, ranked,
            V2S_MAX_CANDIDATES, &ranked_count);
    }
    if (score_status != V2S_STATUS_OK) {
        return fail_with_reason(session, V2S_SESSION_INVALID_MEASUREMENTS);
    }
    if (ranked_count == 0) {
        return fail_with_reason(session, V2S_SESSION_NO_CANDIDATES);
    }

    fallback_status = v2s_fallback_init(
        &session->fallback, ranked, ranked_count, session->max_attempts);
    if (fallback_status != V2S_FALLBACK_OK) {
        return fail_with_reason(session, V2S_SESSION_INVALID_MEASUREMENTS);
    }
    for (i = 0; i < candidate_count; i++) {
        (void)memcpy(session->server_ids[i], candidates[i].server_id, V2S_SERVER_ID_CAPACITY);
    }
    session->input_count = candidate_count;
    session->ranked_count = ranked_count;

    connection_status = v2s_connection_apply(
        &session->connection, generation, V2S_EVENT_BENCHMARK_COMPLETED);
    if (connection_status != V2S_CONNECTION_STATUS_OK) {
        return fail_with_reason(session, map_connection_status(connection_status));
    }
    return V2S_SESSION_OK;
}

V2SSessionStatus v2s_smart_session_next_candidate(
    V2SSmartConnectSession *session,
    uint64_t generation,
    char out_server_id[V2S_SERVER_ID_CAPACITY]) {
    size_t input_index;
    V2SFallbackStatus fallback_status;
    V2SSessionStatus valid = validate_generation(session, generation);
    if (valid != V2S_SESSION_OK) return valid;
    if (out_server_id == NULL) return V2S_SESSION_INVALID_ARGUMENT;
    if (session->connection.state != V2S_CONNECTION_CONNECTING) return V2S_SESSION_INVALID_STATE;

    fallback_status = v2s_fallback_begin_next(&session->fallback, &input_index);
    if (fallback_status == V2S_FALLBACK_EXHAUSTED) {
        return fail_with_reason(session, V2S_SESSION_EXHAUSTED);
    }
    if (fallback_status == V2S_FALLBACK_BUSY) return V2S_SESSION_BUSY;
    if (fallback_status != V2S_FALLBACK_OK) return V2S_SESSION_INVALID_STATE;
    if (input_index >= session->input_count || session->server_ids[input_index][0] == '\0') {
        const V2SFallbackStatus cancel_status =
            v2s_fallback_complete(&session->fallback, V2S_ATTEMPT_CANCELLED);
        if (cancel_status != V2S_FALLBACK_OK) return V2S_SESSION_INVALID_STATE;
        return fail_with_reason(session, V2S_SESSION_INVALID_MEASUREMENTS);
    }

    (void)memcpy(out_server_id, session->server_ids[input_index], V2S_SERVER_ID_CAPACITY);
    return V2S_SESSION_OK;
}

V2SSessionStatus v2s_smart_session_complete_attempt(
    V2SSmartConnectSession *session,
    uint64_t generation,
    V2SAttemptOutcome outcome) {
    V2SFallbackStatus fallback_status;
    V2SConnectionStatus connection_status;
    V2SSessionStatus valid = validate_generation(session, generation);
    if (valid != V2S_SESSION_OK) return valid;
    if (session->connection.state != V2S_CONNECTION_CONNECTING) return V2S_SESSION_INVALID_STATE;
    if (outcome < V2S_ATTEMPT_TUNNEL_VERIFIED || outcome > V2S_ATTEMPT_CANCELLED) {
        return V2S_SESSION_INVALID_ARGUMENT;
    }

    fallback_status = v2s_fallback_complete(&session->fallback, outcome);
    if (fallback_status == V2S_FALLBACK_INVALID_ARGUMENT) return V2S_SESSION_INVALID_STATE;
    if (fallback_status != V2S_FALLBACK_OK) return V2S_SESSION_INVALID_STATE;

    if (outcome == V2S_ATTEMPT_TUNNEL_VERIFIED) {
        connection_status = v2s_connection_apply(
            &session->connection, generation, V2S_EVENT_TUNNEL_ESTABLISHED);
        return map_connection_status(connection_status);
    }
    if (outcome == V2S_ATTEMPT_CANCELLED) {
        connection_status = v2s_connection_apply(
            &session->connection, generation, V2S_EVENT_USER_DISCONNECTED);
        return map_connection_status(connection_status);
    }
    if (session->fallback.result == V2S_FALLBACK_RESULT_EXHAUSTED) {
        return fail_with_reason(session, V2S_SESSION_EXHAUSTED);
    }
    return V2S_SESSION_OK;
}

V2SSessionStatus v2s_smart_session_disconnect(
    V2SSmartConnectSession *session,
    uint64_t generation) {
    V2SConnectionStatus connection_status;
    V2SSessionStatus valid = validate_generation(session, generation);
    if (valid != V2S_SESSION_OK) return valid;
    if (session->fallback.in_flight) {
        if (v2s_fallback_complete(&session->fallback, V2S_ATTEMPT_CANCELLED) != V2S_FALLBACK_OK) {
            return V2S_SESSION_INVALID_STATE;
        }
    }
    connection_status = v2s_connection_apply(
        &session->connection, generation, V2S_EVENT_USER_DISCONNECTED);
    return map_connection_status(connection_status);
}
