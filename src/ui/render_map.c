#include "include/ui/render_map.h"

#include "include/domain/terrain.h"

int32_t screen_column_to_world_x(int32_t distance, int column) {
    return distance + (int32_t)(column - GURPIL_VEHICLE_COLUMN);
}

uint8_t terrain_height_to_screen_y(int16_t height) {
    if (height < TERRAIN_HEIGHT_MIN) {
        height = TERRAIN_HEIGHT_MIN;
    } else if (height > TERRAIN_HEIGHT_MAX) {
        height = TERRAIN_HEIGHT_MAX;
    }
    return (uint8_t)(GURPIL_GROUND_BASELINE_Y - height);
}

ShapeId shape_for_input_key(GurpilKey key) {
    switch (key) {
        case GurpilKeyUp:
            return ShapeCircle;
        case GurpilKeyRight:
            return ShapeLine;
        case GurpilKeyDown:
            return ShapeSquare;
        case GurpilKeyLeft:
            return ShapeTriangle;
        case GurpilKeyOk:
        case GurpilKeyBack:
        case GurpilKeyOther:
        default:
            return ShapeCount;
    }
}
