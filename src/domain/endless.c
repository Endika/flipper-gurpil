#include "include/domain/endless.h"

void endless_init(EndlessState *state) {
    state->phase = EndlessIdle;
    state->time_left_ms = ENDLESS_START_TIME_MS;
    state->distance = 0;
    state->next_checkpoint = ENDLESS_CHECKPOINT_SPACING;
    state->checkpoints_hit = 0;
}

void endless_start(EndlessState *state) {
    if (state->phase == EndlessIdle) {
        state->phase = EndlessRunning;
    }
}

void endless_tick(EndlessState *state, uint32_t dt_ms, int32_t distance) {
    if (state->phase != EndlessRunning) {
        return;
    }

    if (distance > state->distance) {
        state->distance = distance;
    }

    // Saturating subtract: a dt_ms larger than the remaining budget clamps to 0 instead of
    // wrapping around (time_left_ms is unsigned, so plain subtraction could underflow).
    if (dt_ms >= state->time_left_ms) {
        state->time_left_ms = 0;
    } else {
        state->time_left_ms -= dt_ms;
    }

    // Loop so a single big distance jump that crosses several checkpoints at once still
    // grants a bonus and a next_checkpoint advance for each one crossed.
    while (state->distance >= state->next_checkpoint) {
        uint32_t bonus_total = state->time_left_ms + ENDLESS_CHECKPOINT_BONUS_MS;
        state->time_left_ms = bonus_total > ENDLESS_MAX_TIME_MS ? ENDLESS_MAX_TIME_MS : bonus_total;
        state->next_checkpoint += ENDLESS_CHECKPOINT_SPACING;
        state->checkpoints_hit++;
    }

    if (state->time_left_ms == 0) {
        state->phase = EndlessOver;
    }
}
