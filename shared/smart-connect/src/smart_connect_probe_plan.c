#include "v2speed/smart_connect_probe_plan.h"

#include <string.h>

static int valid_plan(const V2SProbePlan *plan) {
    size_t i;
    size_t in_flight = 0;
    if (plan == NULL || plan->candidate_count > V2S_MAX_PROBE_PLAN_CANDIDATES ||
        plan->next_candidate > plan->candidate_count ||
        plan->concurrency_limit == 0 || plan->concurrency_limit > V2S_MAX_PROBE_CONCURRENCY ||
        plan->in_flight_count > plan->concurrency_limit || plan->timeout_ms == 0 ||
        plan->deadline_ms <= plan->created_at_ms || plan->cancelled > 1) {
        return 0;
    }
    for (i = 0; i < plan->candidate_count; i++) {
        if (plan->tasks[i].state < V2S_PROBE_TASK_PENDING ||
            plan->tasks[i].state > V2S_PROBE_TASK_CANCELLED) {
            return 0;
        }
        if (plan->tasks[i].state == V2S_PROBE_TASK_IN_FLIGHT) in_flight++;
    }
    return in_flight == plan->in_flight_count;
}

static int now_not_before_active_tasks(const V2SProbePlan *plan, uint64_t now_ms) {
    size_t i;
    for (i = 0; i < plan->candidate_count; i++) {
        if (plan->tasks[i].state == V2S_PROBE_TASK_IN_FLIGHT &&
            now_ms < plan->tasks[i].started_at_ms) {
            return 0;
        }
    }
    return 1;
}

V2SProbePlanStatus v2s_probe_plan_init(
    V2SProbePlan *plan,
    size_t candidate_count,
    size_t concurrency_limit,
    uint64_t per_probe_timeout_ms,
    uint64_t created_at_ms,
    uint64_t deadline_ms) {
    if (plan == NULL || candidate_count > V2S_MAX_PROBE_PLAN_CANDIDATES ||
        concurrency_limit == 0 || concurrency_limit > V2S_MAX_PROBE_CONCURRENCY ||
        per_probe_timeout_ms == 0 || deadline_ms <= created_at_ms) {
        return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    }
    (void)memset(plan, 0, sizeof(*plan));
    plan->candidate_count = candidate_count;
    plan->concurrency_limit = concurrency_limit;
    plan->timeout_ms = per_probe_timeout_ms;
    plan->created_at_ms = created_at_ms;
    plan->deadline_ms = deadline_ms;
    return V2S_PROBE_PLAN_OK;
}

V2SProbePlanStatus v2s_probe_plan_next(
    V2SProbePlan *plan,
    uint64_t now_ms,
    size_t *out_candidate_index) {
    if (plan == NULL || out_candidate_index == NULL) return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    if (!valid_plan(plan)) return V2S_PROBE_PLAN_INVALID_STATE;
    if (plan->cancelled) return V2S_PROBE_PLAN_CANCELLED;
    if (now_ms < plan->created_at_ms || !now_not_before_active_tasks(plan, now_ms)) {
        return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    }
    if (plan->candidate_count == 0 ||
        (plan->next_candidate == plan->candidate_count && plan->in_flight_count == 0)) {
        return V2S_PROBE_PLAN_COMPLETE;
    }
    if (now_ms >= plan->deadline_ms) return V2S_PROBE_PLAN_DEADLINE_REACHED;
    if (plan->in_flight_count >= plan->concurrency_limit) return V2S_PROBE_PLAN_BUSY;

    while (plan->next_candidate < plan->candidate_count &&
           plan->tasks[plan->next_candidate].state != V2S_PROBE_TASK_PENDING) {
        plan->next_candidate++;
    }
    if (plan->next_candidate == plan->candidate_count) {
        return plan->in_flight_count == 0 ? V2S_PROBE_PLAN_COMPLETE : V2S_PROBE_PLAN_BUSY;
    }

    *out_candidate_index = plan->next_candidate;
    plan->tasks[plan->next_candidate].started_at_ms = now_ms;
    plan->tasks[plan->next_candidate].state = V2S_PROBE_TASK_IN_FLIGHT;
    plan->next_candidate++;
    plan->in_flight_count++;
    return V2S_PROBE_PLAN_OK;
}

