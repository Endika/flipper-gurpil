#include "include/ui/render_map.h"

#include "include/domain/shapes.h"
#include "include/domain/terrain.h"

#include <assert.h>
#include <stdio.h>

static void test_world_x_centers_on_vehicle_column(void) {
    // At the vehicle's own column, world x is exactly the current distance, regardless of how
    // far the run has travelled.
    assert(screen_column_to_world_x(0, GURPIL_VEHICLE_COLUMN) == 0);
    assert(screen_column_to_world_x(500, GURPIL_VEHICLE_COLUMN) == 500);
}

static void test_world_x_scrolls_with_column(void) {
    // Columns right of the vehicle look ahead; columns left of it look behind.
    assert(screen_column_to_world_x(500, GURPIL_VEHICLE_COLUMN + 10) == 510);
    assert(screen_column_to_world_x(500, GURPIL_VEHICLE_COLUMN - 10) == 490);
}

static void test_world_x_can_go_negative_near_run_start(void) {
    // terrain_at documents that it clamps negative x to 0, so this is a valid, expected input
    // right after game_start when early columns look behind x=0.
    assert(screen_column_to_world_x(0, 0) < 0);
}

static void test_height_to_screen_y_at_the_bounds(void) {
    assert(terrain_height_to_screen_y(TERRAIN_HEIGHT_MIN) == GURPIL_GROUND_BASELINE_Y);
    assert(terrain_height_to_screen_y(TERRAIN_HEIGHT_MAX) ==
           GURPIL_GROUND_BASELINE_Y - TERRAIN_HEIGHT_MAX);
}

static void test_height_to_screen_y_is_monotonic_decreasing(void) {
    // Taller ground must draw strictly higher up the screen (smaller y) than shorter ground.
    uint8_t y_low = terrain_height_to_screen_y(5);
    uint8_t y_high = terrain_height_to_screen_y(30);
    assert(y_high < y_low);
}

static void test_height_to_screen_y_clamps_out_of_range(void) {
    assert(terrain_height_to_screen_y(-10) == terrain_height_to_screen_y(TERRAIN_HEIGHT_MIN));
    assert(terrain_height_to_screen_y(1000) == terrain_height_to_screen_y(TERRAIN_HEIGHT_MAX));
}

static void test_shape_for_input_key_matches_design(void) {
    assert(shape_for_input_key(GurpilKeyUp) == ShapeCircle);
    assert(shape_for_input_key(GurpilKeyRight) == ShapeLine);
    assert(shape_for_input_key(GurpilKeyDown) == ShapeSquare);
    assert(shape_for_input_key(GurpilKeyLeft) == ShapeTriangle);
}

static void test_shape_for_input_key_non_shape_keys_are_invalid(void) {
    // ShapeCount is the same "out of range, ignore me" sentinel game_set_shape already guards
    // against, so callers can wire this straight through without an extra branch.
    assert(shape_for_input_key(GurpilKeyOk) == ShapeCount);
    assert(shape_for_input_key(GurpilKeyBack) == ShapeCount);
    assert(shape_for_input_key(GurpilKeyOther) == ShapeCount);
}

static void test_wheel_rotation_step_holds_then_advances(void) {
    // Each step is held for WHEEL_ROTATION_TICKS_PER_STEP frames before advancing.
    assert(wheel_rotation_step(0) == 0);
    assert(wheel_rotation_step(WHEEL_ROTATION_TICKS_PER_STEP - 1) == 0);
    assert(wheel_rotation_step(WHEEL_ROTATION_TICKS_PER_STEP) == 1);
}

static void test_wheel_rotation_step_wraps_after_a_full_spin(void) {
    uint32_t frames_per_spin = WHEEL_ROTATION_STEP_COUNT * WHEEL_ROTATION_TICKS_PER_STEP;
    assert(wheel_rotation_step(frames_per_spin) == wheel_rotation_step(0));
    assert(wheel_rotation_step(frames_per_spin + 1) == wheel_rotation_step(1));
}

static void test_wheel_spoke_endpoint_starts_pointing_right(void) {
    int32_t dx = 0, dy = 0;
    wheel_spoke_endpoint(0, 4, &dx, &dy);
    assert(dx == 4);
    assert(dy == 0);
}

