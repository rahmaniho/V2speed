#include "v2speed/fallback_planner.h"

#include <math.h>
#include <string.h>

V2SFallbackStatus v2s_fallback_init(
    V2SFallbackPlanner *planner,
    const V2SRankedCandidate *ranked,
    size_t ranked_count,
    size_t max_attempts) {
    V2SFallbackPlanner initialized = {0};
    size_t i;
    size_t j;

    if (planner == NULL) return V2S_FALLBACK_INVALID_ARGUMENT;
    if (ranked_count > V2S_MAX_CANDIDATES || max_attempts == 0 ||
        max_attempts > V2S_MAX_CONNECT_ATTEMPTS) {
        return V2S_FALLBACK_INVALID_PLAN;
    }
    if (ranked_count != 0 && ranked == NULL) return V2S_FALLBACK_INVALID_ARGUMENT;

    for (i = 0; i < ranked_count; i++) {
        if (!isfinite(ranked[i].score) || ranked[i].score < 0.0 || ranked[i].score > 100.0 ||
            !isfinite(ranked[i].evidence_coverage) || ranked[i].evidence_coverage < 0.0 ||
            ranked[i].evidence_coverage > 100.0) {
            return V2S_FALLBACK_INVALID_PLAN;
        }
        for (j = 0; j < i; j++) {
            if (ranked[i].input_index == ranked[j].input_index) {
                return V2S_FALLBACK_DUPLICATE_CANDIDATE;
            }
        }
        initialized.candidate_indices[i] = ranked[i].input_index;
    }

    initialized.ranked_count = ranked_count;
    initialized.max_attempts = max_attempts;
    initialized.result = V2S_FALLBACK_RESULT_ACTIVE;
    *planner = initialized;
    return V2S_FALLBACK_OK;
}

V2SFallbackStatus v2s_fallback_begin_next(
    V2SFallbackPlanner *planner,
    size_t *out_input_index) {
    if (planner == NULL || out_input_index == NULL) return V2S_FALLBACK_INVALID_ARGUMENT;
    if (planner->result == V2S_FALLBACK_RESULT_EXHAUSTED) return V2S_FALLBACK_EXHAUSTED;
    if (planner->result != V2S_FALLBACK_RESULT_ACTIVE) return V2S_FALLBACK_FINISHED;
    if (planner->in_flight) return V2S_FALLBACK_BUSY;
    if (planner->attempts_started >= planner->max_attempts ||
        planner->next_position >= planner->ranked_count) {
        planner->result = V2S_FALLBACK_RESULT_EXHAUSTED;
        return V2S_FALLBACK_EXHAUSTED;
    }

    *out_input_index = planner->candidate_indices[planner->next_position];
    planner->next_position++;
    planner->attempts_started++;
    planner->in_flight = 1;
    return V2S_FALLBACK_OK;
}

V2SFallbackStatus v2s_fallback_complete(
    V2SFallbackPlanner *planner,
    V2SAttemptOutcome outcome) {
    if (planner == NULL) return V2S_FALLBACK_INVALID_ARGUMENT;
    if (!planner->in_flight) return V2S_FALLBACK_INVALID_ARGUMENT;
    if (outcome < V2S_ATTEMPT_TUNNEL_VERIFIED || outcome > V2S_ATTEMPT_CANCELLED) {
        return V2S_FALLBACK_INVALID_ARGUMENT;
    }

    planner->in_flight = 0;
    switch (outcome) {
        case V2S_ATTEMPT_TUNNEL_VERIFIED:
            planner->result = V2S_FALLBACK_RESULT_CONNECTED;
            break;
        case V2S_ATTEMPT_CANCELLED:
            planner->result = V2S_FALLBACK_RESULT_CANCELLED;
            break;
        case V2S_ATTEMPT_FAILED:
            if (planner->attempts_started >= planner->max_attempts ||
                planner->next_position >= planner->ranked_count) {
                planner->result = V2S_FALLBACK_RESULT_EXHAUSTED;
            }
            break;
    }
    return V2S_FALLBACK_OK;
}
