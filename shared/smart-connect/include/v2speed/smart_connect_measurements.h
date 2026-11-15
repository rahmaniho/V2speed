#ifndef V2SPEED_SMART_CONNECT_MEASUREMENTS_H
#define V2SPEED_SMART_CONNECT_MEASUREMENTS_H

#include "v2speed/smart_connect.h"
#include "v2speed/smart_connect_probe_plan.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V2S_MAX_PROBE_OBSERVATIONS 64
#define V2S_PROBE_METRIC_MASK (V2S_METRIC_THROUGHPUT | V2S_METRIC_LATENCY)

typedef enum V2SMeasurementStatus {
    V2S_MEASUREMENT_OK = 0,
    V2S_MEASUREMENT_INVALID_ARGUMENT = 1,
    V2S_MEASUREMENT_INVALID_OBSERVATION = 2,
    V2S_MEASUREMENT_TOO_MANY_OBSERVATIONS = 3,
    V2S_MEASUREMENT_NO_FRESH_ATTEMPTS = 4,
    V2S_MEASUREMENT_INVALID_PROBE_STATE = 5
} V2SMeasurementStatus;

/* One caller-reported probe result. succeeded is the caller's probe outcome,
 * not independent tunnel verification. Timestamps and values use the caller's
 * monotonic timebase; this library cannot verify probe authenticity. */
typedef struct V2SProbeObservation {
    uint64_t timestamp_ms;
    uint8_t succeeded;
    uint32_t measured_mask;
    double throughput_mbps;
    double latency_ms;
} V2SProbeObservation;

/*
 * Aggregate at most 64 observations into a score-ready server measurement.
 * Stale observations are ignored; future or malformed observations reject the
 * entire batch. Latency/throughput use the median of fresh successful samples.
 * Reliability is successes / all fresh attempts; recent_success uses attempts
 * inside recent_window_ms. Missing metric samples remain absent, not zero.
 * With at least two fresh throughput samples at distinct timestamps, stability
 * is 1/(1+CV), where CV is population standard deviation / mean; all-zero
 * samples yield 1.0.
 * This is an initial, uncalibrated metric policy. Output is unchanged unless
 * V2S_MEASUREMENT_OK is returned.
 */
V2SMeasurementStatus v2s_measurement_aggregate(
    const char *server_id,
    uint8_t eligible,
    const V2SProbeObservation *observations,
    size_t observation_count,
    uint64_t now_ms,
    uint64_t max_age_ms,
    uint64_t recent_window_ms,
    V2SServerMeasurement *out_measurement);

/* Convert a terminal scheduler result into an observation at caller-supplied
 * monotonic timestamp. FAILED/TIMED_OUT produce a failed attempt with no metric
 * mask. SUCCEEDED requires a caller-supplied successful observation and copies
 * only its measurement data, replacing its timestamp. PENDING, IN_FLIGHT,
 * NOT_STARTED and CANCELLED do not become attempts. No metric or tunnel result
 * is independently verified. Output is unchanged on error. */
V2SMeasurementStatus v2s_measurement_observation_from_plan(
    const V2SProbePlan *plan,
    size_t candidate_index,
    uint64_t timestamp_ms,
    const V2SProbeObservation *successful_observation,
    V2SProbeObservation *out_observation);

#ifdef __cplusplus
}
#endif

#endif
