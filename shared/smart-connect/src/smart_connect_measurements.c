#include "v2speed/smart_connect_measurements.h"

#include <math.h>
#include <string.h>

static int valid_server_id(const char *server_id, size_t *out_length) {
    size_t length;
    if (server_id == NULL) return 0;
    for (length = 0; length < V2S_SERVER_ID_CAPACITY; length++) {
        if (server_id[length] == '\0') {
            if (length == 0) return 0;
            *out_length = length;
            return 1;
        }
    }
    return 0;
}

static int valid_observation(const V2SProbeObservation *observation, uint64_t now_ms) {
    if (observation->succeeded > 1 ||
        (observation->measured_mask & ~V2S_PROBE_METRIC_MASK) != 0 ||
        observation->timestamp_ms > now_ms) {
        return 0;
    }
    if (!observation->succeeded && observation->measured_mask != 0) return 0;
    if ((observation->measured_mask & V2S_METRIC_THROUGHPUT) != 0 &&
        (!isfinite(observation->throughput_mbps) || observation->throughput_mbps < 0.0)) {
        return 0;
    }
    if ((observation->measured_mask & V2S_METRIC_LATENCY) != 0 &&
        (!isfinite(observation->latency_ms) || observation->latency_ms < 0.0)) {
        return 0;
    }
    return 1;
}

static void sort_samples(double values[V2S_MAX_PROBE_OBSERVATIONS], size_t count) {
    size_t i;
    for (i = 1; i < count; i++) {
        const double item = values[i];
        size_t position = i;
        while (position > 0 && item < values[position - 1]) {
            values[position] = values[position - 1];
            position--;
        }
        values[position] = item;
    }
}

static double median(double values[V2S_MAX_PROBE_OBSERVATIONS], size_t count) {
    sort_samples(values, count);
    if ((count & 1u) != 0) return values[count / 2];
    return values[count / 2 - 1] / 2.0 + values[count / 2] / 2.0;
}

static double throughput_stability(const double samples[V2S_MAX_PROBE_OBSERVATIONS], size_t count) {
    double maximum = 0.0;
    double mean = 0.0;
    double sum_squared_deviation = 0.0;
    size_t i;

    for (i = 0; i < count; i++) {
        if (samples[i] > maximum) maximum = samples[i];
    }
    if (maximum == 0.0) return 1.0;

    /* Normalize before Welford's variance calculation to avoid overflow for
     * very large but finite throughput values. */
    for (i = 0; i < count; i++) {
        const double scaled = samples[i] / maximum;
        const double delta = scaled - mean;
        mean += delta / (double)(i + 1);
        sum_squared_deviation += delta * (scaled - mean);
    }
    {
        const double variance = sum_squared_deviation / (double)count;
        const double coefficient_of_variation = sqrt(variance < 0.0 ? 0.0 : variance) / mean;
        return 1.0 / (1.0 + coefficient_of_variation);
    }
}

V2SMeasurementStatus v2s_measurement_observation_from_plan(
    const V2SProbePlan *plan,
    size_t candidate_index,
    uint64_t timestamp_ms,
    const V2SProbeObservation *successful_observation,
    V2SProbeObservation *out_observation) {
    V2SProbeTaskState task_state;
    V2SProbePlanStatus plan_status;
    V2SProbeObservation result = {0};

    if (plan == NULL || out_observation == NULL) return V2S_MEASUREMENT_INVALID_ARGUMENT;
    plan_status = v2s_probe_plan_task_state(plan, candidate_index, &task_state);
    if (plan_status == V2S_PROBE_PLAN_INVALID_ARGUMENT) return V2S_MEASUREMENT_INVALID_ARGUMENT;
    if (plan_status != V2S_PROBE_PLAN_OK) return V2S_MEASUREMENT_INVALID_PROBE_STATE;

    if (task_state == V2S_PROBE_TASK_SUCCEEDED) {
        if (successful_observation == NULL || successful_observation->succeeded != 1) {
            return V2S_MEASUREMENT_INVALID_ARGUMENT;
        }
        if (timestamp_ms < plan->tasks[candidate_index].started_at_ms) {
            return V2S_MEASUREMENT_INVALID_ARGUMENT;
        }
        result = *successful_observation;
        result.timestamp_ms = timestamp_ms;
        result.succeeded = 1;
        if (!valid_observation(&result, timestamp_ms)) {
            return V2S_MEASUREMENT_INVALID_OBSERVATION;
        }
    } else if (task_state == V2S_PROBE_TASK_FAILED ||
               task_state == V2S_PROBE_TASK_TIMED_OUT) {
        if (successful_observation != NULL ||
            timestamp_ms < plan->tasks[candidate_index].started_at_ms) {
            return V2S_MEASUREMENT_INVALID_ARGUMENT;
        }
        result.timestamp_ms = timestamp_ms;
        result.succeeded = 0;
        result.measured_mask = 0;
    } else {
        return V2S_MEASUREMENT_INVALID_PROBE_STATE;
    }

    *out_observation = result;
    return V2S_MEASUREMENT_OK;
}

