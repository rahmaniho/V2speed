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
    test_initialization_policy_and_null_arguments();
    if (failures != 0) {
        fprintf(stderr, "%d Smart Connect probe-plan test(s) failed.\n", failures);
        return 1;
    }
    puts("9 Smart Connect probe-plan tests passed.");
    return 0;
}
