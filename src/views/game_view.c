#include "include/views/game_view.h"

#include "include/domain/terrain.h"
#include "include/ui/render_map.h"

#include <gui/canvas.h>
#include <stdio.h>

// Vehicle sizing, screen px. The wheel glyph is shape-specific and animated (see render_wheel)
// so the mounted ShapeId — and the fact that it is spinning — are both legible at a glance in
// 1-bit; the scooter deck/stem/handlebar and the stick-figure rider sit above it, all anchored
// to the wheel's own center so swapping shapes or the vehicle bobbing over terrain never shifts
// them sideways relative to each other.
enum {
    VEHICLE_WHEEL_RADIUS = 4,

    SCOOTER_DECK_GAP = 1,             // clearance between the wheel's top and the deck line, px.
    SCOOTER_DECK_BACK_OFFSET = 4,     // deck's rear tip, px behind the wheel center.
    SCOOTER_DECK_FRONT_OFFSET = 5,    // deck's front tip (handlebar side), px ahead of center.
    SCOOTER_STEM_HEIGHT = 6,          // handlebar post height above the deck, px.
    SCOOTER_HANDLEBAR_HALF_WIDTH = 2, // handlebar half-width, px.

    RIDER_STANDING_SETBACK = 1, // how far behind the stem the rider's feet plant, px.
    RIDER_TORSO_HEIGHT = 4,     // stick body height above the deck, px.
    RIDER_HEAD_RADIUS = 2,      // round head radius, px.
};

// Tiny shape glyph used on the start screen's controls legend — smaller than the in-run wheel
// glyph since it just needs to be recognizable next to a direction label, not animated.
enum {
    LEGEND_GLYPH_RADIUS = 3,
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

// Wheel: one distinct glyph per ShapeId, all centered on (vx, wheel_y) so swapping shapes never
// shifts the chassis above it. Each glyph animates by stepping through wheel_rotation_step(
// frame)'s discrete angles so the mounted shape visibly spins, not just sits there:
//   - circle: a rim outline (not filled, so the spoke reads against the white interior) plus a
//     spoke line from center to rim.
//   - line: a full diameter bar whose angle steps — the "bar" itself rotates.
//   - square: alternates between an axis-aligned frame and a ~45-degree diamond of the same
//     footprint, a 2-orientation approximation of rotation.
//   - triangle: alternates its apex between pointing up and pointing down (the primitive's own
//     CanvasDirection), a 2-orientation approximation that keeps the whole glyph inside the
//     wheel's footprint (see the anchor-point comment below — cycling all 4 CanvasDirection
//     values would let it poke through the deck or below the ground contact point).
static void render_wheel(Canvas *canvas, int32_t vx, int32_t wheel_y, ShapeId shape,
                         uint32_t frame) {
    int32_t radius = VEHICLE_WHEEL_RADIUS;
    int32_t dx = 0;
    int32_t dy = 0;
    wheel_spoke_endpoint(frame, radius, &dx, &dy);

    switch (shape) {
        case ShapeCircle:
            canvas_draw_circle(canvas, vx, wheel_y, radius);
            canvas_draw_line(canvas, vx, wheel_y, vx + dx, wheel_y + dy);
            break;
        case ShapeLine:
            canvas_draw_line(canvas, vx - dx, wheel_y - dy, vx + dx, wheel_y + dy);
            break;
        case ShapeSquare:
            if (wheel_rotation_step(frame) % 2 == 0) {
                canvas_draw_frame(canvas, vx - radius, wheel_y - radius, radius * 2, radius * 2);
            } else {
                canvas_draw_line(canvas, vx, wheel_y - radius, vx + radius, wheel_y);
                canvas_draw_line(canvas, vx + radius, wheel_y, vx, wheel_y + radius);
                canvas_draw_line(canvas, vx, wheel_y + radius, vx - radius, wheel_y);
                canvas_draw_line(canvas, vx - radius, wheel_y, vx, wheel_y - radius);
            }
            break;
        case ShapeTriangle:
        case ShapeCount:
        default:
            // Alternates the apex-up/apex-down orientation so the triangle's base stays
            // anchored at the wheel's top or bottom edge either way — never poking below the
            // ground contact point or up through the deck above it.
            if (wheel_rotation_step(frame) % 2 == 0) {
                canvas_draw_triangle(canvas, vx, wheel_y + radius, radius * 2, radius * 2,
                                     CanvasDirectionBottomToTop);
            } else {
                canvas_draw_triangle(canvas, vx, wheel_y - radius, radius * 2, radius * 2,
                                     CanvasDirectionTopToBottom);
            }
            break;
    }
}

// Scooter deck/stem/handlebar plus a stick-figure rider standing on it, all anchored to the
// wheel's own center so it reads as one coherent "character on a scooter" silhouette, drawn
// entirely above `wheel_y` — in the white sky, never over the black ground fill — for contrast.
static void render_rider(Canvas *canvas, int32_t vx, int32_t wheel_y) {
    int32_t deck_y = wheel_y - VEHICLE_WHEEL_RADIUS - SCOOTER_DECK_GAP;
    int32_t deck_back_x = vx - SCOOTER_DECK_BACK_OFFSET;
    int32_t deck_front_x = vx + SCOOTER_DECK_FRONT_OFFSET;

    canvas_draw_line(canvas, deck_back_x, deck_y, deck_front_x, deck_y);

    int32_t handlebar_y = deck_y - SCOOTER_STEM_HEIGHT;
    canvas_draw_line(canvas, deck_front_x, deck_y, deck_front_x, handlebar_y);
    canvas_draw_line(canvas, deck_front_x - SCOOTER_HANDLEBAR_HALF_WIDTH, handlebar_y,
                     deck_front_x + SCOOTER_HANDLEBAR_HALF_WIDTH, handlebar_y);

    int32_t rider_x = deck_front_x - RIDER_STANDING_SETBACK;
    int32_t shoulder_y = deck_y - RIDER_TORSO_HEIGHT;
    canvas_draw_line(canvas, rider_x, deck_y, rider_x, shoulder_y);
    canvas_draw_disc(canvas, rider_x, shoulder_y - RIDER_HEAD_RADIUS, RIDER_HEAD_RADIUS);
}

static void render_vehicle(Canvas *canvas, const GameState *game, uint32_t frame) {
    int32_t vx = GURPIL_VEHICLE_COLUMN;
    int32_t vy = terrain_height_to_screen_y(game_vehicle_y(game));
    int32_t wheel_y = vy - VEHICLE_WHEEL_RADIUS;

    render_rider(canvas, vx, wheel_y);
    render_wheel(canvas, vx, wheel_y, game->shape, frame);
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

    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, 58, AlignCenter, AlignCenter,
                            "OK: menu");
}

