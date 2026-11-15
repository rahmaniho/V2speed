#ifndef V2SPEED_SMART_CONNECT_H
#define V2SPEED_SMART_CONNECT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V2S_MAX_CANDIDATES 256
#define V2S_SERVER_ID_CAPACITY 64
#define V2S_SMART_CONNECT_METRIC_COUNT 5

#define V2S_METRIC_THROUGHPUT (1u << 0)
#define V2S_METRIC_LATENCY (1u << 1)
#define V2S_METRIC_RELIABILITY (1u << 2)
#define V2S_METRIC_STABILITY (1u << 3)
#define V2S_METRIC_RECENT_SUCCESS (1u << 4)
#define V2S_METRIC_ALL ((1u << V2S_SMART_CONNECT_METRIC_COUNT) - 1u)

typedef enum V2SStatus {
    V2S_STATUS_OK = 0,
    V2S_STATUS_INVALID_ARGUMENT = 1,
    V2S_STATUS_INVALID_CANDIDATE = 2,
    V2S_STATUS_DUPLICATE_SERVER_ID = 3,
    V2S_STATUS_TOO_MANY_CANDIDATES = 4,
    V2S_STATUS_OUTPUT_TOO_SMALL = 5,
    V2S_STATUS_INVALID_WEIGHTS = 6
} V2SStatus;

typedef struct V2SServerMeasurement {
    char server_id[V2S_SERVER_ID_CAPACITY];
    uint8_t eligible;
    uint32_t measured_mask;
    double throughput_mbps;
    double latency_ms;
    double reliability;
    double stability;
    double recent_success;
} V2SServerMeasurement;

typedef struct V2SRankedCandidate {
    size_t input_index;
    uint32_t measured_mask;
    double normalized[V2S_SMART_CONNECT_METRIC_COUNT];
    double score;
    double evidence_coverage;
} V2SRankedCandidate;

/*
 * Rank healthy/authorized/unexpired candidates with at least one valid measurement.
 * Metrics are throughput Mbps, latency ms, and [0,1] ratios for the final three.
 * Missing metrics add no score or coverage; a single measured sample normalizes to 0.5.
 * Ties are ordered by evidence coverage descending, then server_id ascending.
 * Callers provide output storage sized for candidate_count entries.
 * On failure, output entries are left untouched and *out_count is zero.
 */
V2SStatus v2s_smart_connect_rank(
    const V2SServerMeasurement *candidates,
    size_t candidate_count,
    V2SRankedCandidate *ranked,
    size_t ranked_capacity,
    size_t *out_count);

/* Custom integer weights in metric order; must sum to 100. */
V2SStatus v2s_smart_connect_rank_with_weights(
    const V2SServerMeasurement *candidates,
    size_t candidate_count,
    const uint8_t custom_weights[V2S_SMART_CONNECT_METRIC_COUNT],
    V2SRankedCandidate *ranked,
    size_t ranked_capacity,
    size_t *out_count);

/* Fixed initial policy weights, in metric order: throughput, latency,
 * reliability, stability, recent success. The weights sum to 100. */
const uint8_t *v2s_smart_connect_weights(void);

#ifdef __cplusplus
}
#endif

#endif
