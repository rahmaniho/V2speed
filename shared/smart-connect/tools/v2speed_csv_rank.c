#include "v2speed/smart_connect.h"
#include "v2speed/smart_connect_measurements.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSV_LINE_CAPACITY 512

typedef struct ServerRows {
    char id[V2S_SERVER_ID_CAPACITY];
    V2SProbeObservation observations[V2S_MAX_PROBE_OBSERVATIONS];
    size_t count;
} ServerRows;

static void usage(const char *program) {
    (void)fprintf(stderr,
                  "Usage: %s OBSERVATIONS.csv NOW_MS MAX_AGE_MS RECENT_WINDOW_MS\n"
                  "CSV header: server_id,timestamp_ms,success,measured_mask,throughput_mbps,latency_ms\n"
                  "Mask bits: 1=throughput, 2=latency, 3=both; failed rows use mask 0.\n",
                  program);
}

static int parse_u64(const char *text, uint64_t *value) {
    char *end = NULL;
    unsigned long long parsed;
    const char *digit;
    if (text == NULL || text[0] == '\0') return 0;
    for (digit = text; *digit != '\0'; digit++) {
        if (*digit < '0' || *digit > '9') return 0;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0') return 0;
    *value = (uint64_t)parsed;
    return 1;
}

static int parse_double_value(const char *text, double *value) {
    char *end = NULL;
    double parsed;
    if (text == NULL || text[0] == '\0') return 0;
    errno = 0;
    parsed = strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(parsed)) return 0;
    *value = parsed;
    return 1;
}

static int split_row(char *line, char *fields[], size_t expected) {
    size_t count = 0;
    char *cursor = line;
    while (count < expected) {
        char *comma;
        fields[count++] = cursor;
        comma = strchr(cursor, ',');
        if (count == expected) return comma == NULL;
        if (comma == NULL) return 0;
        *comma = '\0';
        cursor = comma + 1;
    }
    return 0;
}

static ServerRows *find_or_add_server(ServerRows servers[V2S_MAX_CANDIDATES],
                                      size_t *server_count,
                                      const char *id) {
    size_t i;
    size_t length = strlen(id);
    if (length == 0 || length >= V2S_SERVER_ID_CAPACITY) return NULL;
    for (i = 0; i < *server_count; i++) {
        if (strcmp(servers[i].id, id) == 0) return &servers[i];
    }
    if (*server_count == V2S_MAX_CANDIDATES) return NULL;
    (void)memcpy(servers[*server_count].id, id, length + 1);
    servers[*server_count].count = 0;
    return &servers[(*server_count)++];
}

static int read_csv(FILE *input,
                    ServerRows servers[V2S_MAX_CANDIDATES],
                    size_t *server_count) {
    char line[CSV_LINE_CAPACITY];
    size_t line_number = 0;
    int saw_header = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        char *fields[6];
        size_t length;
        uint64_t timestamp;
        uint64_t success;
        uint64_t mask;
        double throughput;
        double latency;
        ServerRows *server;
        line_number++;
        length = strlen(line);
        if (length != 0 && line[length - 1] != '\n' && !feof(input)) {
            (void)fprintf(stderr, "Line %zu exceeds %d bytes.\n", line_number,
                          CSV_LINE_CAPACITY - 1);
            return 0;
        }
        while (length != 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (length == 0 || line[0] == '#') continue;
        if (!saw_header) {
            if (strcmp(line,
                       "server_id,timestamp_ms,success,measured_mask,throughput_mbps,latency_ms") != 0) {
                (void)fprintf(stderr, "CSV header does not match the documented schema.\n");
                return 0;
            }
            saw_header = 1;
            continue;
        }
        if (!split_row(line, fields, 6) ||
            strchr(fields[0], '"') != NULL || fields[0][0] == '\0' ||
            strlen(fields[0]) >= V2S_SERVER_ID_CAPACITY ||
            !parse_u64(fields[1], &timestamp) ||
            !parse_u64(fields[2], &success) || success > 1 ||
            !parse_u64(fields[3], &mask) || mask > V2S_PROBE_METRIC_MASK ||
            !parse_double_value(fields[4], &throughput) ||
            !parse_double_value(fields[5], &latency)) {
            (void)fprintf(stderr, "Invalid CSV row at line %zu.\n", line_number);
            return 0;
        }
        server = find_or_add_server(servers, server_count, fields[0]);
        if (server == NULL) {
            (void)fprintf(stderr, "Invalid server ID or more than %d distinct servers.\n",
                          V2S_MAX_CANDIDATES);
            return 0;
        }
        if (server->count == V2S_MAX_PROBE_OBSERVATIONS) {
            (void)fprintf(stderr, "Server '%s' has more than %d observations.\n", fields[0],
                          V2S_MAX_PROBE_OBSERVATIONS);
            return 0;
        }
        server->observations[server->count++] = (V2SProbeObservation){
            timestamp, (uint8_t)success, (uint32_t)mask, throughput, latency
        };
    }
    if (ferror(input)) {
        (void)fprintf(stderr, "Failed while reading CSV input.\n");
        return 0;
    }
    if (!saw_header) {
        (void)fprintf(stderr, "CSV input is empty or has no header.\n");
        return 0;
    }
    if (*server_count == 0) {
        (void)fprintf(stderr, "CSV contains a header but no observations.\n");
        return 0;
    }
    return 1;
}