V2SProbePlanStatus v2s_probe_plan_complete(
    V2SProbePlan *plan,
    size_t candidate_index,
    uint64_t now_ms,
    uint8_t succeeded) {
    V2SProbeTask *task;
    if (plan == NULL) return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    if (!valid_plan(plan)) return V2S_PROBE_PLAN_INVALID_STATE;
    if (candidate_index >= plan->candidate_count || succeeded > 1) {
        return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    }
    if (plan->cancelled) return V2S_PROBE_PLAN_CANCELLED;
    task = &plan->tasks[candidate_index];
    if (task->state == V2S_PROBE_TASK_TIMED_OUT) return V2S_PROBE_PLAN_TIMED_OUT;
    if (task->state != V2S_PROBE_TASK_IN_FLIGHT) return V2S_PROBE_PLAN_INVALID_STATE;
    if (now_ms < plan->created_at_ms || now_ms < task->started_at_ms ||
        !now_not_before_active_tasks(plan, now_ms)) {
        return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    }

    plan->in_flight_count--;
    if (now_ms >= plan->deadline_ms || now_ms - task->started_at_ms >= plan->timeout_ms) {
        task->state = V2S_PROBE_TASK_TIMED_OUT;
        return V2S_PROBE_PLAN_TIMED_OUT;
    }
    task->state = succeeded ? V2S_PROBE_TASK_SUCCEEDED : V2S_PROBE_TASK_FAILED;
    return V2S_PROBE_PLAN_OK;
}

V2SProbePlanStatus v2s_probe_plan_expire(
    V2SProbePlan *plan,
    uint64_t now_ms,
    size_t *out_expired_indices,
    size_t expired_capacity,
    size_t *out_expired_count) {
    size_t expired_count = 0;
    size_t i;
    const int deadline_reached = plan != NULL && now_ms >= plan->deadline_ms;
    if (plan == NULL || out_expired_count == NULL) return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    if (!valid_plan(plan)) return V2S_PROBE_PLAN_INVALID_STATE;
    if (plan->cancelled) return V2S_PROBE_PLAN_CANCELLED;
    if (now_ms < plan->created_at_ms || !now_not_before_active_tasks(plan, now_ms)) {
        return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    }

    for (i = 0; i < plan->candidate_count; i++) {
        const V2SProbeTask *task = &plan->tasks[i];
        if (task->state == V2S_PROBE_TASK_IN_FLIGHT &&
            (deadline_reached || now_ms - task->started_at_ms >= plan->timeout_ms)) {
            expired_count++;
        }
    }
    if (expired_count > expired_capacity) return V2S_PROBE_PLAN_OUTPUT_TOO_SMALL;
    if (expired_count != 0 && out_expired_indices == NULL) return V2S_PROBE_PLAN_INVALID_ARGUMENT;

    expired_count = 0;
    for (i = 0; i < plan->candidate_count; i++) {
        V2SProbeTask *task = &plan->tasks[i];
        if (deadline_reached && task->state == V2S_PROBE_TASK_PENDING) {
            task->state = V2S_PROBE_TASK_NOT_STARTED;
        } else if (task->state == V2S_PROBE_TASK_IN_FLIGHT &&
                   (deadline_reached || now_ms - task->started_at_ms >= plan->timeout_ms)) {
            out_expired_indices[expired_count++] = i;
            task->state = V2S_PROBE_TASK_TIMED_OUT;
            plan->in_flight_count--;
        }
    }
    *out_expired_count = expired_count;
    return V2S_PROBE_PLAN_OK;
}

V2SProbePlanStatus v2s_probe_plan_cancel(
    V2SProbePlan *plan,
    size_t *out_in_flight_indices,
    size_t index_capacity,
    size_t *out_in_flight_count) {
    size_t active_count = 0;
    size_t i;
    if (plan == NULL || out_in_flight_count == NULL) return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    if (!valid_plan(plan)) return V2S_PROBE_PLAN_INVALID_STATE;
    if (plan->cancelled) {
        *out_in_flight_count = 0;
        return V2S_PROBE_PLAN_OK;
    }
    for (i = 0; i < plan->candidate_count; i++) {
        if (plan->tasks[i].state == V2S_PROBE_TASK_IN_FLIGHT) active_count++;
    }
    if (active_count > index_capacity) return V2S_PROBE_PLAN_OUTPUT_TOO_SMALL;
    if (active_count != 0 && out_in_flight_indices == NULL) return V2S_PROBE_PLAN_INVALID_ARGUMENT;

    active_count = 0;
    for (i = 0; i < plan->candidate_count; i++) {
        V2SProbeTask *task = &plan->tasks[i];
        if (task->state == V2S_PROBE_TASK_IN_FLIGHT) {
            out_in_flight_indices[active_count++] = i;
            task->state = V2S_PROBE_TASK_CANCELLED;
        } else if (task->state == V2S_PROBE_TASK_PENDING) {
            task->state = V2S_PROBE_TASK_CANCELLED;
        }
    }
    plan->in_flight_count = 0;
    plan->cancelled = 1;
    *out_in_flight_count = active_count;
    return V2S_PROBE_PLAN_OK;
}

V2SProbePlanStatus v2s_probe_plan_task_state(
    const V2SProbePlan *plan,
    size_t candidate_index,
    V2SProbeTaskState *out_state) {
    if (plan == NULL || out_state == NULL) return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    if (!valid_plan(plan)) return V2S_PROBE_PLAN_INVALID_STATE;
    if (candidate_index >= plan->candidate_count) return V2S_PROBE_PLAN_INVALID_ARGUMENT;
    *out_state = plan->tasks[candidate_index].state;
    return V2S_PROBE_PLAN_OK;
}
