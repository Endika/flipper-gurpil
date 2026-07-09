#pragma once

#include "include/application/game.h"

#include <gui/canvas.h>

/*
 * The furi isolation boundary for rendering: this header pulls in Canvas, so only furi-aware
 * callers (the Task 11 app layer) include it. GameState and its accessors stay plain C.
 *
 * `gurpil_render` is a pure function of `game` and `best` — it never mutates GameState and
 * never blocks on input, so the caller can (and must) invoke it on every tick via
 * view_port_update, not just in response to input. That is what avoids the blank-UI-until-
 * first-keypress bug seen when a FAP is launched from Favourites/quick buttons: the very first
 * frame must be paintable before any InputEvent ever arrives.
 *
 * Seam for the game-over score: `best` (the persisted best distance) is supplied by the caller
 * rather than stored on GameState, so best_store's furi Storage I/O never has to leak into the
 * pure application/domain layers — the app loads it once via best_store_load and passes it in
 * every frame.
 *
 * Seam for the wheel-spin animation: `frame`, a per-tick counter, is supplied by the caller
 * (rather than kept as static state in this file) so the spin stays deterministic and the app
 * fully controls when it advances (it must freeze once the run is over, and must not run at all
 * on the start screen — see gurpil_app.c).
 */

/* Draws one frame: the scrolling terrain silhouette, the rider-on-a-scooter vehicle with its
 * currently mounted wheel shape spinning per `frame`, a distance+timer HUD, and — once
 * game_is_over(game) — a game-over overlay showing the run's final distance alongside `best`. */
void gurpil_render(Canvas *canvas, const GameState *game, int32_t best, uint32_t frame);

/* Draws the pre-run start screen: the title, a legend mapping each D-pad direction to the wheel
 * shape it mounts, and an "OK: start" prompt. Never touches GameState — the endless timer must
 * not be ticking while this is on screen (see gurpil_app.c's AppScreenStart). */
void gurpil_render_start(Canvas *canvas);