V2SMeasurementStatus v2s_measurement_aggregate(
    const char *server_id,
    uint8_t eligible,
    const V2SProbeObservation *observations,
    size_t observation_count,
    uint64_t now_ms,
    uint64_t max_age_ms,
    uint64_t recent_window_ms,
    V2SServerMeasurement *out_measurement) {
    V2SServerMeasurement result = {0};
    double throughput[V2S_MAX_PROBE_OBSERVATIONS];
    double latency[V2S_MAX_PROBE_OBSERVATIONS];
    size_t server_id_length;
    size_t fresh_attempts = 0;
    size_t successful_attempts = 0;
    size_t recent_attempts = 0;
    size_t recent_successes = 0;
    size_t throughput_count = 0;
    size_t latency_count = 0;
    size_t i;
    uint64_t first_throughput_timestamp = 0;
    int throughput_timestamps_distinct = 0;
    uint64_t freshness_cutoff;
    uint64_t recent_cutoff;

    if (out_measurement == NULL || !valid_server_id(server_id, &server_id_length) ||
        eligible > 1 || max_age_ms == 0 || recent_window_ms == 0 ||
        recent_window_ms > max_age_ms) {
        return V2S_MEASUREMENT_INVALID_ARGUMENT;
    }
    if (observation_count > V2S_MAX_PROBE_OBSERVATIONS) {
        return V2S_MEASUREMENT_TOO_MANY_OBSERVATIONS;
    }
    if (observation_count != 0 && observations == NULL) {
        return V2S_MEASUREMENT_INVALID_ARGUMENT;
    }

    for (i = 0; i < observation_count; i++) {
        if (!valid_observation(&observations[i], now_ms)) {
            return V2S_MEASUREMENT_INVALID_OBSERVATION;
        }
    }

    freshness_cutoff = now_ms > max_age_ms ? now_ms - max_age_ms : 0;
    recent_cutoff = now_ms > recent_window_ms ? now_ms - recent_window_ms : 0;
    for (i = 0; i < observation_count; i++) {
        const V2SProbeObservation *observation = &observations[i];
        if (observation->timestamp_ms < freshness_cutoff) continue;

        fresh_attempts++;
        if (observation->succeeded) successful_attempts++;
        if (observation->timestamp_ms >= recent_cutoff) {
            recent_attempts++;
            if (observation->succeeded) recent_successes++;
        }
        if (!observation->succeeded) continue;
        if ((observation->measured_mask & V2S_METRIC_THROUGHPUT) != 0) {
            if (throughput_count == 0) first_throughput_timestamp = observation->timestamp_ms;
            else if (observation->timestamp_ms != first_throughput_timestamp) {
                throughput_timestamps_distinct = 1;
            }
            throughput[throughput_count++] = observation->throughput_mbps;
        }
        if ((observation->measured_mask & V2S_METRIC_LATENCY) != 0) {
            latency[latency_count++] = observation->latency_ms;
        }
    }
    if (fresh_attempts == 0) return V2S_MEASUREMENT_NO_FRESH_ATTEMPTS;

    (void)memcpy(result.server_id, server_id, server_id_length + 1);
    result.eligible = eligible;
    result.measured_mask = V2S_METRIC_RELIABILITY;
    result.reliability = (double)successful_attempts / (double)fresh_attempts;
    if (recent_attempts != 0) {
        result.measured_mask |= V2S_METRIC_RECENT_SUCCESS;
        result.recent_success = (double)recent_successes / (double)recent_attempts;
    }
    if (throughput_count != 0) {
        result.measured_mask |= V2S_METRIC_THROUGHPUT;
        result.throughput_mbps = median(throughput, throughput_count);
        if (throughput_count >= 2 && throughput_timestamps_distinct) {
            result.measured_mask |= V2S_METRIC_STABILITY;
            result.stability = throughput_stability(throughput, throughput_count);
        }
    }
    if (latency_count != 0) {
        result.measured_mask |= V2S_METRIC_LATENCY;
        result.latency_ms = median(latency, latency_count);
    }
    *out_measurement = result;
    return V2S_MEASUREMENT_OK;
}