static void test_wheel_spoke_endpoint_stays_within_radius(void) {
    // Every step's offset must stay within the wheel's own radius in both axes, whatever the
    // angle — a spoke that overshoots the glyph it is meant to sit inside would look broken.
    for (uint32_t frame = 0; frame < WHEEL_ROTATION_STEP_COUNT * WHEEL_ROTATION_TICKS_PER_STEP;
         frame++) {
        int32_t dx = 0, dy = 0;
        wheel_spoke_endpoint(frame, 4, &dx, &dy);
        assert(dx >= -4 && dx <= 4);
        assert(dy >= -4 && dy <= 4);
    }
}

static void test_wheel_spoke_endpoint_traces_a_full_rotation(void) {
    // Stepping through one whole spin's worth of frames must visit every discrete direction
    // exactly once before the offset repeats.
    int32_t seen_dx[WHEEL_ROTATION_STEP_COUNT];
    int32_t seen_dy[WHEEL_ROTATION_STEP_COUNT];
    for (uint32_t step = 0; step < WHEEL_ROTATION_STEP_COUNT; step++) {
        wheel_spoke_endpoint(step * WHEEL_ROTATION_TICKS_PER_STEP, 4, &seen_dx[step],
                             &seen_dy[step]);
    }
    for (uint32_t a = 0; a < WHEEL_ROTATION_STEP_COUNT; a++) {
        for (uint32_t b = a + 1; b < WHEEL_ROTATION_STEP_COUNT; b++) {
            assert(seen_dx[a] != seen_dx[b] || seen_dy[a] != seen_dy[b]);
        }
    }
}

static void test_controls_legend_rows_are_evenly_spaced_and_ordered(void) {
    for (int i = 1; i < CONTROLS_LEGEND_ROW_COUNT; i++) {
        ControlsLegendRow previous = controls_legend_row(i - 1);
        ControlsLegendRow current = controls_legend_row(i);
        assert(current.y - previous.y == CONTROLS_LEGEND_ROW_SPACING);
        assert(current.label_x == previous.label_x);
        assert(current.glyph_x == previous.glyph_x);
    }
}

static void test_controls_legend_rows_fit_on_screen(void) {
    ControlsLegendRow last = controls_legend_row(CONTROLS_LEGEND_ROW_COUNT - 1);
    assert(last.y < GURPIL_SCREEN_HEIGHT);
    assert(last.glyph_x < GURPIL_SCREEN_WIDTH);
}

int main(void) {
    test_world_x_centers_on_vehicle_column();
    printf("test_world_x_centers_on_vehicle_column: PASS\n");

    test_world_x_scrolls_with_column();
    printf("test_world_x_scrolls_with_column: PASS\n");

    test_world_x_can_go_negative_near_run_start();
    printf("test_world_x_can_go_negative_near_run_start: PASS\n");

    test_height_to_screen_y_at_the_bounds();
    printf("test_height_to_screen_y_at_the_bounds: PASS\n");

    test_height_to_screen_y_is_monotonic_decreasing();
    printf("test_height_to_screen_y_is_monotonic_decreasing: PASS\n");

    test_height_to_screen_y_clamps_out_of_range();
    printf("test_height_to_screen_y_clamps_out_of_range: PASS\n");

    test_shape_for_input_key_matches_design();
    printf("test_shape_for_input_key_matches_design: PASS\n");

    test_shape_for_input_key_non_shape_keys_are_invalid();
    printf("test_shape_for_input_key_non_shape_keys_are_invalid: PASS\n");

    test_wheel_rotation_step_holds_then_advances();
    printf("test_wheel_rotation_step_holds_then_advances: PASS\n");

    test_wheel_rotation_step_wraps_after_a_full_spin();
    printf("test_wheel_rotation_step_wraps_after_a_full_spin: PASS\n");

    test_wheel_spoke_endpoint_starts_pointing_right();
    printf("test_wheel_spoke_endpoint_starts_pointing_right: PASS\n");

    test_wheel_spoke_endpoint_stays_within_radius();
    printf("test_wheel_spoke_endpoint_stays_within_radius: PASS\n");

    test_wheel_spoke_endpoint_traces_a_full_rotation();
    printf("test_wheel_spoke_endpoint_traces_a_full_rotation: PASS\n");

    test_controls_legend_rows_are_evenly_spaced_and_ordered();
    printf("test_controls_legend_rows_are_evenly_spaced_and_ordered: PASS\n");

    test_controls_legend_rows_fit_on_screen();
    printf("test_controls_legend_rows_fit_on_screen: PASS\n");

    return 0;
}
