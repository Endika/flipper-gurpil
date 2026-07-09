#pragma once

#include <stdint.h>

/*
 * Pure, deterministic endless-mode checkpoint timer (no furi, no I/O, no floats).
 *
 * The vehicle never "dies" from the terrain in endless mode: the only fail condition is
 * running out of time. The run starts with a small time budget; every ENDLESS_CHECKPOINT_SPACING
 * units of distance travelled tops the timer back up by ENDLESS_CHECKPOINT_BONUS_MS (capped at
 * ENDLESS_MAX_TIME_MS), so a run that keeps making distance keeps surviving. The caller (app layer)
 * owns the real time source and the sim's distance; this module only advances state given
 * an elapsed `dt_ms` and the sim's current distance — it never reads a clock itself.
 */

typedef enum {
    EndlessIdle,    // not started yet: endless_init's resting state.
    EndlessRunning, // timer depleting, distance/checkpoints tracked.
    EndlessOver,    // time_left_ms hit 0: run is frozen, score is `distance`.
} EndlessPhase;

// Tuning constants. Public (unlike the rest of this module's internals) so the app/UI layer can
// read the exact same values it displays instead of duplicating them — e.g. the checkpoint-flash
// HUD text formats ENDLESS_CHECKPOINT_BONUS_MS directly (see src/views/game_view.c).
enum {
    ENDLESS_START_TIME_MS = 18000,      // initial time budget for a fresh run, ms.
    ENDLESS_CHECKPOINT_BONUS_MS = 4000, // time added per checkpoint crossed, ms.
    ENDLESS_CHECKPOINT_SPACING = 50,    // distance units between checkpoints.
    ENDLESS_MAX_TIME_MS = 27000,        // hard cap on time_left_ms; bonuses never push past this.
};

typedef struct {
    EndlessPhase phase;
    uint32_t time_left_ms;    // remaining time budget; saturates at 0, never wraps.
    int32_t distance;         // best (monotonic non-decreasing) distance reached so far.
    int32_t next_checkpoint;  // distance threshold that triggers the next time bonus.
    uint32_t checkpoints_hit; // total checkpoints crossed so far, for scoring/UI.
} EndlessState;

/* Resets `state` to a run's start: EndlessIdle; time_left_ms = ENDLESS_START_TIME_MS;
 * distance = 0; next_checkpoint = ENDLESS_CHECKPOINT_SPACING; checkpoints_hit = 0. */
void endless_init(EndlessState *state);

/* Moves `state` from EndlessIdle to EndlessRunning. No-op if not currently Idle. */
void endless_start(EndlessState *state);

/* Advances `state` by one tick of `dt_ms` milliseconds, given the sim's current `distance`.
 * No-op unless `state->phase == EndlessRunning` (a call while Idle or Over does nothing).
 * Effects, in order:
 *   1. `state->distance` becomes max(state->distance, distance) — monotonic, never regresses
 *      even if the caller passes a smaller distance than already recorded.
 *   2. `time_left_ms` is depleted by `dt_ms`, saturating at 0 (never underflows/wraps).
 *   3. While `state->distance >= next_checkpoint`: add ENDLESS_CHECKPOINT_BONUS_MS to
 *      time_left_ms (capped at ENDLESS_MAX_TIME_MS), advance next_checkpoint by
 *      ENDLESS_CHECKPOINT_SPACING, and increment checkpoints_hit. Looped so several checkpoints
 *      crossed in one big jump all count.
 *   4. If `time_left_ms == 0`, phase becomes EndlessOver (freezing distance/checkpoints: later
 *      ticks are no-ops per the guard above). */
void endless_tick(EndlessState *state, uint32_t dt_ms, int32_t distance);
