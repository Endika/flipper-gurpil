#include "include/views/game_view.h"

#include "include/domain/terrain.h"
#include "include/ui/render_map.h"

#include <gui/canvas.h>
#include <stdio.h>

// Vehicle glyph sizing, screen px. The chassis is a small box riding above the wheel; the
// wheel glyph itself is shape-specific (see render_vehicle) so the mounted ShapeId is legible
// at a glance in 1-bit.
enum {
    VEHICLE_CHASSIS_WIDTH = 10,
    VEHICLE_CHASSIS_HEIGHT = 4,
    VEHICLE_WHEEL_RADIUS = 3,
};

// HUD text baseline, screen px from the top; both corners share it so distance and timer sit
// on the same row.
enum {
    HUD_TEXT_Y = 7,
};

static void render_terrain(Canvas *canvas, const GameState *game) {
    int32_t distance = game_distance(game);
    for (int column = 0; column < GURPIL_SCREEN_WIDTH; column++) {
        int32_t world_x = screen_column_to_world_x(distance, column);
        TerrainSample sample = terrain_at(game->seed, world_x);
        uint8_t ground_y = terrain_height_to_screen_y(sample.height);
        canvas_draw_line(canvas, column, ground_y, column, GURPIL_SCREEN_HEIGHT - 1);
    }
}

static void render_vehicle(Canvas *canvas, const GameState *game) {
    int32_t vx = GURPIL_VEHICLE_COLUMN;
    int32_t vy = terrain_height_to_screen_y(game_vehicle_y(game));

    // Chassis: a small filled box riding just above the wheel glyph.
    canvas_draw_box(canvas, vx - VEHICLE_CHASSIS_WIDTH / 2,
                    vy - VEHICLE_CHASSIS_HEIGHT - VEHICLE_WHEEL_RADIUS, VEHICLE_CHASSIS_WIDTH,
                    VEHICLE_CHASSIS_HEIGHT);

    // Wheel: one distinct glyph per ShapeId, all centered on (vx, vy - radius) so swapping
    // shapes never shifts the chassis.
    switch (game->shape) {
        case ShapeCircle:
            canvas_draw_disc(canvas, vx, vy - VEHICLE_WHEEL_RADIUS, VEHICLE_WHEEL_RADIUS);
            break;
        case ShapeLine:
            canvas_draw_box(canvas, vx - VEHICLE_WHEEL_RADIUS, vy - VEHICLE_WHEEL_RADIUS,
                            VEHICLE_WHEEL_RADIUS * 2, 1);
            break;
        case ShapeSquare:
            canvas_draw_box(canvas, vx - VEHICLE_WHEEL_RADIUS, vy - VEHICLE_WHEEL_RADIUS * 2,
                            VEHICLE_WHEEL_RADIUS * 2, VEHICLE_WHEEL_RADIUS * 2);
            break;
        case ShapeTriangle:
        case ShapeCount:
        default:
            canvas_draw_triangle(canvas, vx, vy - VEHICLE_WHEEL_RADIUS * 2,
                                 VEHICLE_WHEEL_RADIUS * 2, VEHICLE_WHEEL_RADIUS * 2,
                                 CanvasDirectionBottomToTop);
            break;
    }
}

static void render_hud(Canvas *canvas, const GameState *game) {
    char text[16];

    canvas_set_font(canvas, FontSecondary);

    snprintf(text, sizeof(text), "%ld", (long)game_distance(game));
    canvas_draw_str(canvas, 1, HUD_TEXT_Y, text);

    snprintf(text, sizeof(text), "%lu", (unsigned long)(game_time_left(game) / 1000));
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH - 1, HUD_TEXT_Y, AlignRight, AlignBottom,
                            text);
}

static void render_game_over(Canvas *canvas, const GameState *game, int32_t best) {
    char text[24];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, 24, AlignCenter, AlignCenter,
                            "Game Over");

    canvas_set_font(canvas, FontSecondary);

    snprintf(text, sizeof(text), "Distance: %ld", (long)game_distance(game));
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, 38, AlignCenter, AlignCenter, text);

    snprintf(text, sizeof(text), "Best: %ld", (long)best);
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, 48, AlignCenter, AlignCenter, text);
}

void gurpil_render(Canvas *canvas, const GameState *game, int32_t best) {
    canvas_clear(canvas);

    render_terrain(canvas, game);
    render_vehicle(canvas, game);
    render_hud(canvas, game);

    if (game_is_over(game)) {
        render_game_over(canvas, game, best);
    }
}
