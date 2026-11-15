#include "v2speed/smart_connect_session.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static const char *current_test = "";

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d [%s]: %s\n", __FILE__, __LINE__, current_test, #condition); \
        failures++; \
        return; \
    } \
} while (0)

static V2SServerMeasurement profile(const char *id, double throughput, double latency,
                                    double reliability, uint8_t eligible) {
    V2SServerMeasurement result = {0};
    (void)snprintf(result.server_id, sizeof(result.server_id), "%s", id);
    result.eligible = eligible;
    result.measured_mask = V2S_METRIC_ALL;
    result.throughput_mbps = throughput;
    result.latency_ms = latency;
    result.reliability = reliability;
    result.stability = reliability;
    result.recent_success = reliability;
    return result;
}

static V2SSessionStatus reach_benchmarking(V2SSmartConnectSession *session, uint64_t *generation) {
    V2SSessionStatus status = v2s_smart_session_begin(session, generation);
    if (status != V2S_SESSION_OK) return status;
    return v2s_smart_session_fetch_completed(session, *generation);
}

static void test_rank_fallback_and_verified_connection_flow(void) {
    current_test = "rank_fallback_and_verified_connection_flow";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = {
        profile("best", 100, 10, 1, 1),
        profile("fallback", 50, 50, 0.5, 1),
        profile("unhealthy", 10000, 1, 1, 0)
    };
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 99;
    CHECK(v2s_smart_session_init(&session, 3) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(session.connection.state == V2S_CONNECTION_BENCHMARKING);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 3, NULL) == V2S_SESSION_OK);
    CHECK(session.connection.state == V2S_CONNECTION_CONNECTING);
    CHECK(session.ranked_count == 2);
    (void)strcpy(candidates[0].server_id, "caller-mutated");
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(strcmp(selected, "best") == 0);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_BUSY);
    CHECK(strcmp(selected, "best") == 0);
    CHECK(v2s_smart_session_complete_attempt(&session, generation, V2S_ATTEMPT_FAILED) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(strcmp(selected, "fallback") == 0);
    CHECK(v2s_smart_session_complete_attempt(&session, generation, V2S_ATTEMPT_TUNNEL_VERIFIED) == V2S_SESSION_OK);
    CHECK(session.connection.state == V2S_CONNECTION_CONNECTED);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_INVALID_STATE);
}

static void test_connect_state_is_not_fabricated_before_verified_success(void) {
    current_test = "connect_state_is_not_fabricated_before_verified_success";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = { profile("one", 10, 20, 1, 1) };
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 1, NULL) == V2S_SESSION_OK);
    CHECK(session.connection.state == V2S_CONNECTION_CONNECTING);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(session.connection.state == V2S_CONNECTION_CONNECTING);
}

static void test_empty_eligible_set_fails_observably(void) {
    current_test = "empty_eligible_set_fails_observably";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = { profile("down", 10, 20, 1, 0) };
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 1, NULL) == V2S_SESSION_NO_CANDIDATES);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
}

static void test_invalid_measurements_fail_session(void) {
    current_test = "invalid_measurements_fail_session";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = { profile("bad", 10, 20, 1.1, 1) };
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 1, NULL) == V2S_SESSION_INVALID_MEASUREMENTS);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
}

static void test_attempt_limit_exhaustion_transitions_to_error(void) {
    current_test = "attempt_limit_exhaustion_transitions_to_error";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = {
        profile("first", 100, 10, 1, 1),
        profile("second", 50, 50, 0.5, 1)
    };
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 1) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 2, NULL) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_complete_attempt(&session, generation, V2S_ATTEMPT_FAILED) == V2S_SESSION_EXHAUSTED);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
}

static void test_custom_weights_flow_to_selected_server(void) {
    current_test = "custom_weights_flow_to_selected_server";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = {
        profile("speed", 100, 20, 0, 1),
        profile("reliable", 50, 20, 1, 1)
    };
    const uint8_t speed_only[V2S_SMART_CONNECT_METRIC_COUNT] = {100, 0, 0, 0, 0};
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 2, speed_only) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(strcmp(selected, "speed") == 0);
}

