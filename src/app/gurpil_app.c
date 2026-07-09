#include "include/app/gurpil_app.h"

#include "include/application/game.h"
#include "include/persistence/best_store.h"
#include "include/platform/random_port.h"
#include "include/ui/render_map.h"
#include "include/views/game_view.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * The furi isolation boundary for this app: everything below `app/` stays plain C so it
 * host-tests with gcc; this is the only file (besides the already-furi platform/persistence/
 * ui/views files it calls into) that talks to furi/gui.
 *
 * Main-loop design — the Favourites/quick-buttons blank-UI gotcha (see also the module comment
 * in include/views/game_view.h): a FAP launched from Favourites never gets a synthetic input
 * event, so a redraw driven only by input_cb would leave the screen blank until the player's
 * first keypress. The fix is a FuriTimer firing every GURPIL_TICK_MS *regardless of input*:
 * each fire advances the sim by that fixed step and unconditionally calls view_port_update, so
 * the very first frame (forced right after gui_add_view_port, below) and every frame after it
 * paints on its own schedule. Input only mutates shared state (mounting a shape, restarting,
 * quitting) — it never drives ticking or redrawing itself, so the main loop can block
 * indefinitely on the input queue without starving the screen.
 *
 * `game`, `best` and `game_over_handled` are touched from three different contexts — the timer
 * service thread (tick), the app's own thread (input handling), and the GUI thread (the draw
 * callback) — so every access to them goes through `mutex`.
 *
 * `screen` gates the whole run: the app boots into AppScreenStart, where gurpil_render_start
 * shows the title, the controls legend and an "OK: start" prompt — and timer_cb does not call
 * game_tick at all, so the endless countdown is not yet live. Only leaving the start screen
 * (any D-pad key or Ok) calls start_new_run, which flips to AppScreenPlaying and is the one
 * place the countdown actually starts. From game-over, Ok returns to AppScreenStart rather than
 * restarting straight away, so the legend — and the fact that the timer is not running — is
 * always visible again before a new run's clock starts ticking.
 */

enum {
    GURPIL_TICK_MS = 33, // fixed sim step, ms (~30 FPS); also the FuriTimer period.
    GURPIL_INPUT_QUEUE_SIZE = 8,
};

typedef enum {
    AppScreenStart,   // title + controls legend; game_tick never runs here.
    AppScreenPlaying, // the run itself, including its game-over overlay.
} AppScreen;

typedef struct {
    Gui *gui;
    ViewPort *view_port;
    FuriMessageQueue *input_queue;
    FuriMutex *mutex;
    FuriTimer *timer;

    AppScreen screen;
    GameState game;
    uint32_t frame; // per-tick counter driving the wheel-spin animation; reset per run.
    int32_t best;
    bool game_over_handled; // true once the current run's game-over has been scored/saved, so
                            // the transition fires exactly once per run.
    bool running;
} GurpilApp;

