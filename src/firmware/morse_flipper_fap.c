#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>

#define MORSE_FLIPPER_TONE 440.0f
#define MORSE_FLIPPER_VOLUME 0.7f
#define MORSE_FLIPPER_POLL_MS 20

static const GpioPin* morse_flipper_key_pins[] = {
    &gpio_ext_pa6,
    &gpio_ext_pb2,
    &gpio_ext_pc3,
};

typedef struct {
    FuriMessageQueue* q;
    ViewPort* view_port;
    Gui* gui;
    bool tone_on;
    bool speaker_owned;
    bool speaker_busy;
    bool ok_down;
} MorseFlipperApp;

static void morse_flipper_gpio_init(void) {
    for(size_t i = 0; i < COUNT_OF(morse_flipper_key_pins); i++) {
        furi_hal_gpio_init(
            morse_flipper_key_pins[i], GpioModeInput, GpioPullUp, GpioSpeedLow);
    }
}

static void morse_flipper_gpio_deinit(void) {
    for(size_t i = 0; i < COUNT_OF(morse_flipper_key_pins); i++) {
        furi_hal_gpio_init(
            morse_flipper_key_pins[i], GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
}

static bool morse_flipper_gpio_down(void) {
    for(size_t i = 0; i < COUNT_OF(morse_flipper_key_pins); i++) {
        if(!furi_hal_gpio_read(morse_flipper_key_pins[i])) return true;
    }

    return false;
}

static void morse_flipper_tone_stop(MorseFlipperApp* app) {
    if(app->speaker_owned && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    } else if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    app->tone_on = false;
    app->speaker_owned = false;
    app->speaker_busy = false;
}

static void morse_flipper_tone_start(MorseFlipperApp* app) {
    if(app->tone_on) return;

    if(!app->speaker_owned) {
        if(furi_hal_speaker_acquire(30)) app->speaker_owned = true;
        else {
            app->speaker_busy = true;
            return;
        }
    }

    if(!furi_hal_speaker_is_mine()) {
        app->tone_on = false;
        app->speaker_owned = false;
        app->speaker_busy = true;
        return;
    }

    furi_hal_speaker_start(MORSE_FLIPPER_TONE, MORSE_FLIPPER_VOLUME);
    app->tone_on = true;
    app->speaker_busy = false;
}

static void morse_flipper_sync_tone(MorseFlipperApp* app) {
    bool want_tone = app->ok_down || morse_flipper_gpio_down();
    bool old_tone = app->tone_on;
    bool old_busy = app->speaker_busy;

    if(want_tone && !app->tone_on) {
        morse_flipper_tone_start(app);
    } else if(!want_tone && app->tone_on) {
        morse_flipper_tone_stop(app);
    }

    if(old_tone != app->tone_on || old_busy != app->speaker_busy) view_port_update(app->view_port);
}

static void morse_flipper_draw(Canvas* canvas, void* ctx) {
    MorseFlipperApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 14, "Morse Flipper");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, app->tone_on ? "tone on" : "tone off");
    canvas_draw_str(canvas, 8, 42, "ok p3 p6 p7 buzz");
    canvas_draw_str(canvas, 8, 52, app->speaker_busy ? "speaker busy" : "");
    canvas_draw_str(canvas, 8, 62, "back exits");
}

static void morse_flipper_input(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* q = ctx;

    furi_message_queue_put(q, input_event, FuriWaitForever);
}

int32_t morse_flipper_fap(void* p) {
    UNUSED(p);

    MorseFlipperApp app = {
        .q = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .view_port = view_port_alloc(),
        .gui = furi_record_open(RECORD_GUI),
        .tone_on = false,
        .speaker_owned = false,
        .speaker_busy = false,
        .ok_down = false,
    };

    morse_flipper_gpio_init();
    view_port_draw_callback_set(app.view_port, morse_flipper_draw, &app);
    view_port_input_callback_set(app.view_port, morse_flipper_input, app.q);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;

    while(running) {
        if(furi_message_queue_get(app.q, &event, MORSE_FLIPPER_POLL_MS) == FuriStatusOk) {
            if(event.key == InputKeyOk) {
                if(event.type == InputTypePress) app.ok_down = true;
                else if(event.type == InputTypeRelease) app.ok_down = false;
            }

            if(event.key == InputKeyBack) {
                if(event.type == InputTypePress || event.type == InputTypeShort ||
                   event.type == InputTypeLong) {
                    app.ok_down = false;
                    running = false;
                }
            }
        }

        morse_flipper_sync_tone(&app);
    }

    morse_flipper_tone_stop(&app);

    morse_flipper_gpio_deinit();
    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_message_queue_free(app.q);
    furi_record_close(RECORD_GUI);

    return 0;
}
