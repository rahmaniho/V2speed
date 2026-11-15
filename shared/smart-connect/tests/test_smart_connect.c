#include "v2speed/smart_connect.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static const char *current_test = "";
static uint32_t property_seed = 0x91A2B3C4u;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d [%s]: %s\n", __FILE__, __LINE__, current_test, #condition); \
        failures++; \
        return; \
    } \
} while (0)

static V2SServerMeasurement candidate(const char *id, uint32_t mask,
                                      double throughput, double latency,
                                      double reliability, double stability,
                                      double recent_success) {
    V2SServerMeasurement result = {0};
    (void)snprintf(result.server_id, sizeof(result.server_id), "%s", id);
    result.eligible = 1;
    result.measured_mask = mask;
    result.throughput_mbps = throughput;
    result.latency_ms = latency;
    result.reliability = reliability;
    result.stability = stability;
    result.recent_success = recent_success;
    return result;
}

static uint32_t next_property_value(void) {
    property_seed ^= property_seed << 13;
    property_seed ^= property_seed >> 17;
    property_seed ^= property_seed << 5;
    return property_seed;
}

static void test_deterministic_generated_batches_preserve_ranking_invariants(void) {
    current_test = "deterministic_generated_batches_preserve_ranking_invariants";
    size_t batch;
    for (batch = 0; batch < 1000; batch++) {
        V2SServerMeasurement input[16];
        V2SRankedCandidate output[16];
        size_t candidate_count = (size_t)(next_property_value() % 16u) + 1;
        size_t expected_count = 0;
        size_t count = 0;
        size_t i;
        for (i = 0; i < candidate_count; i++) {
            const uint32_t mask = next_property_value() & V2S_METRIC_ALL;
            char id[V2S_SERVER_ID_CAPACITY];
            double throughput = (double)(next_property_value() % 1000000u) / 1000.0;
            double latency = (double)(next_property_value() % 500000u) / 1000.0;
            double reliability = (double)(next_property_value() % 1000001u) / 1000000.0;
            double stability = (double)(next_property_value() % 1000001u) / 1000000.0;
            double recent = (double)(next_property_value() % 1000001u) / 1000000.0;
            const uint8_t eligible = (uint8_t)(next_property_value() & 1u);
            (void)snprintf(id, sizeof(id), "generated-%03zu", i);
            input[i] = candidate(id, mask, throughput, latency, reliability, stability, recent);
            input[i].eligible = eligible;
            if ((mask & V2S_METRIC_THROUGHPUT) == 0) input[i].throughput_mbps = NAN;
            if ((mask & V2S_METRIC_LATENCY) == 0) input[i].latency_ms = NAN;
            if ((mask & V2S_METRIC_RELIABILITY) == 0) input[i].reliability = NAN;
            if ((mask & V2S_METRIC_STABILITY) == 0) input[i].stability = NAN;
            if ((mask & V2S_METRIC_RECENT_SUCCESS) == 0) input[i].recent_success = NAN;
            if (eligible && mask != 0) expected_count++;
        }
        CHECK(v2s_smart_connect_rank(input, candidate_count, output, candidate_count, &count) == V2S_STATUS_OK);
        CHECK(count == expected_count);
        for (i = 0; i < count; i++) {
            size_t metric;
            CHECK(output[i].score >= 0.0 && output[i].score <= 100.0);
            CHECK(output[i].evidence_coverage >= 0.0 && output[i].evidence_coverage <= 100.0);
            CHECK(output[i].score <= output[i].evidence_coverage + 1e-9);
            CHECK(output[i].measured_mask == input[output[i].input_index].measured_mask);
            for (metric = 0; metric < V2S_SMART_CONNECT_METRIC_COUNT; metric++) {
                if ((output[i].measured_mask & (1u << metric)) != 0) {
                    CHECK(output[i].normalized[metric] >= 0.0 && output[i].normalized[metric] <= 1.0);
                }
            }
            if (i != 0) {
                const V2SRankedCandidate *previous = &output[i - 1];
                const V2SRankedCandidate *current = &output[i];
                CHECK(previous->score >= current->score);
                if (previous->score == current->score) {
                    CHECK(previous->evidence_coverage >= current->evidence_coverage);
                    if (previous->evidence_coverage == current->evidence_coverage) {
                        CHECK(strcmp(input[previous->input_index].server_id,
                                     input[current->input_index].server_id) <= 0);
                    }
                }
            }
        }
    }
}