static void render_cb(Canvas *canvas, void *ctx) {
    GurpilApp *app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if (app->screen == AppScreenStart) {
        gurpil_render_start(canvas);
    } else {
        gurpil_render(canvas, &app->game, app->best, app->frame);
    }
    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent *event, void *ctx) {
    GurpilApp *app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static GurpilKey gurpil_key_from_input(InputKey key) {
    switch (key) {
        case InputKeyUp:
            return GurpilKeyUp;
        case InputKeyDown:
            return GurpilKeyDown;
        case InputKeyLeft:
            return GurpilKeyLeft;
        case InputKeyRight:
            return GurpilKeyRight;
        case InputKeyOk:
            return GurpilKeyOk;
        case InputKeyBack:
            return GurpilKeyBack;
        default:
            return GurpilKeyOther;
    }
}

// Scores and persists the just-finished run exactly once. Called from timer_cb the tick the
// run's timer hits 0, under `app->mutex`.
static void handle_game_over_once(GurpilApp *app) {
    if (app->game_over_handled) {
        return;
    }
    int32_t distance = game_distance(&app->game);
    if (distance > app->best) {
        app->best = distance;
        best_store_save(app->best);
    }
    app->game_over_handled = true;
}

static void timer_cb(void *ctx) {
    GurpilApp *app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    // Ticking (and with it, the wheel-spin frame counter) is gated on AppScreenPlaying: the
    // endless countdown must not run at all while the start screen is up, and freezes — frame
    // included, so the wheel doesn't keep spinning behind the game-over overlay — once
    // game_is_over is true.
    if (app->screen == AppScreenPlaying && !game_is_over(&app->game)) {
        game_tick(&app->game, GURPIL_TICK_MS);
        app->frame++;
        if (game_is_over(&app->game)) {
            handle_game_over_once(app);
        }
    }
    furi_mutex_release(app->mutex);

    view_port_update(app->view_port); // redraw every tick, never only on input.
}

// Starts a brand-new run: fresh RNG seed, sim/endless/shape reset via game_start, animation
// frame counter back to 0. Caller holds `app->mutex`.
static void start_new_run(GurpilApp *app) {
    game_start(&app->game, platform_random_seed());
    app->frame = 0;
    app->game_over_handled = false;
}

// True for the D-pad/Ok keys that leave the start screen and launch a run; false for Back (which
// quits from anywhere, handled separately) and Other.
static bool gurpil_key_starts_run(GurpilKey key) {
    switch (key) {
        case GurpilKeyUp:
        case GurpilKeyRight:
        case GurpilKeyDown:
        case GurpilKeyLeft:
        case GurpilKeyOk:
            return true;
        case GurpilKeyBack:
        case GurpilKeyOther:
        default:
            return false;
    }
}

static void handle_input(GurpilApp *app, const InputEvent *event) {
    GurpilKey key = gurpil_key_from_input(event->key);

    if (event->type == InputTypePress) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if (app->screen == AppScreenStart) {
            if (gurpil_key_starts_run(key)) {
                start_new_run(app);
                app->screen = AppScreenPlaying;
            }
        } else {
            // Immediate feedback for shape selection: react on press, not on release+debounce
            // (InputTypeShort) — a reflex game can't afford that latency. shape_for_input_key
            // returns ShapeCount (ignored by game_set_shape's own range guard) for Ok/Back/
            // Other, so this is safe to call unconditionally for every key.
            game_set_shape(&app->game, shape_for_input_key(key));
        }
        furi_mutex_release(app->mutex);
        return;
    }

    if (event->type != InputTypeShort) {
        return;
    }

    if (key == GurpilKeyBack) {
        app->running = false;
        return;
    }

    if (key == GurpilKeyOk) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if (app->screen == AppScreenPlaying && game_is_over(&app->game)) {
            app->screen = AppScreenStart;
        }
        furi_mutex_release(app->mutex);
    }
}

static GurpilApp *gurpil_app_alloc(void) {
    GurpilApp *app = malloc(sizeof(GurpilApp));
    furi_check(app != NULL);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_check(app->mutex != NULL);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->input_queue = furi_message_queue_alloc(GURPIL_INPUT_QUEUE_SIZE, sizeof(InputEvent));
    app->running = true;

    app->screen = AppScreenStart;
    app->best = best_store_load();
    app->game_over_handled = false;
    app->frame = 0;
    game_start(&app->game, platform_random_seed()); // valid initial state; start_new_run reseeds
                                                    // it for real once the player leaves the
                                                    // start screen — see handle_input.

    view_port_draw_callback_set(app->view_port, render_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port); // force the first frame here; never rely on the
                                      // Apps-menu loader animation to paint it

    app->timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_ms_to_ticks(GURPIL_TICK_MS));

    return app;
}

static void gurpil_app_free(GurpilApp *app) {
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t gurpil_app_run(void) {
    GurpilApp *app = gurpil_app_alloc();

    InputEvent event;
    while (app->running) {
        // Blocks indefinitely: the FuriTimer above drives ticking/redrawing on its own
        // schedule, so waiting here for input never starves the screen.
        if (furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            handle_input(app, &event);
        }
    }

    gurpil_app_free(app);
    return 0;
}
