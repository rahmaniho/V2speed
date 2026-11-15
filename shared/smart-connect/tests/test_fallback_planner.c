#include "v2speed/fallback_planner.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;
static const char *current_test = "";
static unsigned int property_seed = 0x6D2B79F5u;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d [%s]: %s\n", __FILE__, __LINE__, current_test, #condition); \
        failures++; \
        return; \
    } \
} while (0)

static unsigned int next_property_value(void) {
    property_seed ^= property_seed << 13;
    property_seed ^= property_seed >> 17;
    property_seed ^= property_seed << 5;
    return property_seed;
}

static V2SRankedCandidate rank_entry(size_t input_index) {
    V2SRankedCandidate result = {0};
    result.input_index = input_index;
    result.score = 50.0;
    result.evidence_coverage = 50.0;
    return result;
}

static void test_attempts_follow_rank_order_and_fallback_after_failure(void) {
    current_test = "attempts_follow_rank_order_and_fallback_after_failure";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(7), rank_entry(3), rank_entry(9) };
    size_t index = 99;
    CHECK(v2s_fallback_init(&planner, ranked, 3, 3) == V2S_FALLBACK_OK);
    ranked[0] = rank_entry(99); /* The planner owns a snapshot, not this mutable memory. */
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(index == 7 && planner.attempts_started == 1 && planner.in_flight);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(index == 3 && planner.attempts_started == 2);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(index == 9 && planner.attempts_started == 3);
}

static void test_verified_tunnel_ends_fallback(void) {
    current_test = "verified_tunnel_ends_fallback";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(1), rank_entry(2) };
    size_t index = 0;
    CHECK(v2s_fallback_init(&planner, ranked, 2, 2) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_TUNNEL_VERIFIED) == V2S_FALLBACK_OK);
    CHECK(planner.result == V2S_FALLBACK_RESULT_CONNECTED);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_FINISHED);
}

static void test_max_attempts_caps_retries(void) {
    current_test = "max_attempts_caps_retries";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(10), rank_entry(11), rank_entry(12), rank_entry(13) };
    size_t index = 0;
    CHECK(v2s_fallback_init(&planner, ranked, 4, 2) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(index == 10);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(index == 11);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
    CHECK(planner.result == V2S_FALLBACK_RESULT_EXHAUSTED);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_EXHAUSTED);
}

static void test_cancel_is_terminal_and_does_not_fallback(void) {
    current_test = "cancel_is_terminal_and_does_not_fallback";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(4), rank_entry(5) };
    size_t index = 0;
    CHECK(v2s_fallback_init(&planner, ranked, 2, 2) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_CANCELLED) == V2S_FALLBACK_OK);
    CHECK(planner.result == V2S_FALLBACK_RESULT_CANCELLED);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_FINISHED);
}

static void test_single_inflight_attempt_and_result_validation(void) {
    current_test = "single_inflight_attempt_and_result_validation";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(2), rank_entry(8) };
    size_t index = 99;
    CHECK(v2s_fallback_init(&planner, ranked, 2, 2) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_BUSY);
    CHECK(v2s_fallback_complete(&planner, (V2SAttemptOutcome)99) == V2S_FALLBACK_INVALID_ARGUMENT);
    CHECK(planner.in_flight);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_INVALID_ARGUMENT);
}

static void test_empty_plan_and_invalid_limits(void) {
    current_test = "empty_plan_and_invalid_limits";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[1] = { rank_entry(0) };
    size_t index = 88;
    CHECK(v2s_fallback_init(&planner, NULL, 0, 1) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_EXHAUSTED);
    CHECK(planner.result == V2S_FALLBACK_RESULT_EXHAUSTED);
    CHECK(index == 88);
    CHECK(v2s_fallback_init(&planner, ranked, 1, 0) == V2S_FALLBACK_INVALID_PLAN);
    CHECK(v2s_fallback_init(&planner, ranked, 1, V2S_MAX_CONNECT_ATTEMPTS + 1) == V2S_FALLBACK_INVALID_PLAN);
    CHECK(v2s_fallback_init(&planner, ranked, V2S_MAX_CANDIDATES + 1, 1) == V2S_FALLBACK_INVALID_PLAN);
    CHECK(v2s_fallback_init(&planner, NULL, 1, 1) == V2S_FALLBACK_INVALID_ARGUMENT);
    CHECK(v2s_fallback_init(NULL, ranked, 1, 1) == V2S_FALLBACK_INVALID_ARGUMENT);
    CHECK(v2s_fallback_begin_next(NULL, &index) == V2S_FALLBACK_INVALID_ARGUMENT);
    CHECK(v2s_fallback_begin_next(&planner, NULL) == V2S_FALLBACK_INVALID_ARGUMENT);
    CHECK(v2s_fallback_complete(NULL, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_INVALID_ARGUMENT);
}

static void test_duplicate_ranked_candidate_is_rejected(void) {
    current_test = "duplicate_ranked_candidate_is_rejected";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(2), rank_entry(2) };
    CHECK(v2s_fallback_init(&planner, ranked, 2, 2) == V2S_FALLBACK_DUPLICATE_CANDIDATE);
}

