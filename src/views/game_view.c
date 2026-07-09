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

// HUD text baseline, screen px from the top; both corners share it so distance and timer sit
// on the same row.
enum {
    HUD_TEXT_Y = 7,
};

// HUD units: a bare number in each corner reads as ambiguous ("which one is the timer?"), so
// each value carries its unit suffix — distance in meters (left), time left in seconds (right).
static const char *const HUD_DISTANCE_FORMAT = "%ldm";
static const char *const HUD_TIME_LEFT_FORMAT = "%lus";

// Speed bar: a small horizontal gauge that fills with game_speed_permille(game), so changing the
// mounted wheel shape visibly moves it — the mechanic was otherwise invisible. Placed in the top
// strip, a screen row band that is always sky (never ground): the tallest terrain
// (TERRAIN_HEIGHT_MAX) still draws no higher than screen row GURPIL_GROUND_BASELINE_Y -
// TERRAIN_HEIGHT_MAX == 23, so nothing here ever needs to erase ground behind it first.
enum {
    SPEED_BAR_X = 40,
    SPEED_BAR_Y = 10,
    SPEED_BAR_WIDTH = 48,
    SPEED_BAR_HEIGHT = 5,
    SPEED_BAR_INNER_MARGIN = 1,      // frame thickness the fill sits inside of, px.
    SPEED_BAR_PERMILLE_SCALE = 1000, // matches game_speed_permille's own 0..1000 output scale.
};

// Tutorial hint icon: the ideal shape for the upcoming terrain (game_hint_shape), shown inside a
// small highlight frame near the HUD while game_hint_active holds. Same guaranteed-sky band as
// the speed bar above.
enum {
    HINT_ICON_X = 8,
    HINT_ICON_Y = 17,
    HINT_ICON_RADIUS = 3,
    HINT_ICON_FRAME_MARGIN = 2, // padding between the glyph and its highlight frame, px.
};

// Checkpoint marker: a small flag at the next checkpoint's on-screen column, so the player can
// see how close the next time bonus is (and aim their shape choice at the terrain around it).
enum {
    CHECKPOINT_FLAG_HEIGHT = 10, // pole height above the terrain at the checkpoint's column, px.
    CHECKPOINT_FLAG_PENNANT_SIZE = 3, // pennant width/height at the pole's top, px.
};

// Checkpoint flash: a brief "+Ns" readout near the timer when a checkpoint was just crossed
// (show_checkpoint_flash, driven by the caller's own countdown — see game_view.h). Reuses
// ENDLESS_CHECKPOINT_BONUS_MS (endless.h) rather than a duplicated literal, so the text always
// matches the actual bonus granted.
enum {
    CHECKPOINT_FLASH_Y = HUD_TEXT_Y + 9, // just below the timer readout, same corner.
    MS_PER_SECOND = 1000,
};
static const char *const CHECKPOINT_FLASH_FORMAT = "+%lds";

// Game-over overlay: an opaque panel, centered on screen, that first erases the busy scrolling
// terrain/vehicle behind it (canvas_draw_box in ColorWhite) before the outcome text is drawn on
// top in ColorBlack — the previous overlay drew straight over the silhouette and became
// unreadable whenever ground filled the row a text line sat on.
enum {
    GAME_OVER_PANEL_WIDTH = 104,
    GAME_OVER_PANEL_HEIGHT = 54,
    // Five stacked text rows, evenly spaced 10px apart; kept explicit (not a spacing macro) to
    // match this file's existing style, but the shared rhythm is intentional.
    GAME_OVER_TITLE_Y = 13,
    GAME_OVER_DISTANCE_Y = 23,
    GAME_OVER_BEST_Y = 33,
    GAME_OVER_NEW_BEST_Y = 43, // only drawn when the run's distance beat the pre-run best.
    GAME_OVER_PROMPT_Y = 53,
};

static const char *const GAME_OVER_TITLE_TEXT = "Game Over";
static const char *const GAME_OVER_DISTANCE_FORMAT = "Distance: %ldm";
static const char *const GAME_OVER_BEST_FORMAT = "Best: %ldm";
static const char *const GAME_OVER_NEW_BEST_TEXT = "New best!";
static const char *const GAME_OVER_PROMPT_TEXT = "OK: menu";

