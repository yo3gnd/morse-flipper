#include <furi.h>

#include <gui/gui.h>
#include <input/input.h>

static void morse_flipper_draw(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 14, "Morse Flipper");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, "shell only for now");
    canvas_draw_str(canvas, 8, 42, "center tone next");
    canvas_draw_str(canvas, 8, 60, "back exits");
}

static void morse_flipper_input(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* q = ctx;

    furi_message_queue_put(q, input_event, FuriWaitForever);
}

int32_t morse_flipper_fap(void* p) {
    UNUSED(p);

    FuriMessageQueue* q = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);

    view_port_draw_callback_set(view_port, morse_flipper_draw, NULL);
    view_port_input_callback_set(view_port, morse_flipper_input, q);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;

    while(running) {
        if(furi_message_queue_get(q, &event, FuriWaitForever) != FuriStatusOk) continue;

        if(event.key != InputKeyBack) continue;

        if(event.type == InputTypePress || event.type == InputTypeShort || event.type == InputTypeLong) {
            running = false;
        }
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(q);
    furi_record_close(RECORD_GUI);

    return 0;
}

