#ifndef V2SPEED_CONNECTION_STATE_H
#define V2SPEED_CONNECTION_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum V2SConnectionState {
    V2S_CONNECTION_DISCONNECTED = 0,
    V2S_CONNECTION_FETCHING = 1,
    V2S_CONNECTION_BENCHMARKING = 2,
    V2S_CONNECTION_CONNECTING = 3,
    V2S_CONNECTION_CONNECTED = 4,
    V2S_CONNECTION_RECONNECTING = 5,
    V2S_CONNECTION_ERROR = 6
} V2SConnectionState;

typedef enum V2SConnectionEvent {
    V2S_EVENT_FETCH_COMPLETED = 0,
    V2S_EVENT_BENCHMARK_COMPLETED = 1,
    V2S_EVENT_TUNNEL_ESTABLISHED = 2,
    V2S_EVENT_NETWORK_LOST = 3,
    V2S_EVENT_OPERATION_FAILED = 4,
    V2S_EVENT_USER_DISCONNECTED = 5
} V2SConnectionEvent;

typedef enum V2SConnectionStatus {
    V2S_CONNECTION_STATUS_OK = 0,
    V2S_CONNECTION_STATUS_INVALID_ARGUMENT = 1,
    V2S_CONNECTION_STATUS_BUSY = 2,
    V2S_CONNECTION_STATUS_STALE_GENERATION = 3,
    V2S_CONNECTION_STATUS_INVALID_TRANSITION = 4,
    V2S_CONNECTION_STATUS_GENERATION_EXHAUSTED = 5
} V2SConnectionStatus;

typedef struct V2SConnectionMachine {
    V2SConnectionState state;
    uint64_t generation;
} V2SConnectionMachine;

V2SConnectionStatus v2s_connection_init(V2SConnectionMachine *machine);
V2SConnectionStatus v2s_connection_begin_smart(
    V2SConnectionMachine *machine,
    uint64_t *out_generation);
V2SConnectionStatus v2s_connection_begin_reconnect(
    V2SConnectionMachine *machine,
    uint64_t *out_generation);
V2SConnectionStatus v2s_connection_apply(
    V2SConnectionMachine *machine,
    uint64_t generation,
    V2SConnectionEvent event);

#ifdef __cplusplus
}
#endif

#endif