static void test_stale_callbacks_and_disconnect_invalidate_attempts(void) {
    current_test = "stale_callbacks_and_disconnect_invalidate_attempts";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = { profile("one", 10, 20, 1, 1) };
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 1, NULL) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_disconnect(&session, generation) == V2S_SESSION_OK);
    CHECK(session.connection.state == V2S_CONNECTION_DISCONNECTED);
    CHECK(v2s_smart_session_complete_attempt(&session, generation, V2S_ATTEMPT_TUNNEL_VERIFIED) == V2S_SESSION_STALE_GENERATION);
}

static void test_cancel_attempt_transitions_to_disconnected(void) {
    current_test = "cancel_attempt_transitions_to_disconnected";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = { profile("one", 10, 20, 1, 1) };
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 1, NULL) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_complete_attempt(&session, generation, V2S_ATTEMPT_CANCELLED) == V2S_SESSION_OK);
    CHECK(session.connection.state == V2S_CONNECTION_DISCONNECTED);
    CHECK(session.connection.generation == generation + 1);
}

static void test_duplicate_start_and_invalid_phase_are_rejected(void) {
    current_test = "duplicate_start_and_invalid_phase_are_rejected";
    V2SSmartConnectSession session;
    V2SServerMeasurement candidates[] = { profile("one", 10, 20, 1, 1) };
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    uint64_t duplicate_generation = 77;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_begin(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_begin(&session, &duplicate_generation) == V2S_SESSION_BUSY);
    CHECK(duplicate_generation == 77);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, candidates, 1, NULL) == V2S_SESSION_INVALID_STATE);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_INVALID_STATE);
}

static void test_invalid_policy_and_null_arguments_are_rejected(void) {
    current_test = "invalid_policy_and_null_arguments_are_rejected";
    V2SSmartConnectSession session;
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(NULL, 1) == V2S_SESSION_INVALID_ARGUMENT);
    CHECK(v2s_smart_session_init(&session, 0) == V2S_SESSION_INVALID_ARGUMENT);
    CHECK(v2s_smart_session_init(&session, V2S_MAX_CONNECT_ATTEMPTS + 1) == V2S_SESSION_INVALID_ARGUMENT);
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_begin(&session, NULL) == V2S_SESSION_INVALID_ARGUMENT);
    CHECK(v2s_smart_session_begin(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_fetch_completed(&session, generation + 1) == V2S_SESSION_STALE_GENERATION);
    CHECK(v2s_smart_session_fetch_completed(&session, generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed(&session, generation, NULL, 1, NULL) == V2S_SESSION_INVALID_MEASUREMENTS);
}

static void test_observation_batches_aggregate_and_rank_only_fresh_servers(void) {
    current_test = "observation_batches_aggregate_before_rank_and_skip_stale_servers";
    V2SSmartConnectSession session;
    const V2SProbeObservation fast[] = {
        {990, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 100, 20},
        {995, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 120, 18}
    };
    const V2SProbeObservation slow[] = {
        {990, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 30, 80}
    };
    const V2SProbeObservation stale[] = {
        {100, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 10000, 1}
    };
    const V2SSessionProbeBatch batches[] = {
        {"fast", 1, fast, 2}, {"slow", 1, slow, 1}, {"stale", 1, stale, 1}
    };
    const uint8_t speed_only[V2S_SMART_CONNECT_METRIC_COUNT] = {100, 0, 0, 0, 0};
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, batches, 3, 1000, 100, 50, speed_only) ==
          V2S_SESSION_OK);
    CHECK(session.ranked_count == 2);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) == V2S_SESSION_OK);
    CHECK(strcmp(selected, "fast") == 0);
}

static void test_observation_batch_failure_does_not_partially_apply_ranked_ids(void) {
    current_test = "observation_batch_failure_does_not_partially_apply_ranked_ids";
    V2SSmartConnectSession session;
    const V2SProbeObservation valid[] = {
        {990, 1, V2S_METRIC_THROUGHPUT, 100, 0}
    };
    const V2SProbeObservation future[] = {
        {1001, 1, V2S_METRIC_THROUGHPUT, 1000, 0}
    };
    const V2SSessionProbeBatch batches[] = {
        {"valid", 1, valid, 1}, {"future", 1, future, 1}
    };
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, batches, 2, 1000, 100, 50, NULL) ==
          V2S_SESSION_INVALID_MEASUREMENTS);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
    CHECK(session.input_count == 0 && session.ranked_count == 0);
    CHECK(session.server_ids[0][0] == '\0');
}

