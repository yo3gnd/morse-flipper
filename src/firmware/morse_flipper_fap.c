#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#define MORSE_FLIPPER_VOLUME 0.7f
#define MORSE_FLIPPER_POLL_MS 20
#define MORSE_FLIPPER_PREVIEW_TICKS 8
#define MORSE_FLIPPER_CONFIG_PATH APP_DATA_PATH("config.bin")
#define MORSE_FLIPPER_CONFIG_VERSION 1

static const GpioPin* morse_flipper_straight_pin = &gpio_ext_pa6;
static const GpioPin* morse_flipper_dit_pin = &gpio_ext_pb3;
static const GpioPin* morse_flipper_dah_pin = &gpio_ext_pc3;

typedef struct {
    const char* name;
    float hz;
} MorseFlipperTone;

typedef enum {
    MorseFlipperModeStraight = 1,
} MorseFlipperMode;

typedef enum {
    MorseFlipperHandednessNormal = 0,
    MorseFlipperHandednessSwapped = 1,
} MorseFlipperHandedness;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t spare0;
    uint8_t spare1;
} MorseFlipperConfig;

static const MorseFlipperTone morse_flipper_tones[] = {
    {"G2", 98.00f},
    {"A2", 110.00f},
    {"B2", 123.47f},
    {"C3", 130.81f},
    {"D3", 146.83f},
    {"E3", 164.81f},
    {"F3", 174.61f},
    {"G3", 196.00f},
    {"A3", 220.00f},
    {"B3", 246.94f},
    {"C4", 261.63f},
    {"D4", 293.66f},
    {"E4", 329.63f},
    {"F4", 349.23f},
    {"G4", 392.00f},
    {"A4", 440.00f},
    {"B4", 493.88f},
    {"C5", 523.25f},
    {"D5", 587.33f},
    {"E5", 659.25f},
    {"F5", 698.46f},
    {"G5", 783.99f},
    {"A5", 880.00f},
    {"B5", 987.77f},
    {"C6", 1046.50f},
    {"D6", 1174.66f},
    {"E6", 1318.51f},
    {"F6", 1396.91f},
    {"G6", 1567.98f},
    {"A6", 1760.00f},
    {"B6", 1975.53f},
};

typedef struct {
    FuriMessageQueue* q;
    ViewPort* view_port;
    Gui* gui;
    volatile bool exit_requested;
    bool tone_on;
    bool speaker_owned;
    bool speaker_busy;
    bool ok_down;
    uint8_t handedness;
    uint8_t keyer_mode;
    uint8_t tone_idx;
    uint8_t preview_ticks;
    uint8_t input_mask;
} MorseFlipperApp;

static const MorseFlipperTone* morse_flipper_current_tone(MorseFlipperApp* app) {
    return &morse_flipper_tones[app->tone_idx];
}

static const char* morse_flipper_handedness_line(MorseFlipperApp* app) {
    if(app->handedness == MorseFlipperHandednessSwapped) {
        return "p5 dah p7 dit";
    }

    return "p5 dit p7 dah";
}

