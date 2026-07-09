#include "include/domain/endless.h"

#include <assert.h>
#include <stdio.h>

// START_TIME_MS/CHECKPOINT_BONUS_MS/CHECKPOINT_SPACING/MAX_TIME_MS live in endless.c as an
// anonymous enum, so tests mirror the values here rather than reaching into the module's
// internals. Kept in one place (this comment) to make future drift obvious if endless.c changes.
#define EXPECT_START_TIME_MS 10000u
#define EXPECT_CHECKPOINT_BONUS_MS 3000u
#define EXPECT_CHECKPOINT_SPACING 50
#define EXPECT_MAX_TIME_MS 15000u

static void test_init_sets_expected_resting_state(void) {
    EndlessState state;
    endless_init(&state);

    assert(state.phase == EndlessIdle);
    assert(state.time_left_ms == EXPECT_START_TIME_MS);
    assert(state.distance == 0);
    assert(state.next_checkpoint == EXPECT_CHECKPOINT_SPACING);
    assert(state.checkpoints_hit == 0);
}

static void test_start_moves_idle_to_running(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);
    assert(state.phase == EndlessRunning);
}

static void test_timer_depletes_by_dt_while_running(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    endless_tick(&state, 1000u, 0);
    assert(state.time_left_ms == EXPECT_START_TIME_MS - 1000u);

    endless_tick(&state, 500u, 0);
    assert(state.time_left_ms == EXPECT_START_TIME_MS - 1500u);
}

static void test_timer_saturates_at_zero_instead_of_wrapping(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    // dt_ms far larger than the remaining budget must clamp to 0, not wrap to a huge value.
    endless_tick(&state, EXPECT_START_TIME_MS + 5000u, 0);
    assert(state.time_left_ms == 0);
    assert(state.phase == EndlessOver);
}

static void test_crossing_one_checkpoint_adds_bonus_and_advances_next(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    endless_tick(&state, 100u, EXPECT_CHECKPOINT_SPACING);

    uint32_t expected_time = EXPECT_START_TIME_MS - 100u + EXPECT_CHECKPOINT_BONUS_MS;
    assert(state.time_left_ms == expected_time);
    assert(state.next_checkpoint == EXPECT_CHECKPOINT_SPACING * 2);
    assert(state.checkpoints_hit == 1);
    assert(state.distance == EXPECT_CHECKPOINT_SPACING);
}

static void test_crossing_several_checkpoints_in_one_tick(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    // Drain the budget down to a small, known value first (no checkpoint crossed: distance
    // stays at 0) so the bonus math below stays well clear of MAX_TIME_MS and is checkable
    // by plain addition instead of tangling with the cap (covered separately below).
    uint32_t drained_time = 100u;
    endless_tick(&state, EXPECT_START_TIME_MS - drained_time, 0);
    assert(state.time_left_ms == drained_time);
    assert(state.phase == EndlessRunning);

    // A single big distance jump spanning 3 checkpoint spacings must grant 3 bonuses.
    int32_t jump_distance = EXPECT_CHECKPOINT_SPACING * 3;
    endless_tick(&state, 0u, jump_distance);

    uint32_t expected_time = drained_time + EXPECT_CHECKPOINT_BONUS_MS * 3u;
    assert(state.time_left_ms == expected_time);
    assert(state.next_checkpoint == EXPECT_CHECKPOINT_SPACING * 4);
    assert(state.checkpoints_hit == 3);
    assert(state.distance == jump_distance);
}

static void test_time_bonus_is_capped_at_max_time(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    // Enough checkpoints crossed at once that uncapped bonuses would exceed MAX_TIME_MS.
    int32_t jump_distance = EXPECT_CHECKPOINT_SPACING * 10;
    endless_tick(&state, 0u, jump_distance);

    assert(state.time_left_ms == EXPECT_MAX_TIME_MS);
    assert(state.checkpoints_hit == 10);
}