static void test_complete_measurements_rank_best_candidate_first(void) {
    current_test = "complete_measurements_rank_best_candidate_first";
    V2SServerMeasurement input[] = {
        candidate("alpha", V2S_METRIC_ALL, 100, 10, 1, 1, 1),
        candidate("beta", V2S_METRIC_ALL, 50, 50, 0, 0, 0)
    };
    V2SRankedCandidate output[2];
    size_t count = 99;
    CHECK(v2s_smart_connect_rank(input, 2, output, 2, &count) == V2S_STATUS_OK);
    CHECK(count == 2);
    CHECK(output[0].input_index == 0);
    CHECK(fabs(output[0].score - 100.0) < 1e-9);
    CHECK(fabs(output[1].score) < 1e-9);
    CHECK(fabs(output[0].evidence_coverage - 100.0) < 1e-9);
}

static void test_missing_measurements_reduce_score_and_coverage(void) {
    current_test = "missing_measurements_reduce_score_and_coverage";
    V2SServerMeasurement input[] = {
        candidate("partial", V2S_METRIC_THROUGHPUT, 100, 0, 0, 0, 0),
        candidate("complete-low", V2S_METRIC_ALL, 0, 100, 0, 0, 0),
        candidate("complete-high", V2S_METRIC_ALL, 50, 10, 1, 1, 1)
    };
    V2SRankedCandidate output[3];
    size_t count = 0;
    size_t partial_rank = 0;
    CHECK(v2s_smart_connect_rank(input, 3, output, 3, &count) == V2S_STATUS_OK);
    CHECK(count == 3);
    while (partial_rank < count && output[partial_rank].input_index != 0) partial_rank++;
    CHECK(partial_rank < count);
    CHECK(fabs(output[partial_rank].score - 30.0) < 1e-9);
    CHECK(fabs(output[partial_rank].evidence_coverage - 30.0) < 1e-9);
    CHECK(output[partial_rank].measured_mask == V2S_METRIC_THROUGHPUT);
    CHECK(output[partial_rank].score < 100.0);
}

static void test_lower_latency_ranks_higher(void) {
    current_test = "lower_latency_ranks_higher";
    V2SServerMeasurement input[] = {
        candidate("slow", V2S_METRIC_ALL, 10, 80, 0.5, 0.5, 0.5),
        candidate("fast", V2S_METRIC_ALL, 10, 20, 0.5, 0.5, 0.5)
    };
    V2SRankedCandidate output[2];
    size_t count = 0;
    CHECK(v2s_smart_connect_rank(input, 2, output, 2, &count) == V2S_STATUS_OK);
    CHECK(count == 2);
    CHECK(output[0].input_index == 1);
    CHECK(output[0].normalized[1] > output[1].normalized[1]);
}

static void test_single_sample_is_neutral_not_perfect(void) {
    current_test = "single_sample_is_neutral_not_perfect";
    V2SServerMeasurement input[] = {
        candidate("only", V2S_METRIC_THROUGHPUT, 900, 0, 0, 0, 0)
    };
    V2SRankedCandidate output[1];
    size_t count = 0;
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_OK);
    CHECK(count == 1);
    CHECK(fabs(output[0].score - 15.0) < 1e-9);
    CHECK(fabs(output[0].normalized[0] - 0.5) < 1e-9);
    CHECK(fabs(output[0].evidence_coverage - 30.0) < 1e-9);
}

static void test_unhealthy_candidates_are_excluded(void) {
    current_test = "unhealthy_candidates_are_excluded";
    V2SServerMeasurement input[] = {
        candidate("unhealthy", V2S_METRIC_ALL, 10000, 1, 1, 1, 1),
        candidate("healthy", V2S_METRIC_THROUGHPUT, 50, 0, 0, 0, 0)
    };
    input[0].eligible = 0;
    V2SRankedCandidate output[2];
    size_t count = 0;
    CHECK(v2s_smart_connect_rank(input, 2, output, 2, &count) == V2S_STATUS_OK);
    CHECK(count == 1);
    CHECK(output[0].input_index == 1);
}

static void test_no_measurements_yields_no_ranked_candidates(void) {
    current_test = "no_measurements_yields_no_ranked_candidates";
    V2SServerMeasurement input[] = { candidate("unknown", 0, 0, 0, 0, 0, 0) };
    V2SRankedCandidate output[1];
    size_t count = 5;
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_OK);
    CHECK(count == 0);
}

