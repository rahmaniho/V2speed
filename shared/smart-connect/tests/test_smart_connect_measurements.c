#include "v2speed/smart_connect_measurements.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static const char *current_test = "";
static uint32_t random_state = UINT32_C(0x8BADF00D);

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d [%s]: %s\n", __FILE__, __LINE__, current_test, #condition); \
        failures++; \
        return; \
    } \
} while (0)

static V2SProbeObservation sample(uint64_t at, uint8_t succeeded, uint32_t measured,
                                  double throughput, double latency) {
    V2SProbeObservation result = {0};
    result.timestamp_ms = at;
    result.succeeded = succeeded;
    result.measured_mask = measured;
    result.throughput_mbps = throughput;
    result.latency_ms = latency;
    return result;
}

static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void test_medians_and_rates_aggregate_only_observed_metrics(void) {
    current_test = "medians_and_rates_aggregate_only_observed_metrics";
    const V2SProbeObservation observations[] = {
        sample(900, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 20, 40),
        sample(920, 0, 0, 0, 0),
        sample(950, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 80, 20),
        sample(980, 1, V2S_METRIC_THROUGHPUT, 40, 0)
    };
    V2SServerMeasurement result;
    CHECK(v2s_measurement_aggregate("median", 1, observations, 4, 1000, 200, 80, &result) == V2S_MEASUREMENT_OK);
    CHECK(strcmp(result.server_id, "median") == 0);
    CHECK(result.eligible == 1);
    CHECK(result.measured_mask == (V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY |
                                   V2S_METRIC_RELIABILITY | V2S_METRIC_RECENT_SUCCESS));
    CHECK(fabs(result.throughput_mbps - 40.0) < 1e-12);
    CHECK(fabs(result.latency_ms - 30.0) < 1e-12);
    CHECK(fabs(result.reliability - 0.75) < 1e-12);
    CHECK(fabs(result.recent_success - (2.0 / 3.0)) < 1e-12);
    CHECK(result.stability == 0.0);
}

static void test_stale_samples_do_not_contaminate_fresh_metrics(void) {
    current_test = "stale_samples_do_not_contaminate_fresh_metrics";
    const V2SProbeObservation observations[] = {
        sample(100, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 999, 1),
        sample(950, 1, V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY, 50, 25),
        sample(970, 0, 0, 0, 0)
    };
    V2SServerMeasurement result;
    CHECK(v2s_measurement_aggregate("fresh", 1, observations, 3, 1000, 100, 50, &result) == V2S_MEASUREMENT_OK);
    CHECK(result.throughput_mbps == 50.0);
    CHECK(result.latency_ms == 25.0);
    CHECK(result.reliability == 0.5);
    CHECK(result.recent_success == 0.5);
}

static void test_all_failed_attempts_are_evidence_not_fake_speed(void) {
    current_test = "all_failed_attempts_are_evidence_not_fake_speed";
    const V2SProbeObservation observations[] = {
        sample(950, 0, 0, 0, 0), sample(970, 0, 0, 0, 0)
    };
    V2SServerMeasurement result;
    CHECK(v2s_measurement_aggregate("offline", 1, observations, 2, 1000, 100, 80, &result) == V2S_MEASUREMENT_OK);
    CHECK(result.measured_mask == (V2S_METRIC_RELIABILITY | V2S_METRIC_RECENT_SUCCESS));
    CHECK(result.reliability == 0.0);
    CHECK(result.recent_success == 0.0);
    CHECK(result.throughput_mbps == 0.0);
    CHECK(result.latency_ms == 0.0);
}

static void test_success_without_optional_metrics_does_not_invent_zeroes(void) {
    current_test = "success_without_optional_metrics_does_not_invent_zeroes";
    const V2SProbeObservation observations[] = { sample(100, 1, 0, 0, 0) };
    V2SServerMeasurement result;
    CHECK(v2s_measurement_aggregate("reachable", 0, observations, 1, 100, 100, 50, &result) == V2S_MEASUREMENT_OK);
    CHECK(result.eligible == 0);
    CHECK(result.measured_mask == (V2S_METRIC_RELIABILITY | V2S_METRIC_RECENT_SUCCESS));
    CHECK(result.reliability == 1.0);
    CHECK(result.recent_success == 1.0);
    CHECK(result.throughput_mbps == 0.0);
    CHECK(result.latency_ms == 0.0);
}

