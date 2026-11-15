#include "v2speed/smart_connect_probe_plan.h"

#include <stdio.h>

static int failures = 0;
static const char *current_test = "";

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d [%s]: %s\n", __FILE__, __LINE__, current_test, #condition); \
        failures++; \
        return; \
    } \
} while (0)

static void test_concurrency_limit_and_ordered_dispatch(void) {
    current_test = "concurrency_limit_and_ordered_dispatch";
    V2SProbePlan plan;
    size_t index = 99;
    V2SProbeTaskState state;
    CHECK(v2s_probe_plan_init(&plan, 3, 2, 50, 100, 500) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 100, &index) == V2S_PROBE_PLAN_OK && index == 0);
    CHECK(v2s_probe_plan_next(&plan, 100, &index) == V2S_PROBE_PLAN_OK && index == 1);
    index = 77;
    CHECK(v2s_probe_plan_next(&plan, 101, &index) == V2S_PROBE_PLAN_BUSY);
    CHECK(index == 77);
    CHECK(v2s_probe_plan_complete(&plan, 0, 120, 1) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 120, &index) == V2S_PROBE_PLAN_OK && index == 2);
    CHECK(v2s_probe_plan_complete(&plan, 1, 130, 0) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_complete(&plan, 2, 130, 1) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 130, &index) == V2S_PROBE_PLAN_COMPLETE);
    CHECK(v2s_probe_plan_task_state(&plan, 0, &state) == V2S_PROBE_PLAN_OK);
    CHECK(state == V2S_PROBE_TASK_SUCCEEDED);
    CHECK(v2s_probe_plan_task_state(&plan, 1, &state) == V2S_PROBE_PLAN_OK);
    CHECK(state == V2S_PROBE_TASK_FAILED);
}

static void test_per_probe_expiry_frees_capacity_and_late_completion_is_ignored(void) {
    current_test = "per_probe_expiry_frees_capacity_and_late_completion_is_ignored";
    V2SProbePlan plan;
    size_t expired[2] = {99, 99};
    size_t expired_count = 99;
    size_t index = 99;
    V2SProbeTaskState state;
    CHECK(v2s_probe_plan_init(&plan, 3, 2, 20, 0, 100) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 0, &index) == V2S_PROBE_PLAN_OK && index == 0);
    CHECK(v2s_probe_plan_next(&plan, 10, &index) == V2S_PROBE_PLAN_OK && index == 1);
    CHECK(v2s_probe_plan_expire(&plan, 20, expired, 2, &expired_count) == V2S_PROBE_PLAN_OK);
    CHECK(expired_count == 1 && expired[0] == 0);
    CHECK(v2s_probe_plan_complete(&plan, 0, 21, 1) == V2S_PROBE_PLAN_TIMED_OUT);
    CHECK(v2s_probe_plan_task_state(&plan, 0, &state) == V2S_PROBE_PLAN_OK);
    CHECK(state == V2S_PROBE_TASK_TIMED_OUT);
    CHECK(v2s_probe_plan_next(&plan, 20, &index) == V2S_PROBE_PLAN_OK && index == 2);
    CHECK(v2s_probe_plan_complete(&plan, 1, 29, 1) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_complete(&plan, 2, 40, 1) == V2S_PROBE_PLAN_TIMED_OUT);
}

static void test_global_deadline_marks_pending_not_started_and_active_timed_out(void) {
    current_test = "global_deadline_marks_pending_not_started_and_active_timed_out";
    V2SProbePlan plan;
    size_t expired[2] = {99, 99};
    size_t expired_count = 99;
    size_t index = 99;
    V2SProbeTaskState state;
    CHECK(v2s_probe_plan_init(&plan, 3, 2, 50, 0, 30) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_OK && index == 0);
    CHECK(v2s_probe_plan_next(&plan, 2, &index) == V2S_PROBE_PLAN_OK && index == 1);
    CHECK(v2s_probe_plan_expire(&plan, 30, expired, 2, &expired_count) == V2S_PROBE_PLAN_OK);
    CHECK(expired_count == 2 && expired[0] == 0 && expired[1] == 1);
    CHECK(v2s_probe_plan_task_state(&plan, 2, &state) == V2S_PROBE_PLAN_OK);
    CHECK(state == V2S_PROBE_TASK_NOT_STARTED);
    CHECK(v2s_probe_plan_next(&plan, 30, &index) == V2S_PROBE_PLAN_DEADLINE_REACHED);
}

static void test_deadline_and_timeout_boundaries_are_inclusive(void) {
    current_test = "deadline_and_timeout_boundaries_are_inclusive";
    V2SProbePlan plan;
    size_t index = 99;
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 10, 100, 200) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 100, &index) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_complete(&plan, index, 110, 1) == V2S_PROBE_PLAN_TIMED_OUT);
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 50, 100, 120) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 120, &index) == V2S_PROBE_PLAN_DEADLINE_REACHED);
}

