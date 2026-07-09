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

// One entry per WHEEL_ROTATION_STEP_COUNT step, each a unit vector scaled by 1000 (so callers
// divide back down after multiplying by their radius) — 8 evenly-spaced angles around the
// circle, starting at 0 degrees (pointing right) and stepping counter-clockwise on screen.
typedef struct {
    int16_t dx1000;
    int16_t dy1000;
} WheelSpokeDirection;

static const WheelSpokeDirection WHEEL_SPOKE_DIRECTIONS[WHEEL_ROTATION_STEP_COUNT] = {
    {1000, 0},  {707, -707}, {0, -1000}, {-707, -707},
    {-1000, 0}, {-707, 707}, {0, 1000},  {707, 707},
};

uint32_t wheel_rotation_step(uint32_t frame) {
    return (frame / WHEEL_ROTATION_TICKS_PER_STEP) % WHEEL_ROTATION_STEP_COUNT;
}

void wheel_spoke_endpoint(uint32_t frame, int32_t radius, int32_t *dx, int32_t *dy) {
    const WheelSpokeDirection *direction = &WHEEL_SPOKE_DIRECTIONS[wheel_rotation_step(frame)];
    *dx = (radius * direction->dx1000) / 1000;
    *dy = (radius * direction->dy1000) / 1000;
}

ControlsLegendRow controls_legend_row(int index) {
    ControlsLegendRow row;
    row.label_x = CONTROLS_LEGEND_LABEL_X;
    row.glyph_x = CONTROLS_LEGEND_GLYPH_X;
    row.y = CONTROLS_LEGEND_FIRST_Y + index * CONTROLS_LEGEND_ROW_SPACING;
    return row;
}

int32_t footer_legend_slot_center_x(int slot_index) {
    return slot_index * FOOTER_LEGEND_SLOT_WIDTH + FOOTER_LEGEND_SLOT_WIDTH / 2;
}

FooterLegendCell footer_legend_cell(int index) {
    FooterLegendCell cell;
    cell.glyph_y = FOOTER_LEGEND_GLYPH_Y;
    cell.arrow_y = FOOTER_LEGEND_ARROW_Y;

    switch (index) {
        case 0: // Up
            cell.glyph_x = footer_legend_slot_center_x(0);
            cell.arrow_x = cell.glyph_x;
            cell.shape = ShapeCircle;
            break;
        case 1: // Right
            cell.glyph_x = footer_legend_slot_center_x(1);
            cell.arrow_x = cell.glyph_x;
            cell.shape = ShapeLine;
            break;
        case 2: // Down
            cell.glyph_x = footer_legend_slot_center_x(2);
            cell.arrow_x = cell.glyph_x;
            cell.shape = ShapeSquare;
            break;
        case 3: // Left
        default:
            cell.glyph_x = footer_legend_slot_center_x(3);
            cell.arrow_x = cell.glyph_x;
            cell.shape = ShapeTriangle;
            break;
    }
    return cell;
}
