#include "v2speed/connection_state.h"

#include <stdio.h>

static int failures = 0;
static const char *current_test = "";
static uint32_t property_seed = 0xA1B2C3D4u;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d [%s]: %s\n", __FILE__, __LINE__, current_test, #condition); \
        failures++; \
        return; \
    } \
} while (0)

static uint32_t next_property_value(void) {
    property_seed ^= property_seed << 13;
    property_seed ^= property_seed >> 17;
    property_seed ^= property_seed << 5;
    return property_seed;
}

static int valid_state(V2SConnectionState state) {
    return state >= V2S_CONNECTION_DISCONNECTED && state <= V2S_CONNECTION_ERROR;
}

static void test_successful_smart_connect_lifecycle(void) {
    current_test = "successful_smart_connect_lifecycle";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_DISCONNECTED);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_FETCHING);
    CHECK(generation == 1 && machine.generation == generation);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_BENCHMARKING);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_CONNECTING);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_CONNECTED);
}

static void test_duplicate_connect_is_rejected_without_new_generation(void) {
    current_test = "duplicate_connect_is_rejected_without_new_generation";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    uint64_t duplicate_generation = 99;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &duplicate_generation) == V2S_CONNECTION_STATUS_BUSY);
    CHECK(machine.generation == generation);
    CHECK(duplicate_generation == 99);
}

static void test_failed_attempt_can_be_retried_with_new_generation(void) {
    current_test = "failed_attempt_can_be_retried_with_new_generation";
    V2SConnectionMachine machine;
    uint64_t first = 0;
    uint64_t second = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &first) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, first, V2S_EVENT_OPERATION_FAILED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_ERROR);
    CHECK(v2s_connection_begin_smart(&machine, &second) == V2S_CONNECTION_STATUS_OK);
    CHECK(second == first + 1);
    CHECK(machine.state == V2S_CONNECTION_FETCHING);
}

static void test_user_disconnect_invalidates_stale_callbacks(void) {
    current_test = "user_disconnect_invalidates_stale_callbacks";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_USER_DISCONNECTED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_DISCONNECTED);
    CHECK(machine.generation == generation + 1);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_STALE_GENERATION);
    CHECK(machine.state == V2S_CONNECTION_DISCONNECTED);
}

static void test_network_loss_and_recovery(void) {
    current_test = "network_loss_and_recovery";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_NETWORK_LOST) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_RECONNECTING);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_CONNECTED);
}

static void test_explicit_reconnect_supersedes_old_generation(void) {
    current_test = "explicit_reconnect_supersedes_old_generation";
    V2SConnectionMachine machine;
    uint64_t initial = 0;
    uint64_t reconnect = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &initial) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, initial, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, initial, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, initial, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_reconnect(&machine, &reconnect) == V2S_CONNECTION_STATUS_OK);
    CHECK(reconnect == initial + 1);
    CHECK(machine.state == V2S_CONNECTION_RECONNECTING);
    CHECK(v2s_connection_apply(&machine, initial, V2S_EVENT_OPERATION_FAILED) == V2S_CONNECTION_STATUS_STALE_GENERATION);
    CHECK(v2s_connection_apply(&machine, reconnect, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_CONNECTED);
}

static void test_invalid_transition_does_not_mutate_state(void) {
    current_test = "invalid_transition_does_not_mutate_state";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(machine.state == V2S_CONNECTION_FETCHING);
}

static void test_connection_failure_at_connecting_is_observable(void) {
    current_test = "connection_failure_at_connecting_is_observable";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_OPERATION_FAILED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_ERROR);
}

static void test_reconnect_failure_leaves_error_state(void) {
    current_test = "reconnect_failure_leaves_error_state";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_NETWORK_LOST) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_OPERATION_FAILED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_ERROR);
}

