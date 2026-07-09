#include "include/app/gurpil_app.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

// The furi isolation boundary for this app: everything below `app/` stays
// plain C so it host-tests with gcc; only this entry file talks to furi/gui.
typedef struct {
    Gui *gui;
    ViewPort *view_port;
    FuriMessageQueue *input_queue;
    bool running;
} GurpilApp;

static void render_cb(Canvas *canvas, void *ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Gurpil");
}

static void input_cb(InputEvent *event, void *ctx) {
    GurpilApp *app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static GurpilApp *gurpil_app_alloc(void) {
    GurpilApp *app = malloc(sizeof(GurpilApp));
    furi_check(app != NULL);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->running = true;

    view_port_draw_callback_set(app->view_port, render_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port); // force the first frame here; never rely on the
                                      // Apps-menu loader animation to paint it

    return app;
}

static void gurpil_app_free(GurpilApp *app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t gurpil_app_run(void) {
    GurpilApp *app = gurpil_app_alloc();

    InputEvent event;
    while (app->running) {
        if (furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if (event.type == InputTypeShort && event.key == InputKeyBack) {
                app->running = false;
            }
        }
    }

    gurpil_app_free(app);
    return 0;
}
