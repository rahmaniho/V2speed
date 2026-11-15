#include "v2speed/smart_connect.h"

#include <math.h>
#include <string.h>

static const uint8_t weights[V2S_SMART_CONNECT_METRIC_COUNT] = {30, 25, 20, 15, 10};

static double metric_value(const V2SServerMeasurement *candidate, size_t metric) {
    switch (metric) {
        case 0: return candidate->throughput_mbps;
        case 1: return candidate->latency_ms;
        case 2: return candidate->reliability;
        case 3: return candidate->stability;
        default: return candidate->recent_success;
    }
}

static int valid_candidate(const V2SServerMeasurement *candidate) {
    const double ratios[] = {
        candidate->reliability,
        candidate->stability,
        candidate->recent_success
    };
    size_t i;

    if (memchr(candidate->server_id, '\0', sizeof(candidate->server_id)) == NULL ||
        candidate->server_id[0] == '\0' || candidate->eligible > 1 ||
        (candidate->measured_mask & ~V2S_METRIC_ALL) != 0) {
        return 0;
    }
    if ((candidate->measured_mask & V2S_METRIC_THROUGHPUT) != 0 &&
        (!isfinite(candidate->throughput_mbps) || candidate->throughput_mbps < 0.0)) {
        return 0;
    }
    if ((candidate->measured_mask & V2S_METRIC_LATENCY) != 0 &&
        (!isfinite(candidate->latency_ms) || candidate->latency_ms < 0.0)) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        const uint32_t bit = (uint32_t)(1u << (i + 2));
        if ((candidate->measured_mask & bit) != 0 &&
            (!isfinite(ratios[i]) || ratios[i] < 0.0 || ratios[i] > 1.0)) {
            return 0;
        }
    }
    return 1;
}

static V2SStatus validate_batch(const V2SServerMeasurement *candidates, size_t count) {
    size_t i;
    size_t j;
    for (i = 0; i < count; i++) {
        if (!valid_candidate(&candidates[i])) return V2S_STATUS_INVALID_CANDIDATE;
        for (j = 0; j < i; j++) {
            if (strcmp(candidates[i].server_id, candidates[j].server_id) == 0) {
                return V2S_STATUS_DUPLICATE_SERVER_ID;
            }
        }
    }
    return V2S_STATUS_OK;
}

static int ranks_before(const V2SRankedCandidate *left,
                        const V2SRankedCandidate *right,
                        const V2SServerMeasurement *candidates) {
    if (left->score != right->score) return left->score > right->score;
    if (left->evidence_coverage != right->evidence_coverage) {
        return left->evidence_coverage > right->evidence_coverage;
    }
    return strcmp(candidates[left->input_index].server_id,
                  candidates[right->input_index].server_id) < 0;
}

V2SStatus v2s_smart_connect_rank_with_weights(
    const V2SServerMeasurement *candidates,
    size_t candidate_count,
    const uint8_t custom_weights[V2S_SMART_CONNECT_METRIC_COUNT],
    V2SRankedCandidate *ranked,
    size_t ranked_capacity,
    size_t *out_count) {
    V2SRankedCandidate work[V2S_MAX_CANDIDATES];
    size_t work_count = 0;
    size_t i;
    size_t j;
    size_t metric;
    size_t total_weight = 0;
    const uint8_t *weights = custom_weights;

    if (out_count == NULL) return V2S_STATUS_INVALID_ARGUMENT;
    *out_count = 0;
    if (custom_weights == NULL) return V2S_STATUS_INVALID_ARGUMENT;
    for (i = 0; i < V2S_SMART_CONNECT_METRIC_COUNT; i++) total_weight += custom_weights[i];
    if (total_weight != 100) return V2S_STATUS_INVALID_WEIGHTS;
    if (candidate_count == 0) return V2S_STATUS_OK;
    if (candidate_count > V2S_MAX_CANDIDATES) return V2S_STATUS_TOO_MANY_CANDIDATES;
    if (candidates == NULL || ranked == NULL) return V2S_STATUS_INVALID_ARGUMENT;
    if (ranked_capacity < candidate_count) return V2S_STATUS_OUTPUT_TOO_SMALL;

    /* Validate the entire batch before generating any output. */
    {
        const V2SStatus validation = validate_batch(candidates, candidate_count);
        if (validation != V2S_STATUS_OK) return validation;
    }

    for (i = 0; i < candidate_count; i++) {
        if (!candidates[i].eligible || candidates[i].measured_mask == 0) continue;
        work[work_count] = (V2SRankedCandidate){0};
        work[work_count].input_index = i;
        work[work_count].measured_mask = candidates[i].measured_mask;
        work_count++;
    }

    for (metric = 0; metric < V2S_SMART_CONNECT_METRIC_COUNT; metric++) {
        double minimum = 0.0;
        double maximum = 0.0;
        size_t observed = 0;
        const uint32_t bit = (uint32_t)(1u << metric);

        for (i = 0; i < work_count; i++) {
            const V2SServerMeasurement *candidate = &candidates[work[i].input_index];
            double value;
            if ((candidate->measured_mask & bit) == 0) continue;
            value = metric_value(candidate, metric);
            if (observed == 0) {
                minimum = value;
                maximum = value;
            } else {
                if (value < minimum) minimum = value;
                if (value > maximum) maximum = value;
            }
            observed++;
        }

        for (i = 0; i < work_count; i++) {
            const V2SServerMeasurement *candidate = &candidates[work[i].input_index];
            double normalized;
            if ((candidate->measured_mask & bit) == 0) continue;
            if (maximum == minimum) {
                /* A lone/equal sample has no comparative evidence: use neutral 0.5. */
                normalized = 0.5;
            } else {
                const double fraction =
                    (metric_value(candidate, metric) - minimum) / (maximum - minimum);
                normalized = metric == 1 ? 1.0 - fraction : fraction;
            }
            work[i].normalized[metric] = normalized;
            work[i].score += (double)weights[metric] * normalized;
            work[i].evidence_coverage += (double)weights[metric];
        }
    }

    /* Stable total ordering: score, evidence coverage, then server ID. */
    for (i = 1; i < work_count; i++) {
        const V2SRankedCandidate item = work[i];
        j = i;
        while (j > 0 && ranks_before(&item, &work[j - 1], candidates)) {
            work[j] = work[j - 1];
            j--;
        }
        work[j] = item;
    }

    if (work_count != 0) memcpy(ranked, work, work_count * sizeof(work[0]));
    *out_count = work_count;
    return V2S_STATUS_OK;
}

V2SStatus v2s_smart_connect_rank(
    const V2SServerMeasurement *candidates,
    size_t candidate_count,
    V2SRankedCandidate *ranked,
    size_t ranked_capacity,
    size_t *out_count) {
    return v2s_smart_connect_rank_with_weights(
        candidates, candidate_count, weights, ranked, ranked_capacity, out_count);
}

const uint8_t *v2s_smart_connect_weights(void) {
    return weights;
}