static void test_observation_session_rejects_invalid_phase_and_stale_generation(void) {
    current_test = "observation_session_rejects_invalid_phase_and_stale_generation";
    V2SSmartConnectSession session;
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_begin(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, NULL, 0, 1000, 100, 50, NULL) ==
          V2S_SESSION_INVALID_STATE);
    CHECK(v2s_smart_session_fetch_completed(&session, generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation + 1, NULL, 1, 1000, 100, 50, NULL) ==
          V2S_SESSION_STALE_GENERATION);
    CHECK(session.connection.state == V2S_CONNECTION_BENCHMARKING);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, NULL, 0, 1000, 0, 0, NULL) ==
          V2S_SESSION_INVALID_MEASUREMENTS);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
}

static void test_all_stale_observation_batches_exhaust_without_fake_metrics(void) {
    current_test = "all_stale_observation_batches_exhaust_without_fake_metrics";
    V2SSmartConnectSession session;
    const V2SProbeObservation stale[] = {
        {100, 1, V2S_METRIC_THROUGHPUT, 9000, 0}
    };
    const V2SSessionProbeBatch batch = {"stale", 1, stale, 1};
    uint64_t generation = 0;
    CHECK(v2s_smart_session_init(&session, 1) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, &batch, 1, 1000, 100, 50, NULL) ==
          V2S_SESSION_NO_CANDIDATES);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
    CHECK(session.ranked_count == 0);
}

static void test_observation_batch_count_and_pointer_are_bounded(void) {
    current_test = "observation_batch_count_and_pointer_are_bounded";
    V2SSmartConnectSession session;
    V2SProbeObservation too_many[V2S_MAX_PROBE_OBSERVATIONS + 1];
    V2SSessionProbeBatch oversized = {"oversized", 1, too_many,
                                      V2S_MAX_PROBE_OBSERVATIONS + 1};
    const V2SProbeObservation fresh[] = {
        {990, 1, V2S_METRIC_THROUGHPUT, 10.0, 0.0}
    };
    const V2SSessionProbeBatch duplicate_ids[] = {
        {"duplicate", 1, fresh, 1}, {"duplicate", 1, fresh, 1}
    };
    size_t i;
    uint64_t generation = 0;
    for (i = 0; i < V2S_MAX_PROBE_OBSERVATIONS + 1; i++) {
        too_many[i] = (V2SProbeObservation){990, 1, V2S_METRIC_THROUGHPUT, 10.0, 0.0};
    }
    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, NULL, V2S_MAX_CANDIDATES + 1, 1000, 100, 50, NULL) ==
          V2S_SESSION_INVALID_MEASUREMENTS);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
    CHECK(session.input_count == 0 && session.ranked_count == 0);

    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, NULL, 1, 1000, 100, 50, NULL) ==
          V2S_SESSION_INVALID_MEASUREMENTS);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
    CHECK(session.input_count == 0 && session.ranked_count == 0);

    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, &oversized, 1, 1000, 100, 50, NULL) ==
          V2S_SESSION_INVALID_MEASUREMENTS);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
    CHECK(session.input_count == 0 && session.ranked_count == 0);

    CHECK(v2s_smart_session_init(&session, 2) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, duplicate_ids, 2, 1000, 100, 50, NULL) ==
          V2S_SESSION_INVALID_MEASUREMENTS);
    CHECK(session.connection.state == V2S_CONNECTION_ERROR);
    CHECK(session.input_count == 0 && session.ranked_count == 0);
}

