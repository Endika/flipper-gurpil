#include "include/application/game.h"

void game_start(GameState *game, uint32_t seed) {
    game->seed = seed;
    sim_init(&game->sim);
    endless_init(&game->endless);
    game->shape = ShapeCircle;
    endless_start(&game->endless);
}

void game_set_shape(GameState *game, ShapeId shape) {
    if (shape >= ShapeCount) {
        return;
    }
    game->shape = shape;
}

void game_tick(GameState *game, uint32_t dt_ms) {
    if (game->endless.phase != EndlessRunning) {
        return;
    }

    sim_step(&game->sim, game->shape, game->seed, dt_ms);

    int32_t distance = game->sim.distance_fp >> SIM_FP_SHIFT;
    endless_tick(&game->endless, dt_ms, distance);
}

int32_t game_distance(const GameState *game) {
    return game->endless.distance;
}

uint32_t game_time_left(const GameState *game) {
    return game->endless.time_left_ms;
}

int game_is_over(const GameState *game) {
    return game->endless.phase == EndlessOver;
}

int16_t game_vehicle_y(const GameState *game) {
    return game->sim.vehicle_y;
}