static void morse_flipper_load_config(MorseFlipperApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperConfig config;

    if(storage_file_open(file, MORSE_FLIPPER_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &config, sizeof(config)) == sizeof(config) &&
           config.version == MORSE_FLIPPER_CONFIG_VERSION) {
            if(config.tone_idx < COUNT_OF(morse_flipper_tones)) {
                app->tone_idx = config.tone_idx;
            }

            if(config.keyer_mode >= MorseFlipperModeStraight) {
                app->keyer_mode = config.keyer_mode;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void morse_flipper_save_config(MorseFlipperApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperConfig config = {
        .version = MORSE_FLIPPER_CONFIG_VERSION,
        .tone_idx = app->tone_idx,
        .keyer_mode = app->keyer_mode,
        .spare0 = 0,
        .spare1 = 0,
    };

    if(storage_file_open(file, MORSE_FLIPPER_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &config, sizeof(config));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void morse_flipper_gpio_init(void) {
    furi_hal_gpio_init(morse_flipper_straight_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
}

static void morse_flipper_gpio_deinit(void) {
    furi_hal_gpio_init(morse_flipper_straight_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

static bool morse_flipper_straight_down(void) {
    return !furi_hal_gpio_read(morse_flipper_straight_pin);
}

static bool morse_flipper_dit_down(void) {
    return !furi_hal_gpio_read(morse_flipper_dit_pin);
}

static bool morse_flipper_dah_down(void) {
    return !furi_hal_gpio_read(morse_flipper_dah_pin);
}

static uint8_t morse_flipper_read_input_mask(MorseFlipperApp* app) {
    uint8_t mask = 0;

    if(app->ok_down) mask |= 1 << 0;
    if(morse_flipper_straight_down()) mask |= 1 << 1;
    if(morse_flipper_dit_down()) mask |= 1 << 2;
    if(morse_flipper_dah_down()) mask |= 1 << 3;

    return mask;
}

static const char* morse_flipper_input_line(MorseFlipperApp* app, char* buf, size_t buf_sz) {
    size_t n = snprintf(buf, buf_sz, "raw ");

    if(app->input_mask & (1 << 0)) n += snprintf(buf + n, buf_sz - n, "ok ");
    if(app->input_mask & (1 << 1)) n += snprintf(buf + n, buf_sz - n, "p3 ");
    if(app->input_mask & (1 << 2)) n += snprintf(buf + n, buf_sz - n, "p5 ");
    if(app->input_mask & (1 << 3)) n += snprintf(buf + n, buf_sz - n, "p7 ");

    if(n == 4) snprintf(buf, buf_sz, "raw -");

    return buf;
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

    furi_hal_speaker_start(morse_flipper_current_tone(app)->hz, MORSE_FLIPPER_VOLUME);
    app->tone_on = true;
    app->speaker_busy = false;
}

static void morse_flipper_tone_nudge(MorseFlipperApp* app, int dir) {
    int idx = (int)app->tone_idx + dir;

    if(idx < 0) idx = 0;
    if(idx >= (int)COUNT_OF(morse_flipper_tones)) idx = COUNT_OF(morse_flipper_tones) - 1;
    if(idx == (int)app->tone_idx) return;

    app->tone_idx = idx;
    app->preview_ticks = MORSE_FLIPPER_PREVIEW_TICKS;

    if(app->tone_on && app->speaker_owned && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_start(morse_flipper_current_tone(app)->hz, MORSE_FLIPPER_VOLUME);
    }

    morse_flipper_save_config(app);
    view_port_update(app->view_port);
}

static void morse_flipper_sync_tone(MorseFlipperApp* app) {
    bool want_tone = app->ok_down || morse_flipper_straight_down() || morse_flipper_dit_down() ||
                     morse_flipper_dah_down() || (app->preview_ticks > 0);
    bool old_tone = app->tone_on;
    bool old_busy = app->speaker_busy;
    uint8_t old_mask = app->input_mask;

    app->input_mask = morse_flipper_read_input_mask(app);

    if(want_tone && !app->tone_on) {
        morse_flipper_tone_start(app);
    } else if(!want_tone && app->tone_on) {
        morse_flipper_tone_stop(app);
    }

    if(old_tone != app->tone_on || old_busy != app->speaker_busy || old_mask != app->input_mask) {
        view_port_update(app->view_port);
    }
}

static void morse_flipper_draw(Canvas* canvas, void* ctx) {
    MorseFlipperApp* app = ctx;
    char tone_line[16];
    char input_line[24];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 14, "Morse Flipper");

    snprintf(tone_line, sizeof(tone_line), "< %s >", morse_flipper_current_tone(app)->name);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, app->tone_on ? "tone on" : "tone off");
    canvas_draw_str(canvas, 8, 42, tone_line);
    canvas_draw_str(canvas, 8, 52, morse_flipper_input_line(app, input_line, sizeof(input_line)));
    canvas_draw_str(
        canvas,
        8,
        62,
        app->speaker_busy ? "speaker busy" : morse_flipper_handedness_line(app));
}

static void morse_flipper_input(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* q = ctx;

    furi_message_queue_put(q, input_event, FuriWaitForever);
}

static bool morse_flipper_signal_callback(uint32_t signal, void* arg, void* ctx) {
    UNUSED(arg);
    MorseFlipperApp* app = ctx;

    if(signal == FuriSignalExit) {
        app->exit_requested = true;
        return true;
    }

    return false;
}

int32_t morse_flipper_fap(void* p) {
    UNUSED(p);

    MorseFlipperApp app = {
        .q = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .view_port = view_port_alloc(),
        .gui = furi_record_open(RECORD_GUI),
        .exit_requested = false,
        .tone_on = false,
        .speaker_owned = false,
        .speaker_busy = false,
        .ok_down = false,
        .handedness = MorseFlipperHandednessNormal,
        .keyer_mode = MorseFlipperModeStraight,
        .tone_idx = 0,
        .preview_ticks = 0,
        .input_mask = 0,
    };

    morse_flipper_load_config(&app);
    morse_flipper_gpio_init();
    furi_thread_set_signal_callback(
        furi_thread_get_current(), morse_flipper_signal_callback, &app);
    view_port_draw_callback_set(app.view_port, morse_flipper_draw, &app);
    view_port_input_callback_set(app.view_port, morse_flipper_input, app.q);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;

    while(running && !app.exit_requested) {
        if(furi_message_queue_get(app.q, &event, MORSE_FLIPPER_POLL_MS) == FuriStatusOk) {
            if(event.key == InputKeyOk) {
                if(event.type == InputTypePress) app.ok_down = true;
                else if(event.type == InputTypeRelease) app.ok_down = false;
            }

            if(event.type == InputTypePress) {
                if(event.key == InputKeyLeft) morse_flipper_tone_nudge(&app, -1);
                else if(event.key == InputKeyRight) morse_flipper_tone_nudge(&app, 1);
            }

            if(event.key == InputKeyDown && event.type == InputTypeShort) {
                if(app.handedness == MorseFlipperHandednessNormal) {
                    app.handedness = MorseFlipperHandednessSwapped;
                } else {
                    app.handedness = MorseFlipperHandednessNormal;
                }

                view_port_update(app.view_port);
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
        if(app.preview_ticks > 0) app.preview_ticks--;
    }

    morse_flipper_tone_stop(&app);
    morse_flipper_save_config(&app);
    furi_thread_set_signal_callback(furi_thread_get_current(), NULL, NULL);

    morse_flipper_gpio_deinit();
    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_message_queue_free(app.q);
    furi_record_close(RECORD_GUI);

    return 0;
}