static void test_generated_observation_batches_rank_the_fresh_best_server(void) {
    current_test = "generated_observation_batches_rank_the_fresh_best_server";
    size_t iteration;
    for (iteration = 0; iteration < 250; iteration++) {
        V2SSmartConnectSession session;
        V2SProbeObservation observations[5];
        V2SSessionProbeBatch batches[5];
        char server_ids[5][V2S_SERVER_ID_CAPACITY];
        char selected[V2S_SERVER_ID_CAPACITY];
        const size_t candidate_count = 2 + iteration % 4;
        uint64_t generation = 0;
        size_t i;
        for (i = 0; i < candidate_count; i++) {
            (void)snprintf(server_ids[i], sizeof(server_ids[i]), "generated-%zu-%zu",
                           iteration, i);
            observations[i] = (V2SProbeObservation){
                999, 1, V2S_METRIC_THROUGHPUT,
                (double)(iteration * 10 + i * 5 + 1), 0.0
            };
            batches[i] = (V2SSessionProbeBatch){
                server_ids[i], 1, &observations[i], 1
            };
        }
        CHECK(v2s_smart_session_init(&session, 1) == V2S_SESSION_OK);
        CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
        CHECK(v2s_smart_session_benchmark_completed_from_observations(
                  &session, generation, batches, candidate_count, 1000, 100, 50, NULL) ==
              V2S_SESSION_OK);
        CHECK(session.ranked_count == candidate_count);
        CHECK(v2s_smart_session_next_candidate(&session, generation, selected) ==
              V2S_SESSION_OK);
        CHECK(strcmp(selected, server_ids[candidate_count - 1]) == 0);
    }
}

static void test_maximum_candidate_batch_fits_session_aggregation_path(void) {
    current_test = "maximum_candidate_batch_fits_session_aggregation_path";
    V2SSmartConnectSession session;
    V2SSessionProbeBatch batches[V2S_MAX_CANDIDATES];
    V2SProbeObservation observations[V2S_MAX_CANDIDATES];
    char server_ids[V2S_MAX_CANDIDATES][V2S_SERVER_ID_CAPACITY];
    char selected[V2S_SERVER_ID_CAPACITY];
    uint64_t generation = 0;
    size_t i;
    for (i = 0; i < V2S_MAX_CANDIDATES; i++) {
        (void)snprintf(server_ids[i], sizeof(server_ids[i]), "node-%03zu", i);
        observations[i] = (V2SProbeObservation){
            49999, 1, V2S_METRIC_THROUGHPUT, 25.0, 0.0
        };
        batches[i] = (V2SSessionProbeBatch){server_ids[i], 1, &observations[i], 1};
    }
    CHECK(v2s_smart_session_init(&session, 1) == V2S_SESSION_OK);
    CHECK(reach_benchmarking(&session, &generation) == V2S_SESSION_OK);
    CHECK(v2s_smart_session_benchmark_completed_from_observations(
              &session, generation, batches, V2S_MAX_CANDIDATES,
              50000, 100, 50, NULL) == V2S_SESSION_OK);
    CHECK(session.ranked_count == V2S_MAX_CANDIDATES);
    CHECK(v2s_smart_session_next_candidate(&session, generation, selected) ==
          V2S_SESSION_OK);
    CHECK(strcmp(selected, "node-000") == 0);
}

int main(void) {
    test_rank_fallback_and_verified_connection_flow();
    test_connect_state_is_not_fabricated_before_verified_success();
    test_empty_eligible_set_fails_observably();
    test_invalid_measurements_fail_session();
    test_attempt_limit_exhaustion_transitions_to_error();
    test_custom_weights_flow_to_selected_server();
    test_stale_callbacks_and_disconnect_invalidate_attempts();
    test_cancel_attempt_transitions_to_disconnected();
    test_duplicate_start_and_invalid_phase_are_rejected();
    test_invalid_policy_and_null_arguments_are_rejected();
    test_observation_batches_aggregate_and_rank_only_fresh_servers();
    test_observation_batch_failure_does_not_partially_apply_ranked_ids();
    test_observation_session_rejects_invalid_phase_and_stale_generation();
    test_all_stale_observation_batches_exhaust_without_fake_metrics();
    test_observation_batch_count_and_pointer_are_bounded();
    test_generated_observation_batches_rank_the_fresh_best_server();
    test_maximum_candidate_batch_fits_session_aggregation_path();
    if (failures != 0) {
        fprintf(stderr, "%d Smart Connect session test(s) failed.\n", failures);
        return 1;
    }
    puts("17 Smart Connect session tests passed (including 250 generated observation batches and 256-candidate maximum).");
    return 0;
}
