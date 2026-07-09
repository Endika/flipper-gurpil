#pragma once

#include "include/domain/endless.h"
#include "include/domain/shapes.h"
#include "include/domain/sim.h"

#include <stdint.h>

/*
 * Pure per-tick orchestrator (no furi, no I/O, integer only) tying together the sim (distance/
 * speed/vehicle height), the endless checkpoint timer, and the player-mounted wheel shape into
 * one game state. The caller (UI/hardware layer) owns the real time source and the RNG seed;
 * this module never reads either itself — both arrive as plain arguments.
 *
 * Design choice: `game_start` moves endless straight to EndlessRunning instead of leaving it
 * Idle until the player's first shape input. Endless mode's whole point is the timer counting
 * down from the very start of a run, so the countdown should be live immediately; there is no
 * "waiting room" phase. `game_tick` only advances the sim while endless is Running (see below),
 * so the vehicle simply doesn't move until the caller starts ticking — an idle player loses time
 * but not distance.
 */

typedef struct {
    SimState sim;
    EndlessState endless;
    ShapeId shape;
    uint32_t seed;
} GameState;

/* Resets `game` to a fresh run: stores `seed` (used by every later sim_step); sim_init and
 * endless_init reset their substates; shape defaults to ShapeCircle; endless is immediately
 * moved to EndlessRunning (see the module comment above for why). */
void game_start(GameState *game, uint32_t seed);

/* Mounts `shape`, effective from the very next game_tick. Out-of-range values (>= ShapeCount)
 * are ignored, leaving the previously mounted shape unchanged. */
void game_set_shape(GameState *game, ShapeId shape);

/* Advances `game` by one fixed step of `dt_ms` milliseconds:
 *   1. no-op unless `game->endless.phase == EndlessRunning` — once endless is Over, the sim is
 *      frozen too, so the whole orchestrator stops, not just the timer.
 *   2. sim_step(&game->sim, game->shape, game->seed, dt_ms) advances distance/speed/vehicle_y.
 *   3. the sim's integer distance feeds endless_tick, which may grant checkpoint bonuses or end
 *      the run. */
void game_tick(GameState *game, uint32_t dt_ms);

/* Best (monotonic non-decreasing) integer distance reached so far this run. */
int32_t game_distance(const GameState *game);

/* Remaining time budget, in milliseconds; 0 once the run is over. */
uint32_t game_time_left(const GameState *game);

/* Non-zero once the run's timer has hit 0 (EndlessOver): distance/time are frozen. */
int game_is_over(const GameState *game);

/* Terrain height (game units) under the vehicle at its current position. */
int16_t game_vehicle_y(const GameState *game);
