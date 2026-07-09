#pragma once

#include "include/domain/shapes.h"

#include <stdint.h>

/*
 * Pure screen-mapping and input-mapping helpers for the Canvas UI layer (no furi, no I/O),
 * host-testable in isolation from Canvas/InputEvent. src/views/game_view.c (the furi-facing
 * render callback) and the Task 11 app's input callback are the only places these get wired to
 * real hardware types.
 *
 * Single source of truth for the 128x64 screen layout: the vehicle's fixed column and the
 * ground baseline row both live here so the terrain/vehicle renderer and any test stay in sync.
 */

enum {
    GURPIL_SCREEN_WIDTH = 128, // display width, px.
    GURPIL_SCREEN_HEIGHT = 64, // display height, px.

    GURPIL_VEHICLE_COLUMN = 42, // fixed screen column the vehicle sits at (~1/3 from left);
                                // the terrain scrolls under it as the game distance grows.

    GURPIL_GROUND_BASELINE_Y = 63, // screen y a height-0 ground column draws at (bottom row);
                                   // taller ground (up to TERRAIN_HEIGHT_MAX) draws higher up.
};

/* Maps a screen column (0..GURPIL_SCREEN_WIDTH-1) to the world x sampled by terrain_at, given
 * the current integer game distance: GURPIL_VEHICLE_COLUMN always shows the terrain at
 * `distance`, columns to its right show terrain ahead, columns to its left show terrain
 * already passed. Negative results are valid inputs to terrain_at (it clamps them to 0). */
int32_t screen_column_to_world_x(int32_t distance, int column);

/* Maps a terrain height (clamped to TERRAIN_HEIGHT_MIN..TERRAIN_HEIGHT_MAX game units, see
 * domain/terrain.h) to a screen y: height 0 draws at GURPIL_GROUND_BASELINE_Y, taller ground
 * draws higher up the screen (smaller y). Clamps defensively even though terrain_at already
 * guarantees its samples stay in range. */
uint8_t terrain_height_to_screen_y(int16_t height);

/* Abstraction over the D-pad keys this game reacts to, decoupled from furi's InputKey so this
 * mapping stays host-testable; the input callback (furi-aware, Task 11) translates the real
 * InputKey into this enum before calling shape_for_input_key. */
typedef enum {
    GurpilKeyUp,
    GurpilKeyRight,
    GurpilKeyDown,
    GurpilKeyLeft,
    GurpilKeyOk,
    GurpilKeyBack,
    GurpilKeyOther,
} GurpilKey;

/* Maps a D-pad direction to the wheel shape it mounts: Up->Circle, Right->Line, Down->Square,
 * Left->Triangle. Returns ShapeCount — an invalid shape, per game_set_shape's own out-of-range
 * guard — for keys that don't select a shape (Ok/Back/Other), so callers can pass the result
 * straight to game_set_shape without an extra branch. */
ShapeId shape_for_input_key(GurpilKey key);

/*
 * Wheel-spin animation stepper (no furi, no Canvas): the app passes a per-tick `frame` counter
 * into gurpil_render so the mounted wheel glyph visibly rotates instead of sitting still. Pure
 * integer math, no trig library — a fixed 8-direction lookup table stands in for angle steps.
 */

enum {
    WHEEL_ROTATION_STEP_COUNT = 8,     // discrete angles a full spin steps through.
    WHEEL_ROTATION_TICKS_PER_STEP = 2, // sim ticks each angle is held; lower spins faster.
};

/* Returns the current discrete rotation step (0..WHEEL_ROTATION_STEP_COUNT-1) for `frame`, the
 * app's per-tick counter. Wraps every WHEEL_ROTATION_STEP_COUNT * WHEEL_ROTATION_TICKS_PER_STEP
 * frames. Shape glyphs that approximate rotation with a handful of discrete orientations
 * (square, triangle) index into their own smaller cycle with this same step. */
uint32_t wheel_rotation_step(uint32_t frame);

/* Computes the (dx, dy) offset from a wheel's center to the tip of a spinning spoke/diameter
 * marker of `radius` screen px, for the rotation step `wheel_rotation_step(frame)` selects.
 * Used to draw a rotating spoke on the circle glyph, or a rotating diameter bar on the line
 * glyph. */
void wheel_spoke_endpoint(uint32_t frame, int32_t radius, int32_t *dx, int32_t *dy);

/*
 * Start-screen controls-legend layout (no furi, no Canvas): the four D-pad-to-shape rows are
 * evenly spaced so they, plus the title and the "OK: start" prompt, all fit on the 128x64
 * display without the app layer hardcoding pixel positions inline.
 */

enum {
    CONTROLS_LEGEND_ROW_COUNT = 4,   // one row per D-pad direction that mounts a shape.
    CONTROLS_LEGEND_FIRST_Y = 24,    // baseline y of the first (Up) row.
    CONTROLS_LEGEND_ROW_SPACING = 9, // baseline-to-baseline gap between rows, px.
    CONTROLS_LEGEND_LABEL_X = 6,     // left x of the direction label text.
    CONTROLS_LEGEND_GLYPH_X = 76,    // center x of the tiny shape glyph.
};

typedef struct {
    int32_t label_x; // x to draw the direction label at (e.g. "Up").
    int32_t glyph_x; // x to center the tiny shape glyph at.
    int32_t y;       // shared baseline y for both.
} ControlsLegendRow;

/* Returns the layout for legend row `index` (0..CONTROLS_LEGEND_ROW_COUNT-1, in Up/Right/Down/
 * Left order matching shape_for_input_key). Out-of-range indices still return a position (no
 * clamping) — the caller only ever loops 0..CONTROLS_LEGEND_ROW_COUNT-1. */
ControlsLegendRow controls_legend_row(int index);
