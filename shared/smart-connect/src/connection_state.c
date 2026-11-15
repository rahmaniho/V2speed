#include "v2speed/connection_state.h"

#include <stddef.h>

V2SConnectionStatus v2s_connection_init(V2SConnectionMachine *machine) {
    if (machine == NULL) return V2S_CONNECTION_STATUS_INVALID_ARGUMENT;
    machine->state = V2S_CONNECTION_DISCONNECTED;
    machine->generation = 0;
    return V2S_CONNECTION_STATUS_OK;
}

V2SConnectionStatus v2s_connection_begin_smart(
    V2SConnectionMachine *machine,
    uint64_t *out_generation) {
    if (machine == NULL || out_generation == NULL) {
        return V2S_CONNECTION_STATUS_INVALID_ARGUMENT;
    }
    if (machine->state != V2S_CONNECTION_DISCONNECTED &&
        machine->state != V2S_CONNECTION_ERROR) {
        return V2S_CONNECTION_STATUS_BUSY;
    }
    if (machine->generation == UINT64_MAX) {
        return V2S_CONNECTION_STATUS_GENERATION_EXHAUSTED;
    }
    machine->generation++;
    machine->state = V2S_CONNECTION_FETCHING;
    *out_generation = machine->generation;
    return V2S_CONNECTION_STATUS_OK;
}

V2SConnectionStatus v2s_connection_begin_reconnect(
    V2SConnectionMachine *machine,
    uint64_t *out_generation) {
    if (machine == NULL || out_generation == NULL) {
        return V2S_CONNECTION_STATUS_INVALID_ARGUMENT;
    }
    if (machine->state != V2S_CONNECTION_CONNECTED) {
        return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
    }
    if (machine->generation == UINT64_MAX) {
        return V2S_CONNECTION_STATUS_GENERATION_EXHAUSTED;
    }
    machine->generation++;
    machine->state = V2S_CONNECTION_RECONNECTING;
    *out_generation = machine->generation;
    return V2S_CONNECTION_STATUS_OK;
}

V2SConnectionStatus v2s_connection_apply(
    V2SConnectionMachine *machine,
    uint64_t generation,
    V2SConnectionEvent event) {
    V2SConnectionState next_state;

    if (machine == NULL) return V2S_CONNECTION_STATUS_INVALID_ARGUMENT;
    if (generation != machine->generation) return V2S_CONNECTION_STATUS_STALE_GENERATION;
    if (event < V2S_EVENT_FETCH_COMPLETED || event > V2S_EVENT_USER_DISCONNECTED) {
        return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
    }

    if (event == V2S_EVENT_USER_DISCONNECTED) {
        if (machine->state == V2S_CONNECTION_DISCONNECTED) return V2S_CONNECTION_STATUS_OK;
        machine->state = V2S_CONNECTION_DISCONNECTED;
        /* At exhaustion, no callback can transition a disconnected machine. */
        if (machine->generation < UINT64_MAX) machine->generation++;
        return V2S_CONNECTION_STATUS_OK;
    }

    switch (machine->state) {
        case V2S_CONNECTION_FETCHING:
            if (event == V2S_EVENT_FETCH_COMPLETED) next_state = V2S_CONNECTION_BENCHMARKING;
            else if (event == V2S_EVENT_OPERATION_FAILED) next_state = V2S_CONNECTION_ERROR;
            else return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
            break;
        case V2S_CONNECTION_BENCHMARKING:
            if (event == V2S_EVENT_BENCHMARK_COMPLETED) next_state = V2S_CONNECTION_CONNECTING;
            else if (event == V2S_EVENT_OPERATION_FAILED) next_state = V2S_CONNECTION_ERROR;
            else return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
            break;
        case V2S_CONNECTION_CONNECTING:
            if (event == V2S_EVENT_TUNNEL_ESTABLISHED) next_state = V2S_CONNECTION_CONNECTED;
            else if (event == V2S_EVENT_OPERATION_FAILED) next_state = V2S_CONNECTION_ERROR;
            else return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
            break;
        case V2S_CONNECTION_CONNECTED:
            if (event == V2S_EVENT_NETWORK_LOST) next_state = V2S_CONNECTION_RECONNECTING;
            else return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
            break;
        case V2S_CONNECTION_RECONNECTING:
            if (event == V2S_EVENT_TUNNEL_ESTABLISHED) next_state = V2S_CONNECTION_CONNECTED;
            else if (event == V2S_EVENT_OPERATION_FAILED) next_state = V2S_CONNECTION_ERROR;
            else return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
            break;
        case V2S_CONNECTION_DISCONNECTED:
        case V2S_CONNECTION_ERROR:
        default:
            return V2S_CONNECTION_STATUS_INVALID_TRANSITION;
    }

    machine->state = next_state;
    return V2S_CONNECTION_STATUS_OK;
}
