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
    }
    if (latency_count != 0) {
        result.measured_mask |= V2S_METRIC_LATENCY;
        result.latency_ms = median(latency, latency_count);
    }
    *out_measurement = result;
    return V2S_MEASUREMENT_OK;
}