static void test_invalid_arguments_and_generation_exhaustion(void) {
    current_test = "invalid_arguments_and_generation_exhaustion";
    V2SConnectionMachine machine;
    uint64_t generation = 71;
    CHECK(v2s_connection_init(NULL) == V2S_CONNECTION_STATUS_INVALID_ARGUMENT);
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(NULL, &generation) == V2S_CONNECTION_STATUS_INVALID_ARGUMENT);
    CHECK(v2s_connection_begin_smart(&machine, NULL) == V2S_CONNECTION_STATUS_INVALID_ARGUMENT);
    CHECK(generation == 71);
    CHECK(v2s_connection_begin_reconnect(NULL, &generation) == V2S_CONNECTION_STATUS_INVALID_ARGUMENT);
    CHECK(v2s_connection_begin_reconnect(&machine, NULL) == V2S_CONNECTION_STATUS_INVALID_ARGUMENT);
    CHECK(v2s_connection_begin_reconnect(&machine, &generation) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(v2s_connection_apply(NULL, 0, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_ARGUMENT);
    CHECK(v2s_connection_apply(&machine, 0, (V2SConnectionEvent)99) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    machine.generation = UINT64_MAX;
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_GENERATION_EXHAUSTED);
    CHECK(machine.state == V2S_CONNECTION_DISCONNECTED);
    CHECK(generation == 71);
}

static void test_invalid_events_are_rejected_in_each_state(void) {
    current_test = "invalid_events_are_rejected_in_each_state";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, 0, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_OPERATION_FAILED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_NETWORK_LOST) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    CHECK(v2s_connection_begin_reconnect(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
    machine.state = (V2SConnectionState)99;
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
}

static void test_reconnect_generation_exhaustion_and_disconnect_at_max(void) {
    current_test = "reconnect_generation_exhaustion_and_disconnect_at_max";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    machine.generation = UINT64_MAX;
    generation = 71;
    CHECK(v2s_connection_begin_reconnect(&machine, &generation) == V2S_CONNECTION_STATUS_GENERATION_EXHAUSTED);
    CHECK(machine.state == V2S_CONNECTION_CONNECTED);
    CHECK(generation == 71);
    CHECK(v2s_connection_apply(&machine, UINT64_MAX, V2S_EVENT_USER_DISCONNECTED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_DISCONNECTED);
    CHECK(machine.generation == UINT64_MAX);
    CHECK(v2s_connection_apply(&machine, UINT64_MAX, V2S_EVENT_NETWORK_LOST) == V2S_CONNECTION_STATUS_INVALID_TRANSITION);
}

static void test_disconnect_from_connected_and_idempotent_disconnect(void) {
    current_test = "disconnect_from_connected_and_idempotent_disconnect";
    V2SConnectionMachine machine;
    uint64_t generation = 0;
    uint64_t after_disconnect;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_begin_smart(&machine, &generation) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_FETCH_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_BENCHMARK_COMPLETED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_TUNNEL_ESTABLISHED) == V2S_CONNECTION_STATUS_OK);
    CHECK(v2s_connection_apply(&machine, generation, V2S_EVENT_USER_DISCONNECTED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.state == V2S_CONNECTION_DISCONNECTED);
    CHECK(machine.generation == generation + 1);
    after_disconnect = machine.generation;
    CHECK(v2s_connection_apply(&machine, after_disconnect, V2S_EVENT_USER_DISCONNECTED) == V2S_CONNECTION_STATUS_OK);
    CHECK(machine.generation == after_disconnect);
}

static void test_generated_event_sequences_preserve_state_invariants(void) {
    current_test = "generated_event_sequences_preserve_state_invariants";
    V2SConnectionMachine machine;
    size_t step;
    CHECK(v2s_connection_init(&machine) == V2S_CONNECTION_STATUS_OK);
    for (step = 0; step < 10000; step++) {
        const uint32_t random_value = next_property_value();
        const V2SConnectionState old_state = machine.state;
        const uint64_t old_generation = machine.generation;
        V2SConnectionStatus status;
        uint64_t output_generation = UINT64_C(0xBAD0BAD0);
        if ((random_value & 3u) == 0) {
            status = v2s_connection_begin_smart(&machine, &output_generation);
            if (status == V2S_CONNECTION_STATUS_OK) {
                CHECK(output_generation == machine.generation);
                CHECK(machine.generation == old_generation + 1);
                CHECK(machine.state == V2S_CONNECTION_FETCHING);
            } else {
                CHECK(output_generation == UINT64_C(0xBAD0BAD0));
            }
        } else if ((random_value & 3u) == 1) {
            status = v2s_connection_begin_reconnect(&machine, &output_generation);
            if (status == V2S_CONNECTION_STATUS_OK) {
                CHECK(output_generation == machine.generation);
                CHECK(machine.generation == old_generation + 1);
                CHECK(machine.state == V2S_CONNECTION_RECONNECTING);
            } else {
                CHECK(output_generation == UINT64_C(0xBAD0BAD0));
            }
        } else {
            const V2SConnectionEvent event = (V2SConnectionEvent)((random_value >> 8) % 6u);
            uint64_t event_generation = machine.generation;
            if (((random_value >> 4) % 3u) == 0 && event_generation != 0) event_generation--;
            status = v2s_connection_apply(&machine, event_generation, event);
        }
        CHECK(valid_state(machine.state));
        CHECK(machine.generation >= old_generation);
        if (status != V2S_CONNECTION_STATUS_OK) {
            CHECK(machine.state == old_state);
            CHECK(machine.generation == old_generation);
        }
    }
}

int main(void) {
    test_generated_event_sequences_preserve_state_invariants();
    test_successful_smart_connect_lifecycle();
    test_duplicate_connect_is_rejected_without_new_generation();
    test_failed_attempt_can_be_retried_with_new_generation();
    test_user_disconnect_invalidates_stale_callbacks();
    test_network_loss_and_recovery();
    test_explicit_reconnect_supersedes_old_generation();
    test_invalid_transition_does_not_mutate_state();
    test_connection_failure_at_connecting_is_observable();
    test_reconnect_failure_leaves_error_state();
    test_invalid_arguments_and_generation_exhaustion();
    test_invalid_events_are_rejected_in_each_state();
    test_reconnect_generation_exhaustion_and_disconnect_at_max();
    test_disconnect_from_connected_and_idempotent_disconnect();
    if (failures != 0) {
        fprintf(stderr, "%d connection-state test(s) failed.\n", failures);
        return 1;
    }
    puts("14 connection-state tests passed (including 10,000 generated events).");
    return 0;
}