static void test_future_or_malformed_sample_rejects_batch_transactionally(void) {
    current_test = "future_or_malformed_sample_rejects_batch_transactionally";
    V2SProbeObservation future[] = { sample(101, 1, V2S_METRIC_LATENCY, 0, 2) };
    V2SProbeObservation failed_with_metric[] = { sample(99, 0, V2S_METRIC_LATENCY, 0, 2) };
    V2SServerMeasurement output;
    V2SServerMeasurement before;
    (void)memset(&output, 0xA5, sizeof(output));
    before = output;
    CHECK(v2s_measurement_aggregate("bad", 1, future, 1, 100, 100, 50, &output) == V2S_MEASUREMENT_INVALID_OBSERVATION);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    CHECK(v2s_measurement_aggregate("bad", 1, failed_with_metric, 1, 100, 100, 50, &output) == V2S_MEASUREMENT_INVALID_OBSERVATION);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
}

static void test_empty_fresh_window_and_invalid_policy_are_explicit(void) {
    current_test = "empty_fresh_window_and_invalid_policy_are_explicit";
    const V2SProbeObservation stale[] = { sample(1, 1, V2S_METRIC_LATENCY, 0, 5) };
    const V2SProbeObservation fresh[] = { sample(99, 1, V2S_METRIC_LATENCY, 0, 5) };
    V2SServerMeasurement output;
    V2SServerMeasurement before;
    (void)memset(&output, 0x5A, sizeof(output));
    before = output;
    CHECK(v2s_measurement_aggregate("old", 1, stale, 1, 100, 10, 5, &output) == V2S_MEASUREMENT_NO_FRESH_ATTEMPTS);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    CHECK(v2s_measurement_aggregate("bad-policy", 1, fresh, 1, 100, 20, 21, &output) == V2S_MEASUREMENT_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    CHECK(v2s_measurement_aggregate("bad-policy", 1, fresh, 1, 100, 0, 0, &output) == V2S_MEASUREMENT_INVALID_ARGUMENT);
}

static void test_invalid_ids_masks_and_batch_size_are_rejected(void) {
    current_test = "invalid_ids_masks_and_batch_size_are_rejected";
    V2SProbeObservation malformed[] = { sample(99, 1, V2S_METRIC_STABILITY, 0, 0) };
    V2SProbeObservation invalid_success[] = { sample(99, 2, 0, 0, 0) };
    V2SProbeObservation invalid_numeric[] = { sample(99, 1, V2S_METRIC_THROUGHPUT, NAN, 0) };
    V2SProbeObservation too_many[V2S_MAX_PROBE_OBSERVATIONS + 1];
    V2SProbeObservation valid[] = { sample(99, 1, V2S_METRIC_LATENCY, 0, 5) };
    V2SServerMeasurement output;
    char unterminated[V2S_SERVER_ID_CAPACITY];
    (void)memset(unterminated, 'x', sizeof(unterminated));
    CHECK(v2s_measurement_aggregate("", 1, valid, 1, 100, 20, 10, &output) == V2S_MEASUREMENT_INVALID_ARGUMENT);
    CHECK(v2s_measurement_aggregate(unterminated, 1, valid, 1, 100, 20, 10, &output) == V2S_MEASUREMENT_INVALID_ARGUMENT);
    CHECK(v2s_measurement_aggregate("bad-mask", 1, malformed, 1, 100, 20, 10, &output) == V2S_MEASUREMENT_INVALID_OBSERVATION);
    CHECK(v2s_measurement_aggregate("bad-success", 1, invalid_success, 1, 100, 20, 10, &output) == V2S_MEASUREMENT_INVALID_OBSERVATION);
    CHECK(v2s_measurement_aggregate("bad-number", 1, invalid_numeric, 1, 100, 20, 10, &output) == V2S_MEASUREMENT_INVALID_OBSERVATION);
    CHECK(v2s_measurement_aggregate("large", 1, too_many, V2S_MAX_PROBE_OBSERVATIONS + 1, 100, 20, 10, &output) == V2S_MEASUREMENT_TOO_MANY_OBSERVATIONS);
    CHECK(v2s_measurement_aggregate("null", 1, NULL, 1, 100, 20, 10, &output) == V2S_MEASUREMENT_INVALID_ARGUMENT);
    CHECK(v2s_measurement_aggregate("null-output", 1, valid, 1, 100, 20, 10, NULL) == V2S_MEASUREMENT_INVALID_ARGUMENT);
    CHECK(v2s_measurement_aggregate("bad-eligibility", 2, valid, 1, 100, 20, 10, &output) == V2S_MEASUREMENT_INVALID_ARGUMENT);
}

static void test_generated_observation_windows_preserve_ratio_and_mask_invariants(void) {
    current_test = "generated_observation_windows_preserve_ratio_and_mask_invariants";
    uint32_t iteration;
    for (iteration = 0; iteration < 500; iteration++) {
        V2SProbeObservation observations[V2S_MAX_PROBE_OBSERVATIONS];
        V2SServerMeasurement result;
        const uint64_t now_ms = UINT64_C(50000);
        const size_t count = 1 + (size_t)(next_random() % V2S_MAX_PROBE_OBSERVATIONS);
        size_t i;
        size_t fresh_count = 0;
        size_t fresh_successes = 0;
        size_t recent_count = 0;
        size_t recent_successes = 0;
        size_t throughput_count = 0;
        size_t latency_count = 0;
        uint8_t eligible = (uint8_t)(next_random() & 1u);

        observations[0] = sample(now_ms - (next_random() % 3000u), 1,
                                 V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY,
                                 (double)(next_random() % 10000u) / 10.0,
                                 (double)(next_random() % 20000u) / 10.0);
        for (i = 1; i < count; i++) {
            const uint64_t age = next_random() % 30000u;
            const uint8_t succeeded = (uint8_t)(next_random() & 1u);
            const uint32_t measured = succeeded
                ? next_random() & V2S_PROBE_METRIC_MASK
                : 0;
            observations[i] = sample(now_ms - age, succeeded, measured,
                                    (double)(next_random() % 10000u) / 10.0,
                                    (double)(next_random() % 20000u) / 10.0);
        }
        CHECK(v2s_measurement_aggregate("generated", eligible, observations, count,
                                        now_ms, 20000, 5000, &result) == V2S_MEASUREMENT_OK);
        for (i = 0; i < count; i++) {
            if (observations[i].timestamp_ms < now_ms - 20000) continue;
            fresh_count++;
            if (observations[i].succeeded) {
                fresh_successes++;
                if ((observations[i].measured_mask & V2S_METRIC_THROUGHPUT) != 0) throughput_count++;
                if ((observations[i].measured_mask & V2S_METRIC_LATENCY) != 0) latency_count++;
            }
            if (observations[i].timestamp_ms >= now_ms - 5000) {
                recent_count++;
                if (observations[i].succeeded) recent_successes++;
            }
        }
        CHECK(fresh_count > 0);
        CHECK(result.eligible == eligible);
        CHECK((result.measured_mask & V2S_METRIC_RELIABILITY) != 0);
        CHECK(fabs(result.reliability - (double)fresh_successes / (double)fresh_count) < 1e-12);
        CHECK(recent_count > 0);
        CHECK((result.measured_mask & V2S_METRIC_RECENT_SUCCESS) != 0);
        CHECK(fabs(result.recent_success - (double)recent_successes / (double)recent_count) < 1e-12);
        CHECK(((result.measured_mask & V2S_METRIC_THROUGHPUT) != 0) == (throughput_count != 0));
        CHECK(((result.measured_mask & V2S_METRIC_LATENCY) != 0) == (latency_count != 0));
        CHECK((result.measured_mask & V2S_METRIC_STABILITY) == 0);
    }
}

int main(void) {
    test_medians_and_rates_aggregate_only_observed_metrics();
    test_stale_samples_do_not_contaminate_fresh_metrics();
    test_all_failed_attempts_are_evidence_not_fake_speed();
    test_success_without_optional_metrics_does_not_invent_zeroes();
    test_future_or_malformed_sample_rejects_batch_transactionally();
    test_empty_fresh_window_and_invalid_policy_are_explicit();
    test_invalid_ids_masks_and_batch_size_are_rejected();
    test_generated_observation_windows_preserve_ratio_and_mask_invariants();
    if (failures != 0) {
        fprintf(stderr, "%d Smart Connect measurement test(s) failed.\n", failures);
        return 1;
    }
    puts("8 Smart Connect measurement tests passed (including 500 generated windows).");
    return 0;
}