static void test_expire_capacity_failure_preserves_state_and_outputs(void) {
    current_test = "expire_capacity_failure_preserves_state_and_outputs";
    V2SProbePlan plan;
    size_t expired = 66;
    size_t expired_count = 77;
    size_t index = 99;
    V2SProbeTaskState state;
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 10, 0, 100) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_expire(&plan, 11, &expired, 0, &expired_count) == V2S_PROBE_PLAN_OUTPUT_TOO_SMALL);
    CHECK(expired == 66 && expired_count == 77);
    CHECK(v2s_probe_plan_task_state(&plan, 0, &state) == V2S_PROBE_PLAN_OK);
    CHECK(state == V2S_PROBE_TASK_IN_FLIGHT);
    CHECK(v2s_probe_plan_expire(&plan, 11, &expired, 1, &expired_count) == V2S_PROBE_PLAN_OK);
    CHECK(expired == 0 && expired_count == 1);
}

static void test_cancel_reports_inflight_tasks_and_is_idempotent(void) {
    current_test = "cancel_reports_inflight_tasks_and_is_idempotent";
    V2SProbePlan plan;
    size_t active[2] = {99, 99};
    size_t active_count = 99;
    size_t index = 99;
    V2SProbeTaskState state;
    CHECK(v2s_probe_plan_init(&plan, 3, 2, 50, 0, 100) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_OK && index == 0);
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_OK && index == 1);
    CHECK(v2s_probe_plan_cancel(&plan, active, 2, &active_count) == V2S_PROBE_PLAN_OK);
    CHECK(active_count == 2 && active[0] == 0 && active[1] == 1);
    CHECK(v2s_probe_plan_task_state(&plan, 2, &state) == V2S_PROBE_PLAN_OK);
    CHECK(state == V2S_PROBE_TASK_CANCELLED);
    CHECK(v2s_probe_plan_next(&plan, 2, &index) == V2S_PROBE_PLAN_CANCELLED);
    active_count = 99;
    CHECK(v2s_probe_plan_cancel(&plan, NULL, 0, &active_count) == V2S_PROBE_PLAN_OK);
    CHECK(active_count == 0);
}

static void test_cancel_capacity_failure_is_transactional(void) {
    current_test = "cancel_capacity_failure_is_transactional";
    V2SProbePlan plan;
    size_t active = 66;
    size_t active_count = 77;
    size_t index = 99;
    V2SProbeTaskState state;
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 10, 0, 100) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_cancel(&plan, &active, 0, &active_count) == V2S_PROBE_PLAN_OUTPUT_TOO_SMALL);
    CHECK(active == 66 && active_count == 77);
    CHECK(v2s_probe_plan_task_state(&plan, 0, &state) == V2S_PROBE_PLAN_OK);
    CHECK(state == V2S_PROBE_TASK_IN_FLIGHT);
}

static void test_invalid_transitions_and_clock_order_are_observable(void) {
    current_test = "invalid_transitions_and_clock_order_are_observable";
    V2SProbePlan plan;
    size_t index = 99;
    size_t expired_index = 88;
    size_t expired_count = 77;
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 10, 10, 50) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 9, &index) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(index == 99);
    CHECK(v2s_probe_plan_complete(&plan, 0, 20, 1) == V2S_PROBE_PLAN_INVALID_STATE);
    CHECK(v2s_probe_plan_next(&plan, 10, &index) == V2S_PROBE_PLAN_OK);
    index = 77;
    CHECK(v2s_probe_plan_next(&plan, 9, &index) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(index == 77);
    CHECK(v2s_probe_plan_expire(&plan, 9, &expired_index, 1, &expired_count) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(expired_index == 88 && expired_count == 77);
    CHECK(v2s_probe_plan_complete(&plan, 2, 11, 1) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_complete(&plan, 0, 9, 1) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
}

static void test_exhausted_dispatch_waits_for_in_flight_work(void) {
    current_test = "exhausted_dispatch_waits_for_in_flight_work";
    V2SProbePlan plan;
    size_t index = 99;
    CHECK(v2s_probe_plan_init(&plan, 2, 2, 50, 0, 100) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_OK && index == 0);
    CHECK(v2s_probe_plan_next(&plan, 2, &index) == V2S_PROBE_PLAN_OK && index == 1);
    CHECK(v2s_probe_plan_next(&plan, 3, &index) == V2S_PROBE_PLAN_BUSY);
    CHECK(v2s_probe_plan_complete(&plan, 0, 4, 1) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 4, &index) == V2S_PROBE_PLAN_BUSY);
    CHECK(v2s_probe_plan_complete(&plan, 1, 5, 0) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 5, &index) == V2S_PROBE_PLAN_COMPLETE);
}