static void render_terrain(Canvas *canvas, const GameState *game) {
    int32_t distance = game_distance(game);
    for (int column = 0; column < GURPIL_SCREEN_WIDTH; column++) {
        int32_t world_x = screen_column_to_world_x(distance, column);
        TerrainSample sample = terrain_at(game->seed, world_x);
        uint8_t ground_y = terrain_height_to_screen_y(sample.height);
        canvas_draw_line(canvas, column, ground_y, column, GURPIL_SCREEN_HEIGHT - 1);
    }
}

// Draws a small flag (pole + pennant) at the next checkpoint's on-screen column, if it is
// currently ahead of the vehicle and still on screen. The pole always sits entirely above the
// terrain's own ground_y at that column (drawn by render_terrain above, running from ground_y
// down to the bottom row), so the two never overlap regardless of terrain height.
static void render_checkpoint_marker(Canvas *canvas, const GameState *game) {
    int32_t distance = game_distance(game);
    int32_t next_checkpoint = game->endless.next_checkpoint;
    int32_t column = GURPIL_VEHICLE_COLUMN + (next_checkpoint - distance);

    if (column < 0 || column >= GURPIL_SCREEN_WIDTH) {
        return; // checkpoint isn't on screen (yet).
    }

    TerrainSample sample = terrain_at(game->seed, next_checkpoint);
    uint8_t ground_y = terrain_height_to_screen_y(sample.height);
    int32_t pole_top_y = ground_y > CHECKPOINT_FLAG_HEIGHT ? ground_y - CHECKPOINT_FLAG_HEIGHT : 0;

    canvas_draw_line(canvas, column, pole_top_y, column, ground_y);
    canvas_draw_line(canvas, column, pole_top_y, column + CHECKPOINT_FLAG_PENNANT_SIZE,
                     pole_top_y + CHECKPOINT_FLAG_PENNANT_SIZE / 2);
    canvas_draw_line(canvas, column + CHECKPOINT_FLAG_PENNANT_SIZE,
                     pole_top_y + CHECKPOINT_FLAG_PENNANT_SIZE / 2, column,
                     pole_top_y + CHECKPOINT_FLAG_PENNANT_SIZE);
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

    snprintf(text, sizeof(text), HUD_DISTANCE_FORMAT, (long)game_distance(game));
    canvas_draw_str(canvas, 1, HUD_TEXT_Y, text);

    snprintf(text, sizeof(text), HUD_TIME_LEFT_FORMAT,
             (unsigned long)(game_time_left(game) / 1000));
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH - 1, HUD_TEXT_Y, AlignRight, AlignBottom,
                            text);
}

// A framed gauge that fills with game_speed_permille(game) — the current speed as a fraction of
// the fastest attainable speed. Mounting a shape that matches the terrain ahead should visibly
// push the fill toward full; a mismatched/stalled shape should visibly drain it toward empty.
static void render_speed_bar(Canvas *canvas, const GameState *game) {
    uint16_t permille = game_speed_permille(game);
    int32_t inner_width = SPEED_BAR_WIDTH - 2 * SPEED_BAR_INNER_MARGIN;
    int32_t fill_width = (inner_width * (int32_t)permille) / SPEED_BAR_PERMILLE_SCALE;

    canvas_draw_frame(canvas, SPEED_BAR_X, SPEED_BAR_Y, SPEED_BAR_WIDTH, SPEED_BAR_HEIGHT);
    if (fill_width > 0) {
        canvas_draw_box(canvas, SPEED_BAR_X + SPEED_BAR_INNER_MARGIN,
                        SPEED_BAR_Y + SPEED_BAR_INNER_MARGIN, fill_width,
                        SPEED_BAR_HEIGHT - 2 * SPEED_BAR_INNER_MARGIN);
    }
}