static void test_equal_measurements_use_stable_id_tiebreak(void) {
    current_test = "equal_measurements_use_stable_id_tiebreak";
    V2SServerMeasurement input[] = {
        candidate("node-b", V2S_METRIC_ALL, 10, 20, 0.5, 0.5, 0.5),
        candidate("node-a", V2S_METRIC_ALL, 10, 20, 0.5, 0.5, 0.5)
    };
    V2SRankedCandidate output[2];
    size_t count = 0;
    CHECK(v2s_smart_connect_rank(input, 2, output, 2, &count) == V2S_STATUS_OK);
    CHECK(count == 2);
    CHECK(output[0].input_index == 1);
}

static void test_more_evidence_wins_an_equal_score_tie(void) {
    current_test = "more_evidence_wins_an_equal_score_tie";
    V2SServerMeasurement input[] = {
        candidate("throughput-only", V2S_METRIC_THROUGHPUT, 100, 0, 0, 0, 0),
        candidate("multi-signal", V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 50, 64, 0, 0, 0),
        candidate("low", V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 0, 100, 0, 0, 0),
        candidate("latency-fast", V2S_METRIC_LATENCY, 0, 40, 0, 0, 0)
    };
    V2SRankedCandidate output[4];
    size_t count = 0;
    CHECK(v2s_smart_connect_rank(input, 4, output, 4, &count) == V2S_STATUS_OK);
    CHECK(count == 4);
    CHECK(output[0].input_index == 1);
    CHECK(fabs(output[0].score - 30.0) < 1e-9);
    CHECK(fabs(output[0].evidence_coverage - 55.0) < 1e-9);
    CHECK(output[1].input_index == 0);
    CHECK(fabs(output[1].score - 30.0) < 1e-9);
}

static void test_invalid_measurement_is_rejected_transactionally(void) {
    current_test = "invalid_measurement_is_rejected_transactionally";
    V2SServerMeasurement input[] = { candidate("bad", V2S_METRIC_RELIABILITY, NAN, 0, 1.1, 0, 0) };
    V2SRankedCandidate output[1];
    memset(output, 0xA5, sizeof(output));
    V2SRankedCandidate before = output[0];
    size_t count = 77;
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
    CHECK(count == 0);
    CHECK(memcmp(&before, output, sizeof(before)) == 0);
}

static void test_duplicate_ids_and_small_output_are_reported(void) {
    current_test = "duplicate_ids_and_small_output_are_reported";
    V2SServerMeasurement input[] = {
        candidate("same", V2S_METRIC_THROUGHPUT, 1, 0, 0, 0, 0),
        candidate("same", V2S_METRIC_THROUGHPUT, 2, 0, 0, 0, 0)
    };
    V2SRankedCandidate output[2];
    size_t count = 99;
    CHECK(v2s_smart_connect_rank(input, 2, output, 2, &count) == V2S_STATUS_DUPLICATE_SERVER_ID);
    CHECK(count == 0);
    input[1] = candidate("different", V2S_METRIC_THROUGHPUT, 2, 0, 0, 0, 0);
    CHECK(v2s_smart_connect_rank(input, 2, output, 1, &count) == V2S_STATUS_OUTPUT_TOO_SMALL);
    CHECK(count == 0);
}

static void test_policy_weights_sum_to_100(void) {
    current_test = "policy_weights_sum_to_100";
    const uint8_t *weights = v2s_smart_connect_weights();
    CHECK(weights != NULL);
    CHECK(weights[0] == 30 && weights[1] == 25 && weights[2] == 20);
    CHECK(weights[3] == 15 && weights[4] == 10);
    CHECK(weights[0] + weights[1] + weights[2] + weights[3] + weights[4] == 100);
}

static void test_custom_weights_are_applied_and_validated(void) {
    current_test = "custom_weights_are_applied_and_validated";
    V2SServerMeasurement input[] = {
        candidate("throughput", V2S_METRIC_THROUGHPUT | V2S_METRIC_RELIABILITY, 100, 0, 0, 0, 0),
        candidate("reliability", V2S_METRIC_THROUGHPUT | V2S_METRIC_RELIABILITY, 0, 0, 1, 0, 0)
    };
    const uint8_t throughput_only[V2S_SMART_CONNECT_METRIC_COUNT] = {100, 0, 0, 0, 0};
    const uint8_t invalid[V2S_SMART_CONNECT_METRIC_COUNT] = {99, 0, 0, 0, 0};
    V2SRankedCandidate output[2];
    size_t count = 0;
    CHECK(v2s_smart_connect_rank_with_weights(input, 2, throughput_only, output, 2, &count) == V2S_STATUS_OK);
    CHECK(count == 2);
    CHECK(output[0].input_index == 0);
    CHECK(fabs(output[0].score - 100.0) < 1e-9);
    CHECK(fabs(output[0].evidence_coverage - 100.0) < 1e-9);
    CHECK(v2s_smart_connect_rank_with_weights(input, 2, invalid, output, 2, &count) == V2S_STATUS_INVALID_WEIGHTS);
    CHECK(count == 0);
    CHECK(v2s_smart_connect_rank_with_weights(input, 2, NULL, output, 2, &count) == V2S_STATUS_INVALID_ARGUMENT);
}