static void test_corrupt_plan_state_is_rejected_transactionally(void) {
    current_test = "corrupt_plan_state_is_rejected_transactionally";
    V2SProbePlan plan;
    size_t index = 77;
    size_t expired = 88;
    size_t expired_count = 99;
    size_t active = 66;
    size_t active_count = 55;
    V2SProbeTaskState state = V2S_PROBE_TASK_SUCCEEDED;
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 10, 0, 100) == V2S_PROBE_PLAN_OK);
    plan.tasks[0].state = (V2SProbeTaskState)99;
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_INVALID_STATE);
    CHECK(index == 77);
    CHECK(v2s_probe_plan_complete(&plan, 0, 1, 1) == V2S_PROBE_PLAN_INVALID_STATE);
    CHECK(v2s_probe_plan_expire(&plan, 1, &expired, 1, &expired_count) == V2S_PROBE_PLAN_INVALID_STATE);
    CHECK(expired == 88 && expired_count == 99);
    CHECK(v2s_probe_plan_cancel(&plan, &active, 1, &active_count) == V2S_PROBE_PLAN_INVALID_STATE);
    CHECK(active == 66 && active_count == 55);
    CHECK(v2s_probe_plan_task_state(&plan, 0, &state) == V2S_PROBE_PLAN_INVALID_STATE);
    CHECK(state == V2S_PROBE_TASK_SUCCEEDED);
}

static void test_null_output_buffers_are_rejected_transactionally(void) {
    current_test = "null_output_buffers_are_rejected_transactionally";
    V2SProbePlan plan;
    size_t index = 99;
    size_t count = 77;
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 10, 0, 100) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 1, &index) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_expire(&plan, 11, NULL, 1, &count) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(count == 77);
    CHECK(v2s_probe_plan_cancel(&plan, NULL, 1, &count) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(count == 77);
    CHECK(v2s_probe_plan_expire(&plan, 11, NULL, 0, &count) == V2S_PROBE_PLAN_OUTPUT_TOO_SMALL);
    CHECK(count == 77);
    CHECK(v2s_probe_plan_cancel(&plan, NULL, 0, &count) == V2S_PROBE_PLAN_OUTPUT_TOO_SMALL);
    CHECK(count == 77);
}

static uint32_t next_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static size_t count_in_flight(const V2SProbePlan *plan) {
    size_t count = 0;
    size_t i;
    for (i = 0; i < plan->candidate_count; i++) {
        if (plan->tasks[i].state == V2S_PROBE_TASK_IN_FLIGHT) count++;
    }
    return count;
}

static int plan_invariants_hold(const V2SProbePlan *plan) {
    return plan->in_flight_count == count_in_flight(plan) &&
           plan->in_flight_count <= plan->concurrency_limit &&
           plan->next_candidate <= plan->candidate_count;
}

