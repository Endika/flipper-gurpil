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

    return 0;
}