static void test_invalid_rank_measurements_do_not_mutate_planner(void) {
    current_test = "invalid_rank_measurements_do_not_mutate_planner";
    V2SFallbackPlanner planner;
    V2SFallbackPlanner before;
    V2SRankedCandidate ranked[] = { rank_entry(1) };
    CHECK(v2s_fallback_init(&planner, ranked, 1, 1) == V2S_FALLBACK_OK);
    before = planner;
    ranked[0].score = NAN;
    CHECK(v2s_fallback_init(&planner, ranked, 1, 1) == V2S_FALLBACK_INVALID_PLAN);
    CHECK(planner.ranked_count == before.ranked_count);
    CHECK(planner.max_attempts == before.max_attempts);
    CHECK(planner.candidate_indices[0] == before.candidate_indices[0]);
    CHECK(planner.result == before.result && planner.attempts_started == before.attempts_started);
    ranked[0] = rank_entry(1);
    ranked[0].evidence_coverage = 101.0;
    CHECK(v2s_fallback_init(&planner, ranked, 1, 1) == V2S_FALLBACK_INVALID_PLAN);
    CHECK(planner.ranked_count == before.ranked_count);
    CHECK(planner.max_attempts == before.max_attempts);
    CHECK(planner.candidate_indices[0] == before.candidate_indices[0]);
    CHECK(planner.result == before.result && planner.attempts_started == before.attempts_started);
}

static void test_attempt_counter_defensively_enforces_limit(void) {
    current_test = "attempt_counter_defensively_enforces_limit";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(1), rank_entry(2) };
    size_t index = 77;
    CHECK(v2s_fallback_init(&planner, ranked, 2, 1) == V2S_FALLBACK_OK);
    planner.attempts_started = planner.max_attempts;
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_EXHAUSTED);
    CHECK(planner.result == V2S_FALLBACK_RESULT_EXHAUSTED);
    CHECK(index == 77);
}

static void test_exhausts_after_all_ranked_candidates_fail(void) {
    current_test = "exhausts_after_all_ranked_candidates_fail";
    V2SFallbackPlanner planner;
    V2SRankedCandidate ranked[] = { rank_entry(6), rank_entry(5) };
    size_t index = 0;
    CHECK(v2s_fallback_init(&planner, ranked, 2, 8) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK && index == 6);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
    CHECK(v2s_fallback_begin_next(&planner, &index) == V2S_FALLBACK_OK && index == 5);
    CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
    CHECK(planner.result == V2S_FALLBACK_RESULT_EXHAUSTED);
}

static void test_generated_plans_never_exceed_the_attempt_cap(void) {
    current_test = "generated_plans_never_exceed_the_attempt_cap";
    size_t batch;
    for (batch = 0; batch < 250; batch++) {
        V2SFallbackPlanner planner;
        V2SRankedCandidate ranked[32];
        const size_t ranked_count = (size_t)(next_property_value() % 32u) + 1;
        const size_t max_attempts = (size_t)(next_property_value() % V2S_MAX_CONNECT_ATTEMPTS) + 1;
        const size_t expected_attempts = ranked_count < max_attempts ? ranked_count : max_attempts;
        size_t i;
        size_t input_index;
        for (i = 0; i < ranked_count; i++) {
            ranked[i] = rank_entry(batch * 32 + i);
        }
        CHECK(v2s_fallback_init(&planner, ranked, ranked_count, max_attempts) == V2S_FALLBACK_OK);
        for (i = 0; i < expected_attempts; i++) {
            CHECK(v2s_fallback_begin_next(&planner, &input_index) == V2S_FALLBACK_OK);
            CHECK(input_index == ranked[i].input_index);
            CHECK(v2s_fallback_complete(&planner, V2S_ATTEMPT_FAILED) == V2S_FALLBACK_OK);
        }
        CHECK(planner.attempts_started == expected_attempts);
        CHECK(planner.result == V2S_FALLBACK_RESULT_EXHAUSTED);
        CHECK(v2s_fallback_begin_next(&planner, &input_index) == V2S_FALLBACK_EXHAUSTED);
    }
}

int main(void) {
    test_generated_plans_never_exceed_the_attempt_cap();
    test_attempts_follow_rank_order_and_fallback_after_failure();
    test_verified_tunnel_ends_fallback();
    test_max_attempts_caps_retries();
    test_cancel_is_terminal_and_does_not_fallback();
    test_single_inflight_attempt_and_result_validation();
    test_empty_plan_and_invalid_limits();
    test_duplicate_ranked_candidate_is_rejected();
    test_invalid_rank_measurements_do_not_mutate_planner();
    test_attempt_counter_defensively_enforces_limit();
    test_exhausts_after_all_ranked_candidates_fail();
    if (failures != 0) {
        fprintf(stderr, "%d fallback-planner test(s) failed.\n", failures);
        return 1;
    }
    puts("11 fallback-planner tests passed (including 250 generated plans).");
    return 0;
}