// One direction label + the D-pad key that shape_for_input_key maps to a shape, in the order
// the legend rows are drawn. The shape itself is looked up from shape_for_input_key rather than
// duplicated here, so the legend can never drift out of sync with the actual controls.
static const struct {
    const char *label;
    GurpilKey key;
} CONTROLS_LEGEND_ENTRIES[CONTROLS_LEGEND_ROW_COUNT] = {
    {"Up", GurpilKeyUp},
    {"Right", GurpilKeyRight},
    {"Down", GurpilKeyDown},
    {"Left", GurpilKeyLeft},
};

// A small, static (non-animated) version of the same glyphs render_wheel draws, just for the
// controls legend — it only needs to be recognizable next to its direction label.
static void render_legend_glyph(Canvas *canvas, int32_t x, int32_t y, ShapeId shape) {
    int32_t radius = LEGEND_GLYPH_RADIUS;

    switch (shape) {
        case ShapeCircle:
            canvas_draw_circle(canvas, x, y, radius);
            break;
        case ShapeLine:
            canvas_draw_line(canvas, x - radius, y, x + radius, y);
            break;
        case ShapeSquare:
            canvas_draw_frame(canvas, x - radius, y - radius, radius * 2, radius * 2);
            break;
        case ShapeTriangle:
        case ShapeCount:
        default:
            canvas_draw_triangle(canvas, x, y + radius, radius * 2, radius * 2,
                                 CanvasDirectionBottomToTop);
            break;
    }
}

void gurpil_render_start(Canvas *canvas) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, 9, AlignCenter, AlignCenter, "Gurpil");

    canvas_set_font(canvas, FontSecondary);
    for (int i = 0; i < CONTROLS_LEGEND_ROW_COUNT; i++) {
        ControlsLegendRow row = controls_legend_row(i);
        canvas_draw_str(canvas, row.label_x, row.y, CONTROLS_LEGEND_ENTRIES[i].label);
        ShapeId shape = shape_for_input_key(CONTROLS_LEGEND_ENTRIES[i].key);
        render_legend_glyph(canvas, row.glyph_x, row.y - LEGEND_GLYPH_RADIUS, shape);
    }

    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, GURPIL_SCREEN_HEIGHT - 4, AlignCenter,
                            AlignCenter, "OK: start");
}

void gurpil_render(Canvas *canvas, const GameState *game, int32_t best, uint32_t frame) {
    canvas_clear(canvas);

    render_terrain(canvas, game);
    render_vehicle(canvas, game, frame);
    render_hud(canvas, game);

    if (game_is_over(game)) {
        render_game_over(canvas, game, best);
    }
}