// The tutorial shape hint: while game_hint_active holds, draws the ideal upcoming shape
// (game_hint_shape) as a small glyph inside a highlight frame, so a new player has a concrete
// "use THIS wheel" answer instead of guessing. Reuses the same per-shape silhouettes as
// render_wheel, at a small fixed size, with no spin (it's a static suggestion, not the vehicle).
static void render_hint_icon(Canvas *canvas, const GameState *game) {
    if (!game_hint_active(game)) {
        return;
    }

    int32_t cx = HINT_ICON_X;
    int32_t cy = HINT_ICON_Y;
    int32_t r = HINT_ICON_RADIUS;

    switch (game_hint_shape(game)) {
        case ShapeCircle:
            canvas_draw_circle(canvas, cx, cy, r);
            break;
        case ShapeLine:
            canvas_draw_line(canvas, cx - r, cy, cx + r, cy);
            break;
        case ShapeSquare:
            canvas_draw_frame(canvas, cx - r, cy - r, r * 2, r * 2);
            break;
        case ShapeTriangle:
        case ShapeCount:
        default:
            canvas_draw_triangle(canvas, cx, cy + r, r * 2, r * 2, CanvasDirectionBottomToTop);
            break;
    }

    int32_t margin = HINT_ICON_FRAME_MARGIN;
    canvas_draw_frame(canvas, cx - r - margin, cy - r - margin, (r + margin) * 2, (r + margin) * 2);
}

// A brief "+Ns" readout near the timer, shown while the caller's own flash countdown
// (show_checkpoint_flash) is still running — see game_view.h's module comment for why the
// countdown itself lives in the caller rather than here.
static void render_checkpoint_flash(Canvas *canvas) {
    char text[8];

    canvas_set_font(canvas, FontSecondary);
    snprintf(text, sizeof(text), CHECKPOINT_FLASH_FORMAT,
             (long)(ENDLESS_CHECKPOINT_BONUS_MS / MS_PER_SECOND));
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH - 1, CHECKPOINT_FLASH_Y, AlignRight,
                            AlignBottom, text);
}

static void render_game_over(Canvas *canvas, const GameState *game, int32_t best,
                             bool is_new_best) {
    char text[24];

    int32_t panel_x = (GURPIL_SCREEN_WIDTH - GAME_OVER_PANEL_WIDTH) / 2;
    int32_t panel_y = (GURPIL_SCREEN_HEIGHT - GAME_OVER_PANEL_HEIGHT) / 2;

    // Erase the terrain/vehicle behind the panel, then frame it, before any text is drawn.
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, panel_x, panel_y, GAME_OVER_PANEL_WIDTH, GAME_OVER_PANEL_HEIGHT);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, panel_x, panel_y, GAME_OVER_PANEL_WIDTH, GAME_OVER_PANEL_HEIGHT);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, GAME_OVER_TITLE_Y, AlignCenter,
                            AlignCenter, GAME_OVER_TITLE_TEXT);

    canvas_set_font(canvas, FontSecondary);

    snprintf(text, sizeof(text), GAME_OVER_DISTANCE_FORMAT, (long)game_distance(game));
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, GAME_OVER_DISTANCE_Y, AlignCenter,
                            AlignCenter, text);

    snprintf(text, sizeof(text), GAME_OVER_BEST_FORMAT, (long)best);
    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, GAME_OVER_BEST_Y, AlignCenter,
                            AlignCenter, text);

    if (is_new_best) {
        canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, GAME_OVER_NEW_BEST_Y, AlignCenter,
                                AlignCenter, GAME_OVER_NEW_BEST_TEXT);
    }

    canvas_draw_str_aligned(canvas, GURPIL_SCREEN_WIDTH / 2, GAME_OVER_PROMPT_Y, AlignCenter,
                            AlignCenter, GAME_OVER_PROMPT_TEXT);
}

void gurpil_render(Canvas *canvas, const GameState *game, int32_t best, uint32_t frame,
                   bool show_checkpoint_flash, bool is_new_best) {
    canvas_clear(canvas);

    render_terrain(canvas, game);
    render_checkpoint_marker(canvas, game);
    render_vehicle(canvas, game, frame);
    render_hud(canvas, game);
    render_speed_bar(canvas, game);
    render_hint_icon(canvas, game);
    if (show_checkpoint_flash) {
        render_checkpoint_flash(canvas);
    }

    if (game_is_over(game)) {
        render_game_over(canvas, game, best, is_new_best);
    }
}
