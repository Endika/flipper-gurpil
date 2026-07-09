#include "include/application/game.h"

#include "include/domain/shapes.h"
#include "include/domain/sim.h"
#include "include/domain/terrain.h"
#include "include/domain/terrain_kind.h"

#include <assert.h>
#include <stdio.h>

#define SEED_A 12345u

// Fixed per-frame timestep, matching the plausible tick used by the sim's own test suite.
#define TICK_MS 15u

// Fixed "wrong" shape mounted for the entire dumb run. It is never `shape_best_for` any
// terrain kind (shapes.c's TABLE has circle/line/triangle as the sole best for flat/rocky-or-
// obstacle/uphill respectively), so a run that never adapts away from it is consistently
// suboptimal.
#define DUMB_SHAPE ShapeSquare

// Upper bound on ticks driven per run before giving up on reaching game-over. Generous: even
// the best-shape run drains its capped time budget (endless.c's MAX_TIME_MS) within a couple
// thousand ticks of TICK_MS each (each checkpoint crossed nets a time *loss* once travel time
// exceeds the fixed bonus, so no shape choice survives indefinitely) — hitting this limit means
// a real bug, not a slow-but-healthy run.
#define MAX_TICKS 20000u

// How many post-game-over ticks to drive in the freeze check.
#define POST_OVER_TICK_COUNT 50u

// endless.c's START_TIME_MS is private to that module; mirrored here (as test_endless.c also
// does) purely to assert game_start's resulting state. Keep in sync if endless.c's tuning
// changes.
#define EXPECT_START_TIME_MS 10000u

// Drives `game` with a per-tick shape chosen as the best for the terrain under the vehicle's
// current position, until endless goes Over or MAX_TICKS is exhausted. Real terrain_at/
// shape_best_for calls, no test seam.
static void run_smart_until_over(GameState *game, uint32_t seed) {
    for (uint32_t i = 0; i < MAX_TICKS && !game_is_over(game); i++) {
        int32_t x = game->sim.distance_fp >> SIM_FP_SHIFT;
        ShapeId best = shape_best_for(terrain_at(seed, x).kind);
        game_set_shape(game, best);
        game_tick(game, TICK_MS);
    }
}

// Drives `game` with a single fixed shape for the whole run, until endless goes Over or
// MAX_TICKS is exhausted.
static void run_fixed_shape_until_over(GameState *game, ShapeId shape) {
    game_set_shape(game, shape);
    for (uint32_t i = 0; i < MAX_TICKS && !game_is_over(game); i++) {
        game_tick(game, TICK_MS);
    }
}

static void test_smart_shape_choice_beats_dumb_fixed_shape(void) {
    GameState smart;
    game_start(&smart, SEED_A);
    run_smart_until_over(&smart, SEED_A);
    assert(game_is_over(&smart));

    GameState dumb;
    game_start(&dumb, SEED_A);
    run_fixed_shape_until_over(&dumb, DUMB_SHAPE);
    assert(game_is_over(&dumb));

    // Proves the shape choice matters end-to-end: adapting to terrain reaches strictly further
    // than sticking to one poor shape, given the same seed and tick pacing.
    assert(game_distance(&smart) > game_distance(&dumb));
}

static void test_over_freezes_distance_through_orchestrator(void) {
    GameState game;
    game_start(&game, SEED_A);
    run_fixed_shape_until_over(&game, DUMB_SHAPE);
    assert(game_is_over(&game));

    int32_t frozen_distance = game_distance(&game);
    uint32_t frozen_time = game_time_left(&game);

    // Further ticks after game-over must not move the run forward at all: the orchestrator's
    // own Running-only guard, not just endless_tick's internal one.
    for (uint32_t i = 0; i < POST_OVER_TICK_COUNT; i++) {
        game_tick(&game, TICK_MS);
    }

    assert(game_is_over(&game));
    assert(game_distance(&game) == frozen_distance);
    assert(game_time_left(&game) == frozen_time);
}

static void test_deterministic_given_same_inputs(void) {
    GameState a;
    GameState b;
    game_start(&a, SEED_A);
    game_start(&b, SEED_A);

    for (uint32_t i = 0; i < MAX_TICKS && !game_is_over(&a); i++) {
        // Same shape sequence fed to both runs: cycle through every shape so it isn't just the
        // default.
        ShapeId shape = (ShapeId)(i % ShapeCount);
        game_set_shape(&a, shape);
        game_set_shape(&b, shape);
        game_tick(&a, TICK_MS);
        game_tick(&b, TICK_MS);
        assert(game_distance(&a) == game_distance(&b));
        assert(game_time_left(&a) == game_time_left(&b));
    }

    assert(game_is_over(&a));
    assert(game_is_over(&b));
    assert(game_distance(&a) == game_distance(&b));
    assert(game_time_left(&a) == game_time_left(&b));
}

static void test_start_initializes_coherent_state(void) {
    GameState game;
    game_start(&game, SEED_A);

    assert(game_distance(&game) == 0);
    assert(game_time_left(&game) == EXPECT_START_TIME_MS);
    assert(!game_is_over(&game));
    assert(game.shape == ShapeCircle);
    assert(game.endless.phase == EndlessRunning);
    // x=0 is always the flat opening zone (terrain.c), so the vehicle starts at that height.
    assert(game_vehicle_y(&game) == terrain_at(SEED_A, 0).height);
}

int main(void) {
    test_smart_shape_choice_beats_dumb_fixed_shape();
    printf("test_smart_shape_choice_beats_dumb_fixed_shape: PASS\n");

    test_over_freezes_distance_through_orchestrator();
    printf("test_over_freezes_distance_through_orchestrator: PASS\n");

    test_deterministic_given_same_inputs();
    printf("test_deterministic_given_same_inputs: PASS\n");

    test_start_initializes_coherent_state();
    printf("test_start_initializes_coherent_state: PASS\n");

    return 0;
}