static void test_invalid_arguments_and_candidate_limit(void) {
    current_test = "invalid_arguments_and_candidate_limit";
    V2SServerMeasurement input[] = { candidate("valid", V2S_METRIC_THROUGHPUT, 10, 0, 0, 0, 0) };
    V2SRankedCandidate output[1];
    size_t count = 8;
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, NULL) == V2S_STATUS_INVALID_ARGUMENT);
    CHECK(v2s_smart_connect_rank(NULL, 1, output, 1, &count) == V2S_STATUS_INVALID_ARGUMENT);
    CHECK(count == 0);
    count = 8;
    CHECK(v2s_smart_connect_rank(input, 1, NULL, 1, &count) == V2S_STATUS_INVALID_ARGUMENT);
    CHECK(count == 0);
    count = 8;
    CHECK(v2s_smart_connect_rank(input, V2S_MAX_CANDIDATES + 1u, output,
                                 V2S_MAX_CANDIDATES + 1u, &count) == V2S_STATUS_TOO_MANY_CANDIDATES);
    CHECK(count == 0);
    count = 8;
    CHECK(v2s_smart_connect_rank(NULL, 0, NULL, 0, &count) == V2S_STATUS_OK);
    CHECK(count == 0);
}

static void test_only_present_measurements_are_validated(void) {
    current_test = "only_present_measurements_are_validated";
    V2SServerMeasurement input[] = {
        candidate("missing", 0, NAN, NAN, NAN, NAN, NAN)
    };
    V2SRankedCandidate output[1];
    size_t count = 1;
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_OK);
    CHECK(count == 0);
}

static void test_oversized_ratio_and_negative_measurement_are_rejected(void) {
    current_test = "oversized_ratio_and_negative_measurement_are_rejected";
    V2SServerMeasurement input[] = {
        candidate("ratio", V2S_METRIC_RELIABILITY, 0, 0, 1.01, 0, 0)
    };
    V2SRankedCandidate output[1];
    size_t count = 0;
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
    input[0] = candidate("negative", V2S_METRIC_THROUGHPUT, -0.01, 0, 0, 0, 0);
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
    input[0] = candidate("latency", V2S_METRIC_LATENCY, 0, -0.01, 0, 0, 0);
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
}

static void test_invalid_ids_eligibility_and_masks_are_rejected(void) {
    current_test = "invalid_ids_eligibility_and_masks_are_rejected";
    V2SServerMeasurement input[] = { candidate("ok", 0, 0, 0, 0, 0, 0) };
    V2SRankedCandidate output[1];
    size_t count = 1;
    input[0].server_id[0] = '\0';
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
    input[0] = candidate("ok", 0, 0, 0, 0, 0, 0);
    memset(input[0].server_id, 'x', sizeof(input[0].server_id));
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
    input[0] = candidate("ok", 0, 0, 0, 0, 0, 0);
    input[0].eligible = 2;
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
    input[0] = candidate("ok", 1u << 12, 0, 0, 0, 0, 0);
    CHECK(v2s_smart_connect_rank(input, 1, output, 1, &count) == V2S_STATUS_INVALID_CANDIDATE);
}

int main(void) {
    test_deterministic_generated_batches_preserve_ranking_invariants();
    test_complete_measurements_rank_best_candidate_first();
    test_missing_measurements_reduce_score_and_coverage();
    test_lower_latency_ranks_higher();
    test_single_sample_is_neutral_not_perfect();
    test_unhealthy_candidates_are_excluded();
    test_no_measurements_yields_no_ranked_candidates();
    test_equal_measurements_use_stable_id_tiebreak();
    test_more_evidence_wins_an_equal_score_tie();
    test_invalid_measurement_is_rejected_transactionally();
    test_duplicate_ids_and_small_output_are_reported();
    test_policy_weights_sum_to_100();
    test_custom_weights_are_applied_and_validated();
    test_invalid_arguments_and_candidate_limit();
    test_only_present_measurements_are_validated();
    test_oversized_ratio_and_negative_measurement_are_rejected();
    test_invalid_ids_eligibility_and_masks_are_rejected();
    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed.\n", failures);
        return 1;
    }
    puts("17 Smart Connect scorer tests passed (including 1,000 generated batches).");
    return 0;
}