static void test_generated_schedules_preserve_invariants(void) {
    current_test = "generated_schedules_preserve_invariants";
    uint32_t random_state = UINT32_C(0x51a7c0de);
    size_t trial;
    for (trial = 0; trial < 500; trial++) {
        V2SProbePlan plan;
        size_t expired[V2S_MAX_PROBE_PLAN_CANDIDATES];
        size_t expired_count = 0;
        size_t candidate_count = 1 + next_random(&random_state) % 16;
        size_t concurrency = 1 + next_random(&random_state) % 8;
        uint64_t timeout_ms = 1 + next_random(&random_state) % 20;
        uint64_t now_ms = 100;
        uint64_t deadline_ms = 105 + next_random(&random_state) % 80;
        size_t step;
        CHECK(v2s_probe_plan_init(&plan, candidate_count, concurrency, timeout_ms,
                                  now_ms, deadline_ms) == V2S_PROBE_PLAN_OK);
        for (step = 0; step < 128 && now_ms < deadline_ms; step++) {
            size_t index;
            size_t offset;
            V2SProbePlanStatus status;
            CHECK(plan_invariants_hold(&plan));
            if (next_random(&random_state) % 4 == 0) {
                status = v2s_probe_plan_expire(&plan, now_ms, expired,
                                               V2S_MAX_PROBE_PLAN_CANDIDATES,
                                               &expired_count);
                CHECK(status == V2S_PROBE_PLAN_OK);
                CHECK(plan_invariants_hold(&plan));
            }
            status = v2s_probe_plan_next(&plan, now_ms, &index);
            if (status == V2S_PROBE_PLAN_OK) {
                CHECK(index < candidate_count);
                CHECK(plan.tasks[index].state == V2S_PROBE_TASK_IN_FLIGHT);
            } else {
                CHECK(status == V2S_PROBE_PLAN_BUSY || status == V2S_PROBE_PLAN_COMPLETE ||
                      status == V2S_PROBE_PLAN_DEADLINE_REACHED);
            }
            CHECK(plan_invariants_hold(&plan));
            if (plan.in_flight_count != 0 && next_random(&random_state) % 3 != 0) {
                size_t start = next_random(&random_state) % candidate_count;
                size_t active_index = candidate_count;
                for (offset = 0; offset < candidate_count; offset++) {
                    size_t candidate = (start + offset) % candidate_count;
                    if (plan.tasks[candidate].state == V2S_PROBE_TASK_IN_FLIGHT) {
                        active_index = candidate;
                        break;
                    }
                }
                CHECK(active_index < candidate_count);
                {
                    uint64_t completed_at = now_ms + next_random(&random_state) % 20;
                    status = v2s_probe_plan_complete(&plan, active_index, completed_at,
                                                     (uint8_t)(next_random(&random_state) & 1));
                    CHECK(status == V2S_PROBE_PLAN_OK || status == V2S_PROBE_PLAN_TIMED_OUT);
                    if (completed_at > now_ms) now_ms = completed_at;
                }
                CHECK(plan_invariants_hold(&plan));
            }
            if (next_random(&random_state) % 97 == 0) {
                size_t active[V2S_MAX_PROBE_CONCURRENCY];
                size_t active_count = 0;
                CHECK(v2s_probe_plan_cancel(&plan, active,
                                            V2S_MAX_PROBE_CONCURRENCY,
                                            &active_count) == V2S_PROBE_PLAN_OK);
                CHECK(active_count <= concurrency);
                CHECK(plan_invariants_hold(&plan));
                break;
            }
            now_ms += 1 + next_random(&random_state) % 7;
        }
        if (!plan.cancelled) {
            CHECK(v2s_probe_plan_expire(&plan, deadline_ms, expired,
                                        V2S_MAX_PROBE_PLAN_CANDIDATES,
                                        &expired_count) == V2S_PROBE_PLAN_OK);
            CHECK(plan.in_flight_count == 0);
            for (step = 0; step < candidate_count; step++) {
                CHECK(plan.tasks[step].state != V2S_PROBE_TASK_PENDING);
                CHECK(plan.tasks[step].state != V2S_PROBE_TASK_IN_FLIGHT);
            }
        }
        CHECK(plan_invariants_hold(&plan));
    }
}

static void test_initialization_policy_and_null_arguments(void) {
    current_test = "initialization_policy_and_null_arguments";
    V2SProbePlan plan;
    size_t index = 99;
    CHECK(v2s_probe_plan_init(NULL, 1, 1, 10, 0, 100) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_init(&plan, V2S_MAX_PROBE_PLAN_CANDIDATES + 1, 1, 10, 0, 100) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_init(&plan, 1, 0, 10, 0, 100) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_init(&plan, 1, V2S_MAX_PROBE_CONCURRENCY + 1, 10, 0, 100) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 0, 0, 100) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_init(&plan, 1, 1, 10, 100, 100) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_init(&plan, 0, 1, 10, 0, 100) == V2S_PROBE_PLAN_OK);
    CHECK(v2s_probe_plan_next(&plan, 0, &index) == V2S_PROBE_PLAN_COMPLETE);
    CHECK(v2s_probe_plan_next(NULL, 0, &index) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
    CHECK(v2s_probe_plan_next(&plan, 0, NULL) == V2S_PROBE_PLAN_INVALID_ARGUMENT);
}

int main(void) {
    test_concurrency_limit_and_ordered_dispatch();
    test_per_probe_expiry_frees_capacity_and_late_completion_is_ignored();
    test_global_deadline_marks_pending_not_started_and_active_timed_out();
    test_deadline_and_timeout_boundaries_are_inclusive();
    test_expire_capacity_failure_preserves_state_and_outputs();
    test_cancel_reports_inflight_tasks_and_is_idempotent();
    test_cancel_capacity_failure_is_transactional();
    test_invalid_transitions_and_clock_order_are_observable();
    test_exhausted_dispatch_waits_for_in_flight_work();
    test_corrupt_plan_state_is_rejected_transactionally();
    test_null_output_buffers_are_rejected_transactionally();
    test_generated_schedules_preserve_invariants();
    test_initialization_policy_and_null_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d Smart Connect probe-plan test(s) failed.\n", failures);
        return 1;
    }
    puts("13 Smart Connect probe-plan tests passed (including 500 generated schedules).");
    return 0;
}