static void print_metric(uint32_t mask, uint32_t bit, double value) {
    if ((mask & bit) != 0) (void)printf("%.3f", value);
}

int main(int argc, char **argv) {
    ServerRows servers[V2S_MAX_CANDIDATES] = {0};
    V2SServerMeasurement measurements[V2S_MAX_CANDIDATES];
    V2SRankedCandidate ranked[V2S_MAX_CANDIDATES];
    size_t server_count = 0;
    size_t measurement_count = 0;
    size_t ranked_count = 0;
    size_t i;
    uint64_t now_ms;
    uint64_t max_age_ms;
    uint64_t recent_window_ms;
    FILE *input;
    if (argc != 5) {
        usage(argv[0]);
        return 2;
    }
    if (!parse_u64(argv[2], &now_ms) ||
        !parse_u64(argv[3], &max_age_ms) || max_age_ms == 0 ||
        !parse_u64(argv[4], &recent_window_ms) || recent_window_ms == 0 ||
        recent_window_ms > max_age_ms) {
        (void)fprintf(stderr, "NOW_MS must be a nonnegative integer; age windows must be positive and recent must not exceed max age.\n");
        return 2;
    }
    input = fopen(argv[1], "r");
    if (input == NULL) {
        (void)fprintf(stderr, "Cannot open '%s': %s\n", argv[1], strerror(errno));
        return 2;
    }
    if (!read_csv(input, servers, &server_count)) {
        (void)fclose(input);
        return 2;
    }
    (void)fclose(input);

    for (i = 0; i < server_count; i++) {
        const V2SMeasurementStatus status = v2s_measurement_aggregate(
            servers[i].id, 1, servers[i].observations, servers[i].count,
            now_ms, max_age_ms, recent_window_ms, &measurements[measurement_count]);
        if (status == V2S_MEASUREMENT_NO_FRESH_ATTEMPTS) continue;
        if (status != V2S_MEASUREMENT_OK) {
            (void)fprintf(stderr, "Invalid observations for server '%s' (status %d).\n",
                          servers[i].id, (int)status);
            return 2;
        }
        measurement_count++;
    }
    if (v2s_smart_connect_rank(measurements, measurement_count, ranked,
                               V2S_MAX_CANDIDATES, &ranked_count) != V2S_STATUS_OK) {
        (void)fprintf(stderr, "Could not rank the supplied measurements.\n");
        return 2;
    }
    (void)fprintf(stderr,
                  "Experimental CSV ranking only: timestamps are caller-provided; no probes or network checks were run.\n");
    if (ranked_count == 0) {
        (void)printf("No eligible server has fresh measured observations.\n");
        return 0;
    }
    (void)printf("rank,server_id,score,evidence_coverage,measured_mask,throughput_mbps,latency_ms,reliability,stability,recent_success\n");
    for (i = 0; i < ranked_count; i++) {
        const V2SServerMeasurement *measurement = &measurements[ranked[i].input_index];
        (void)printf("%zu,%s,%.3f,%.3f,%" PRIu32 ",", i + 1,
                     measurement->server_id, ranked[i].score,
                     ranked[i].evidence_coverage, measurement->measured_mask);
        print_metric(measurement->measured_mask, V2S_METRIC_THROUGHPUT,
                     measurement->throughput_mbps);
        (void)printf(",");
        print_metric(measurement->measured_mask, V2S_METRIC_LATENCY,
                     measurement->latency_ms);
        (void)printf(",");
        print_metric(measurement->measured_mask, V2S_METRIC_RELIABILITY,
                     measurement->reliability);
        (void)printf(",");
        print_metric(measurement->measured_mask, V2S_METRIC_STABILITY,
                     measurement->stability);
        (void)printf(",");
        print_metric(measurement->measured_mask, V2S_METRIC_RECENT_SUCCESS,
                     measurement->recent_success);
        (void)printf("\n");
    }
    return 0;
}