static void test_phase_becomes_over_exactly_when_time_hits_zero(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    endless_tick(&state, EXPECT_START_TIME_MS - 1u, 0);
    assert(state.phase == EndlessRunning);
    assert(state.time_left_ms == 1u);

    endless_tick(&state, 1u, 0);
    assert(state.phase == EndlessOver);
    assert(state.time_left_ms == 0);
}

static void test_over_freezes_distance_and_does_not_resurrect(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    endless_tick(&state, EXPECT_START_TIME_MS, 42);
    assert(state.phase == EndlessOver);
    assert(state.distance == 42);
    uint32_t checkpoints_at_over = state.checkpoints_hit;

    // Further ticks after Over must not change distance, phase, or checkpoints_hit, even
    // with a larger distance and a nonzero dt_ms that would otherwise deplete more time.
    endless_tick(&state, 500u, 10000);
    assert(state.phase == EndlessOver);
    assert(state.distance == 42);
    assert(state.checkpoints_hit == checkpoints_at_over);
    assert(state.time_left_ms == 0);
}

static void test_distance_is_monotonic_non_decreasing(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);

    endless_tick(&state, 10u, 100);
    assert(state.distance == 100);

    // A tick reporting a smaller distance than already recorded must not regress it.
    endless_tick(&state, 10u, 30);
    assert(state.distance == 100);
}

static void test_tick_while_idle_does_nothing(void) {
    EndlessState state;
    endless_init(&state);

    endless_tick(&state, 1000u, 500);

    assert(state.phase == EndlessIdle);
    assert(state.time_left_ms == EXPECT_START_TIME_MS);
    assert(state.distance == 0);
    assert(state.checkpoints_hit == 0);
}

static void test_tick_while_over_does_nothing_beyond_freeze(void) {
    EndlessState state;
    endless_init(&state);
    endless_start(&state);
    // Distance stays below the first checkpoint so no bonus rescues the timer from 0.
    int32_t distance_below_checkpoint = EXPECT_CHECKPOINT_SPACING - 1;
    endless_tick(&state, EXPECT_START_TIME_MS, distance_below_checkpoint);
    assert(state.phase == EndlessOver);

    endless_tick(&state, 1000u, 900);

    assert(state.phase == EndlessOver);
    assert(state.distance == distance_below_checkpoint);
    assert(state.time_left_ms == 0);
}

int main(void) {
    test_init_sets_expected_resting_state();
    printf("test_init_sets_expected_resting_state: PASS\n");

    test_start_moves_idle_to_running();
    printf("test_start_moves_idle_to_running: PASS\n");

    test_timer_depletes_by_dt_while_running();
    printf("test_timer_depletes_by_dt_while_running: PASS\n");

    test_timer_saturates_at_zero_instead_of_wrapping();
    printf("test_timer_saturates_at_zero_instead_of_wrapping: PASS\n");

    test_crossing_one_checkpoint_adds_bonus_and_advances_next();
    printf("test_crossing_one_checkpoint_adds_bonus_and_advances_next: PASS\n");

    test_crossing_several_checkpoints_in_one_tick();
    printf("test_crossing_several_checkpoints_in_one_tick: PASS\n");

    test_time_bonus_is_capped_at_max_time();
    printf("test_time_bonus_is_capped_at_max_time: PASS\n");

    test_phase_becomes_over_exactly_when_time_hits_zero();
    printf("test_phase_becomes_over_exactly_when_time_hits_zero: PASS\n");

    test_over_freezes_distance_and_does_not_resurrect();
    printf("test_over_freezes_distance_and_does_not_resurrect: PASS\n");

    test_distance_is_monotonic_non_decreasing();
    printf("test_distance_is_monotonic_non_decreasing: PASS\n");

    test_tick_while_idle_does_nothing();
    printf("test_tick_while_idle_does_nothing: PASS\n");

    test_tick_while_over_does_nothing_beyond_freeze();
    printf("test_tick_while_over_does_nothing_beyond_freeze: PASS\n");

    return 0;
}
