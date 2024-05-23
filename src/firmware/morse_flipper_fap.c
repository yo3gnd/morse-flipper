#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#include "keyer.h"
#include "pc_keys.h"
#include "trainer.h"
#include "usb/morse_usb_midi.h"

#define MORSE_FLIPPER_VOLUME 0.7f
#define MORSE_FLIPPER_POLL_MS 5
#define MORSE_FLIPPER_PREVIEW_TICKS 8
#define MORSE_FLIPPER_CONFIG_PATH APP_DATA_PATH("config.bin")
#define MORSE_FLIPPER_CONFIG_VERSION 1
#define MORSE_FLIPPER_DEFAULT_DIT_MS 100U

#define MORSE_SOURCE_STRAIGHT_GPIO (1UL << 0)
#define MORSE_SOURCE_STRAIGHT_BTN  (1UL << 1)
#define MORSE_SOURCE_KEYER_DIT     (1UL << 2)
#define MORSE_SOURCE_KEYER_DAH     (1UL << 3)

#define MORSE_PADDLE_SOURCE_GPIO_DIT (1UL << 0)
#define MORSE_PADDLE_SOURCE_GPIO_DAH (1UL << 1)
#define MORSE_PADDLE_SOURCE_BTN_OK   (1UL << 2)
#define MORSE_PADDLE_SOURCE_BTN_BACK (1UL << 3)

static const GpioPin* morse_flipper_straight_pin = &gpio_ext_pa6;
static const GpioPin* morse_flipper_dit_pin = &gpio_ext_pc3;
static const GpioPin* morse_flipper_dah_pin = &gpio_ext_pb3;
static const GpioPin* morse_flipper_ground_pin = &gpio_ext_pa7;

typedef struct {
    const char* name;
    float hz;
    uint8_t midi_note;
} MorseFlipperTone;

typedef enum {
    MorseFlipperHandednessNormal = 0,
    MorseFlipperHandednessSwapped = 1,
} MorseFlipperHandedness;

typedef enum {
    MorseFlipperInputSourceStraight = 0,
    MorseFlipperInputSourcePaddle = 1,
} MorseFlipperInputSource;

typedef enum {
    MorseFlipperScreenHome = 0,
    MorseFlipperScreenRun = 1,
    MorseFlipperScreenTrace = 2,
    MorseFlipperScreenPc = 3,
    MorseFlipperScreenPcKeys = 4,
    MorseFlipperScreenTrainer = 5,
} MorseFlipperScreen;

typedef enum {
    MorseFlipperPcModeOff = 0,
    MorseFlipperPcModeMidi = 1,
    MorseFlipperPcModeKeyboard = 2,
} MorseFlipperPcMode;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t spare0;
    uint8_t spare1;
} MorseFlipperConfig;

static const MorseFlipperTone morse_flipper_tones[] = {
    {"G2", 98.00f, 43U},
    {"A2", 110.00f, 45U},
    {"B2", 123.47f, 47U},
    {"C3", 130.81f, 48U},
    {"D3", 146.83f, 50U},
    {"E3", 164.81f, 52U},
    {"F3", 174.61f, 53U},
    {"G3", 196.00f, 55U},
    {"A3", 220.00f, 57U},
    {"B3", 246.94f, 59U},
    {"C4", 261.63f, 60U},
    {"D4", 293.66f, 62U},
    {"E4", 329.63f, 64U},
    {"F4", 349.23f, 65U},
    {"G4", 392.00f, 67U},
    {"A4", 440.00f, 69U},
    {"B4", 493.88f, 71U},
    {"C5", 523.25f, 72U},
    {"D5", 587.33f, 74U},
    {"E5", 659.25f, 76U},
    {"F5", 698.46f, 77U},
    {"G5", 783.99f, 79U},
    {"A5", 880.00f, 81U},
    {"B5", 987.77f, 83U},
    {"C6", 1046.50f, 84U},
    {"D6", 1174.66f, 86U},
    {"E6", 1318.51f, 88U},
    {"F6", 1396.91f, 89U},
    {"G6", 1567.98f, 91U},
    {"A6", 1760.00f, 93U},
    {"B6", 1975.53f, 95U},
};

typedef struct {
    FuriMessageQueue* q;
    ViewPort* view_port;
    Gui* gui;
    volatile bool exit_requested;
    FuriHalUsbInterface* previous_usb_config;
    FuriHalUsbHidConfig hid_cfg;
    bool tone_on;
    bool speaker_owned;
    bool speaker_busy;
    bool transport_connected;
    bool left_down;
    bool ok_down;
    bool back_down;
    bool trainer_playback_active;
    bool trainer_playback_mark;
    volatile bool midi_rx_pending;
    uint8_t screen;
    uint8_t pc_mode;
    uint8_t pc_paddle_preset;
    uint8_t pc_straight_preset;
    uint8_t pc_keys_row;
    uint8_t handedness;
    uint8_t input_source;
    uint8_t keyer_mode;
    uint8_t trainer_row;
    uint8_t trainer_char_idx;
    uint8_t trainer_mark_idx;
    bool vail_mode_active;
    bool vail_speed_active;
    bool vail_tone_active;
    uint8_t vail_keyer_mode;
    uint16_t vail_dit_ms;
    uint8_t vail_tone_idx;
    MorseKeyer keyer;
    uint8_t tone_idx;
    uint8_t preview_ticks;
    uint8_t input_mask;
    uint32_t trainer_next_at;
    uint32_t paddle_sources[MorseKeyerPaddleCount];
    uint32_t note_sources[3];
    MorseTrainer trainer;
} MorseFlipperApp;

static void morse_flipper_set_paddle_source(
    MorseFlipperApp* app,
    uint8_t paddle,
    uint32_t source_mask,
    bool active,
    uint32_t now_ms);
static void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_poll(MorseFlipperApp* app);
static void morse_flipper_release_all_notes(MorseFlipperApp* app);
static void morse_flipper_set_pc_mode(MorseFlipperApp* app, uint8_t mode);
static void morse_flipper_handle_midi_rx(MorseFlipperApp* app);
static void morse_flipper_update_sidetone(MorseFlipperApp* app);
static void morse_flipper_cycle_pc_mode(MorseFlipperApp* app, int dir);
static void morse_flipper_midi_rx_ready(void* context);
static void morse_flipper_cycle_pc_key_preset(MorseFlipperApp* app, int dir);
static void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_start_trainer_group(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir);

static uint8_t morse_flipper_current_keyer_mode(const MorseFlipperApp* app) {
    if(app->vail_mode_active) {
        return app->vail_keyer_mode;
    }

    return app->keyer_mode;
}

static uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app) {
    if(app->vail_speed_active) {
        return app->vail_dit_ms;
    }

    return MORSE_FLIPPER_DEFAULT_DIT_MS;
}

static uint8_t morse_flipper_current_wpm(const MorseFlipperApp* app) {
    uint16_t dit_ms = morse_flipper_current_dit_ms(app);

    if(dit_ms == 0U) {
        return 0U;
    }

    return (uint8_t)((1200U + (dit_ms / 2U)) / dit_ms);
}

static const MorseFlipperTone* morse_flipper_current_tone(const MorseFlipperApp* app) {
    if(app->vail_tone_active) {
        return &morse_flipper_tones[app->vail_tone_idx];
    }

    return &morse_flipper_tones[app->tone_idx];
}

static bool morse_flipper_any_active_notes(const MorseFlipperApp* app) {
    for(size_t i = 0; i < COUNT_OF(app->note_sources); i++) {
        if(app->note_sources[i] != 0U) {
            return true;
        }
    }

    return false;
}

static const char* morse_flipper_status_line(const MorseFlipperApp* app) {
    if(app->speaker_busy) {
        return "speaker busy";
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        if(app->handedness == MorseFlipperHandednessSwapped) {
            return "src pad hand swp";
        }
        return "src pad hand norm";
    }

    if(app->handedness == MorseFlipperHandednessSwapped) {
        return "src str hand swp";
    }
    return "src str hand norm";
}

static const char* morse_flipper_pc_mode_name(uint8_t mode) {
    switch(mode) {
    case MorseFlipperPcModeMidi:
        return "midi";
    case MorseFlipperPcModeKeyboard:
        return "keyboard";
    default:
        return "off";
    }
}

static const char* morse_flipper_hand_name(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? "swp" : "norm";
}

static const char* morse_flipper_gpio_hand_map(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? "dit5 dah7" : "dit7 dah5";
}

static uint8_t morse_flipper_button_ok_paddle(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? MorseKeyerPaddleDah :
                                                                MorseKeyerPaddleDit;
}

static uint8_t morse_flipper_button_back_paddle(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? MorseKeyerPaddleDit :
                                                                MorseKeyerPaddleDah;
}

static const char* morse_flipper_button_map_line(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? "L str  O dah  B dit" :
                                                                "L str  O dit  B dah";
}

static const char* morse_flipper_pc_state_name(const MorseFlipperApp* app) {
    if(app->pc_mode == MorseFlipperPcModeMidi) {
        return morse_usb_midi_is_connected() ? "usb midi up" : "usb midi wait";
    }

    if(app->pc_mode == MorseFlipperPcModeKeyboard) {
        return furi_hal_hid_is_connected() ? "usb hid up" : "usb hid wait";
    }

    return "usb local off";
}

static uint8_t morse_flipper_nearest_tone_idx_for_midi(uint8_t midi_note) {
    uint8_t best_idx = 0U;
    uint8_t best_delta = 0xFFU;

    for(uint8_t i = 0; i < COUNT_OF(morse_flipper_tones); i++) {
        uint8_t tone_note = morse_flipper_tones[i].midi_note;
        uint8_t delta = (tone_note > midi_note) ? (tone_note - midi_note) : (midi_note - tone_note);
        if(delta < best_delta) {
            best_delta = delta;
            best_idx = i;
        }
    }

    return best_idx;
}

static bool morse_flipper_transport_connected(const MorseFlipperApp* app) {
    if(app->pc_mode == MorseFlipperPcModeMidi) {
        return morse_usb_midi_is_connected();
    }

    if(app->pc_mode == MorseFlipperPcModeKeyboard) {
        return furi_hal_hid_is_connected();
    }

    return false;
}

static uint8_t morse_flipper_keyboard_key_for_note(const MorseFlipperApp* app, uint8_t note) {
    switch(note) {
    case 0:
        return morse_pc_straight_preset_key(app->pc_straight_preset);
    case 1:
    case 2:
        return morse_pc_paddle_preset_key(
            app->pc_paddle_preset, note, app->handedness == MorseFlipperHandednessSwapped);
    default:
        return MorsePcKeyNone;
    }
}

static uint16_t morse_flipper_hid_key_for_pc_key(uint8_t key) {
    switch(key) {
    case MorsePcKeySpace:
        return HID_KEYBOARD_SPACEBAR;
    case MorsePcKeyX:
        return HID_KEYBOARD_X;
    case MorsePcKeyZ:
        return HID_KEYBOARD_Z;
    case MorsePcKeyC:
        return HID_KEYBOARD_C;
    case MorsePcKeyEnter:
        return HID_KEYBOARD_RETURN;
    case MorsePcKeyNumEnter:
        return HID_KEYPAD_ENTER;
    case MorsePcKeyOpenBracket:
        return HID_KEYBOARD_OPEN_BRACKET;
    case MorsePcKeyCloseBracket:
        return HID_KEYBOARD_CLOSE_BRACKET;
    case MorsePcKeyLeftCtrl:
        return HID_KEYBOARD_L_CTRL;
    case MorsePcKeyRightCtrl:
        return HID_KEYBOARD_R_CTRL;
    case MorsePcKeyLeftShift:
        return HID_KEYBOARD_L_SHIFT;
    case MorsePcKeyRightShift:
        return HID_KEYBOARD_R_SHIFT;
    case MorsePcKeyPeriod:
    case MorsePcKeyGreater:
        return HID_KEYBOARD_DOT;
    case MorsePcKeySlash:
        return HID_KEYBOARD_SLASH;
    case MorsePcKeyComma:
    case MorsePcKeyLess:
        return HID_KEYBOARD_COMMA;
    default:
        return HID_KEYBOARD_NONE;
    }
}

static bool morse_flipper_pc_key_needs_shift(uint8_t key) {
    return key == MorsePcKeyLess || key == MorsePcKeyGreater;
}

static void morse_flipper_send_midi_note(uint8_t note, bool active) {
    uint8_t packet[4];

    packet[0] = active ? 0x09U : 0x08U;
    packet[1] = active ? 0x90U : 0x80U;
    packet[2] = note;
    packet[3] = active ? 0x7FU : 0x00U;

    morse_usb_midi_tx(packet, sizeof(packet));
}

static void morse_flipper_send_keyboard_note(MorseFlipperApp* app, uint8_t note, bool active) {
    uint8_t pc_key = morse_flipper_keyboard_key_for_note(app, note);
    uint16_t key = morse_flipper_hid_key_for_pc_key(pc_key);

    if(key == HID_KEYBOARD_NONE) {
        return;
    }

    if(active) {
        if(morse_flipper_pc_key_needs_shift(pc_key)) {
            furi_hal_hid_kb_press(HID_KEYBOARD_L_SHIFT);
        }
        furi_hal_hid_kb_press(key);
    } else {
        furi_hal_hid_kb_release(key);
        if(morse_flipper_pc_key_needs_shift(pc_key)) {
            furi_hal_hid_kb_release(HID_KEYBOARD_L_SHIFT);
        }
    }
}

static void morse_flipper_send_transport_note(MorseFlipperApp* app, uint8_t note, bool active) {
    switch(app->pc_mode) {
    case MorseFlipperPcModeMidi:
        morse_flipper_send_midi_note(note, active);
        break;
    case MorseFlipperPcModeKeyboard:
        morse_flipper_send_keyboard_note(app, note, active);
        break;
    default:
        break;
    }
}

static void morse_flipper_clear_vail_overrides(MorseFlipperApp* app) {
    bool had_tone_override = app->vail_tone_active;

    app->vail_mode_active = false;
    app->vail_speed_active = false;
    app->vail_tone_active = false;

    if(had_tone_override && app->tone_on && app->speaker_owned && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_start(morse_flipper_current_tone(app)->hz, MORSE_FLIPPER_VOLUME);
    }
}

static void morse_flipper_apply_vail_speed(MorseFlipperApp* app, uint8_t value) {
    uint16_t dit_ms = (value == 0U) ? 1U : ((uint16_t)value * 2U);

    if(app->vail_speed_active && app->vail_dit_ms == dit_ms) {
        return;
    }

    app->vail_speed_active = true;
    app->vail_dit_ms = dit_ms;
    morse_flipper_refresh_keyer(app, furi_get_tick());
    view_port_update(app->view_port);
}

static void morse_flipper_apply_vail_mode(MorseFlipperApp* app, uint8_t program) {
    uint8_t mode = morse_keyer_mode_valid(program) ? program : MorseKeyerModePassthrough;

    if(app->vail_mode_active && app->vail_keyer_mode == mode) {
        return;
    }

    app->vail_mode_active = true;
    app->vail_keyer_mode = mode;
    morse_flipper_refresh_keyer(app, furi_get_tick());
    view_port_update(app->view_port);
}

static void morse_flipper_apply_vail_tone(MorseFlipperApp* app, uint8_t midi_note) {
    uint8_t tone_idx = morse_flipper_nearest_tone_idx_for_midi(midi_note);

    if(app->vail_tone_active && app->vail_tone_idx == tone_idx) {
        return;
    }

    app->vail_tone_active = true;
    app->vail_tone_idx = tone_idx;

    if(app->tone_on && app->speaker_owned && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_start(morse_flipper_current_tone(app)->hz, MORSE_FLIPPER_VOLUME);
    } else {
        morse_flipper_update_sidetone(app);
    }

    view_port_update(app->view_port);
}

static void morse_flipper_resync_transport_notes(MorseFlipperApp* app) {
    for(uint8_t note = 0U; note < COUNT_OF(app->note_sources); note++) {
        if(app->note_sources[note] != 0U) {
            morse_flipper_send_transport_note(app, note, true);
        }
    }
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

            if(config.keyer_mode >= MorseKeyerModeStraight &&
               config.keyer_mode <= MorseKeyerModeKeyahead) {
                app->keyer_mode = config.keyer_mode;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void morse_flipper_save_config(const MorseFlipperApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperConfig config = {
        .version = MORSE_FLIPPER_CONFIG_VERSION,
        .tone_idx = app->tone_idx,
        .keyer_mode = app->keyer_mode,
        .spare0 = 0U,
        .spare1 = 0U,
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
    furi_hal_gpio_init(morse_flipper_ground_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(morse_flipper_ground_pin, false);
}

static void morse_flipper_gpio_deinit(void) {
    furi_hal_gpio_init(morse_flipper_straight_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_ground_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
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

static bool morse_flipper_logical_dit_down(const MorseFlipperApp* app) {
    if(app->handedness == MorseFlipperHandednessSwapped) {
        return morse_flipper_dah_down();
    }

    return morse_flipper_dit_down();
}

static bool morse_flipper_logical_dah_down(const MorseFlipperApp* app) {
    if(app->handedness == MorseFlipperHandednessSwapped) {
        return morse_flipper_dit_down();
    }

    return morse_flipper_dah_down();
}

static void morse_flipper_tone_stop(MorseFlipperApp* app) {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    app->tone_on = false;
    app->speaker_owned = false;
    app->speaker_busy = false;
}

static void morse_flipper_tone_start(MorseFlipperApp* app) {
    if(app->tone_on) {
        return;
    }

    if(!app->speaker_owned) {
        if(furi_hal_speaker_acquire(30U)) {
            app->speaker_owned = true;
        } else {
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

static void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    bool want_tone =
        morse_flipper_any_active_notes(app) || (app->preview_ticks > 0U) || app->trainer_playback_mark;

    if(want_tone) {
        morse_flipper_tone_start(app);
    } else {
        morse_flipper_tone_stop(app);
    }
}

static void morse_flipper_set_note_source(
    MorseFlipperApp* app,
    uint8_t note,
    uint32_t source_mask,
    bool active) {
    if(note >= COUNT_OF(app->note_sources)) {
        return;
    }

    uint32_t before = app->note_sources[note];
    uint32_t after = active ? (before | source_mask) : (before & ~source_mask);

    if(before == after) {
        return;
    }

    app->note_sources[note] = after;
    morse_flipper_update_sidetone(app);

    if(before == 0U && after != 0U) {
        morse_flipper_send_transport_note(app, note, true);
    } else if(before != 0U && after == 0U) {
        morse_flipper_send_transport_note(app, note, false);
    }
}

static void morse_flipper_release_all_notes(MorseFlipperApp* app) {
    uint32_t note_sources[COUNT_OF(app->note_sources)];

    for(size_t note = 0; note < COUNT_OF(app->note_sources); note++) {
        note_sources[note] = app->note_sources[note];
        app->note_sources[note] = 0U;
    }

    morse_flipper_update_sidetone(app);

    for(size_t note = 0; note < COUNT_OF(app->note_sources); note++) {
        if(note_sources[note] != 0U) {
            morse_flipper_send_transport_note(app, (uint8_t)note, false);
        }
    }
}

static void morse_flipper_enter_screen(
    MorseFlipperApp* app,
    uint8_t screen,
    uint32_t now_ms) {
    if(app->screen == screen) {
        return;
    }

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace ||
       app->screen == MorseFlipperScreenPc) {
        morse_flipper_clear_button_keying(app, now_ms);
    }

    app->screen = screen;
    if(screen == MorseFlipperScreenHome) {
        morse_flipper_poll(app);
    }
    view_port_update(app->view_port);
}

static void morse_flipper_toggle_source(MorseFlipperApp* app) {
    if(app->input_source == MorseFlipperInputSourceStraight) {
        app->input_source = MorseFlipperInputSourcePaddle;
    } else {
        app->input_source = MorseFlipperInputSourceStraight;
    }

    morse_flipper_refresh_keyer(app, furi_get_tick());
    morse_flipper_poll(app);
    view_port_update(app->view_port);
}

static void morse_flipper_cycle_pc_mode(MorseFlipperApp* app, int dir) {
    int next = (int)app->pc_mode + dir;

    if(next < (int)MorseFlipperPcModeOff) {
        next = (int)MorseFlipperPcModeKeyboard;
    } else if(next > (int)MorseFlipperPcModeKeyboard) {
        next = (int)MorseFlipperPcModeOff;
    }

    morse_flipper_set_pc_mode(app, (uint8_t)next);
    view_port_update(app->view_port);
}

static void morse_flipper_cycle_pc_key_preset(MorseFlipperApp* app, int dir) {
    int next;

    if(app->pc_keys_row == 0U) {
        next = (int)app->pc_paddle_preset + dir;
        if(next < 0) {
            next = (int)morse_pc_paddle_preset_count() - 1;
        } else if(next >= (int)morse_pc_paddle_preset_count()) {
            next = 0;
        }
        app->pc_paddle_preset = (uint8_t)next;
    } else {
        next = (int)app->pc_straight_preset + dir;
        if(next < 0) {
            next = (int)morse_pc_straight_preset_count() - 1;
        } else if(next >= (int)morse_pc_straight_preset_count()) {
            next = 0;
        }
        app->pc_straight_preset = (uint8_t)next;
    }

    view_port_update(app->view_port);
}

static void morse_flipper_start_trainer_group(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) {
        return;
    }

    morse_trainer_next_group(&app->trainer);
    app->trainer_playback_active = true;
    app->trainer_playback_mark = false;
    app->trainer_char_idx = 0U;
    app->trainer_mark_idx = 0U;
    app->trainer_next_at = now_ms;
    view_port_update(app->view_port);
}

static void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms) {
    const char* group;
    const char* morse;
    uint32_t dit_ms;

    if(app == NULL || !app->trainer_playback_active || now_ms < app->trainer_next_at) {
        return;
    }

    group = morse_trainer_last_group(&app->trainer);
    if(group[app->trainer_char_idx] == '\0') {
        app->trainer_playback_active = false;
        app->trainer_playback_mark = false;
        app->trainer_next_at = 0U;
        view_port_update(app->view_port);
        return;
    }

    morse = morse_trainer_char_morse(group[app->trainer_char_idx]);
    dit_ms = morse_flipper_current_dit_ms(app);

    if(app->trainer_playback_mark) {
        app->trainer_playback_mark = false;
        if(morse[app->trainer_mark_idx + 1U] != '\0') {
            app->trainer_mark_idx++;
            app->trainer_next_at = now_ms + dit_ms;
        } else if(group[app->trainer_char_idx + 1U] != '\0') {
            app->trainer_char_idx++;
            app->trainer_mark_idx = 0U;
            app->trainer_next_at = now_ms + (dit_ms * 3U);
        } else {
            app->trainer_playback_active = false;
            app->trainer_next_at = 0U;
        }
        view_port_update(app->view_port);
        return;
    }

    if(morse[app->trainer_mark_idx] == '\0') {
        app->trainer_playback_active = false;
        app->trainer_next_at = 0U;
        return;
    }

    app->trainer_playback_mark = true;
    app->trainer_next_at =
        now_ms + ((morse[app->trainer_mark_idx] == '-') ? (dit_ms * 3U) : dit_ms);
    view_port_update(app->view_port);
}

static void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir) {
    int next;

    if(app == NULL) {
        return;
    }

    switch(app->trainer_row) {
    case 0:
        next = (int)morse_trainer_lesson(&app->trainer) + dir;
        morse_trainer_set_lesson(&app->trainer, (uint8_t)next);
        break;
    case 1:
        next = (int)morse_trainer_group_size(&app->trainer) + dir;
        morse_trainer_set_group_size(&app->trainer, (uint8_t)next);
        break;
    default:
        next = (int)morse_trainer_session_groups(&app->trainer) + dir;
        morse_trainer_set_session_groups(&app->trainer, (uint8_t)next);
        break;
    }

    view_port_update(app->view_port);
}

static void morse_flipper_cycle_mode(MorseFlipperApp* app) {
    app->keyer_mode = morse_keyer_next_ui_mode(app->keyer_mode);
    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, furi_get_tick());
    view_port_update(app->view_port);
}

static void morse_flipper_midi_rx_ready(void* context) {
    MorseFlipperApp* app = context;

    if(app == NULL) {
        return;
    }

    app->midi_rx_pending = true;
}

static void morse_flipper_toggle_handedness(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    if(app->handedness == MorseFlipperHandednessNormal) {
        app->handedness = MorseFlipperHandednessSwapped;
    } else {
        app->handedness = MorseFlipperHandednessNormal;
    }

    morse_flipper_resync_button_paddles(app, now_ms);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    view_port_update(app->view_port);
}

static void morse_flipper_clear_button_paddles(MorseFlipperApp* app, uint32_t now_ms) {
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
}

static void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms) {
    morse_flipper_clear_button_paddles(app, now_ms);

    if(app->ok_down) {
        morse_flipper_set_paddle_source(
            app, morse_flipper_button_ok_paddle(app), MORSE_PADDLE_SOURCE_BTN_OK, true, now_ms);
    }

    if(app->back_down) {
        morse_flipper_set_paddle_source(
            app,
            morse_flipper_button_back_paddle(app),
            MORSE_PADDLE_SOURCE_BTN_BACK,
            true,
            now_ms);
    }
}

static void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now_ms) {
    app->left_down = false;
    app->ok_down = false;
    app->back_down = false;
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_clear_button_paddles(app, now_ms);
}

static uint32_t morse_flipper_note_source_for_paddle(uint8_t paddle) {
    return (paddle == MorseKeyerPaddleDit) ? MORSE_SOURCE_KEYER_DIT : MORSE_SOURCE_KEYER_DAH;
}

static uint8_t morse_flipper_note_for_paddle(uint8_t paddle) {
    return (paddle == MorseKeyerPaddleDit) ? 1U : 2U;
}

static void morse_flipper_drain_keyer_events(MorseFlipperApp* app) {
    MorseKeyerEvent event;

    while(morse_keyer_pop_event(&app->keyer, &event)) {
        morse_flipper_set_note_source(
            app,
            morse_flipper_note_for_paddle(event.paddle),
            morse_flipper_note_source_for_paddle(event.paddle),
            event.type == MorseKeyerEventPress);
    }
}

static void morse_flipper_set_paddle_source(
    MorseFlipperApp* app,
    uint8_t paddle,
    uint32_t source_mask,
    bool active,
    uint32_t now_ms) {
    if(paddle >= MorseKeyerPaddleCount) {
        return;
    }

    uint32_t before = app->paddle_sources[paddle];
    uint32_t after = active ? (before | source_mask) : (before & ~source_mask);

    if(before == after) {
        return;
    }

    app->paddle_sources[paddle] = after;
    morse_keyer_paddle_event(&app->keyer, paddle, after != 0U, now_ms);
    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);
}

static void morse_flipper_handle_active_keying_event(
    MorseFlipperApp* app,
    const InputEvent* event) {
    uint32_t now_ms = furi_get_tick();

    if(event->key == InputKeyLeft) {
        if(event->type == InputTypePress) {
            app->left_down = true;
            morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, true);
        } else if(event->type == InputTypeRelease) {
            app->left_down = false;
            morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
        }
        return;
    }

    if(event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            app->ok_down = true;
            morse_flipper_resync_button_paddles(app, now_ms);
        } else if(event->type == InputTypeRelease) {
            app->ok_down = false;
            morse_flipper_resync_button_paddles(app, now_ms);
        }
        return;
    }

    if(event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            app->back_down = true;
            morse_flipper_resync_button_paddles(app, now_ms);
        } else if(event->type == InputTypeRelease) {
            app->back_down = false;
            morse_flipper_resync_button_paddles(app, now_ms);
        }
        return;
    }

    if(event->key == InputKeyUp && event->type == InputTypeShort) {
        morse_flipper_toggle_source(app);
        return;
    }

    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        morse_flipper_cycle_mode(app);
        return;
    }

    if(event->key == InputKeyDown && event->type == InputTypeLong) {
        morse_flipper_toggle_handedness(app);
        return;
    }

    if(event->key == InputKeyRight &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_enter_screen(app, MorseFlipperScreenHome, now_ms);
    }
}

static void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms) {
    morse_keyer_reset(&app->keyer);
    morse_flipper_drain_keyer_events(app);
    morse_keyer_set_mode(&app->keyer, morse_flipper_current_keyer_mode(app));
    morse_keyer_set_dit_duration(&app->keyer, morse_flipper_current_dit_ms(app));

    for(uint8_t paddle = 0; paddle < MorseKeyerPaddleCount; paddle++) {
        if(app->paddle_sources[paddle] != 0U) {
            morse_keyer_paddle_event(&app->keyer, paddle, true, now_ms);
        }
    }

    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);
}

static void morse_flipper_set_pc_mode(MorseFlipperApp* app, uint8_t mode) {
    if(mode > MorseFlipperPcModeKeyboard) {
        mode = MorseFlipperPcModeOff;
    }

    if(app->pc_mode == mode) {
        return;
    }

    morse_flipper_release_all_notes(app);

    if(app->pc_mode == MorseFlipperPcModeKeyboard) {
        furi_hal_hid_kb_release_all();
    }

    if(mode == MorseFlipperPcModeOff) {
        morse_usb_midi_set_rx_callback(NULL);
        morse_usb_midi_set_context(NULL);
        if(app->previous_usb_config != NULL) {
            furi_check(furi_hal_usb_set_config(app->previous_usb_config, NULL));
            app->previous_usb_config = NULL;
        }
        app->pc_mode = MorseFlipperPcModeOff;
        app->transport_connected = false;
        app->midi_rx_pending = false;
        morse_flipper_clear_vail_overrides(app);
        morse_flipper_refresh_keyer(app, furi_get_tick());
        return;
    }

    if(app->previous_usb_config == NULL) {
        app->previous_usb_config = furi_hal_usb_get_config();
    }

    if(mode == MorseFlipperPcModeMidi) {
        morse_usb_midi_set_context(app);
        morse_usb_midi_set_rx_callback(morse_flipper_midi_rx_ready);
        furi_check(furi_hal_usb_set_config(NULL, NULL));
        furi_delay_ms(150U);
        furi_check(furi_hal_usb_set_config(&morse_usb_midi_interface, NULL));
    } else {
        morse_usb_midi_set_rx_callback(NULL);
        morse_usb_midi_set_context(NULL);
        app->midi_rx_pending = false;
        furi_check(furi_hal_usb_set_config(&usb_hid, &app->hid_cfg));
        morse_flipper_clear_vail_overrides(app);
    }

    app->pc_mode = mode;
    app->transport_connected = false;
    morse_flipper_refresh_keyer(app, furi_get_tick());
}

static void morse_flipper_handle_midi_rx(MorseFlipperApp* app) {
    uint8_t buffer[64];
    size_t size = morse_usb_midi_rx(buffer, sizeof(buffer));

    for(size_t offset = 0; offset + 3U < size; offset += 4U) {
        uint8_t status = buffer[offset + 1U] & 0xF0U;

        if(status == 0xB0U) {
            uint8_t control = buffer[offset + 2U];
            uint8_t value = buffer[offset + 3U];

            if(control == 1U) {
                morse_flipper_apply_vail_speed(app, value);
            } else if(control == 2U) {
                morse_flipper_apply_vail_tone(app, value);
            }
        } else if(status == 0xC0U) {
            morse_flipper_apply_vail_mode(app, buffer[offset + 2U]);
        }
    }
}

static void morse_flipper_sync_gpio_inputs(MorseFlipperApp* app, uint32_t now_ms) {
    bool straight_active =
        (app->input_source == MorseFlipperInputSourceStraight) && morse_flipper_straight_down();
    bool dit_active =
        (app->input_source == MorseFlipperInputSourcePaddle) && morse_flipper_logical_dit_down(app);
    bool dah_active =
        (app->input_source == MorseFlipperInputSourcePaddle) && morse_flipper_logical_dah_down(app);

    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, straight_active);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_GPIO_DIT, dit_active, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_GPIO_DAH, dah_active, now_ms);
}

static uint8_t morse_flipper_read_input_mask(const MorseFlipperApp* app) {
    uint8_t mask = 0U;

    if(app->left_down) {
        mask |= 1U << 0;
    }
    if(app->ok_down) {
        mask |= 1U << 1;
    }
    if(app->back_down) {
        mask |= 1U << 2;
    }
    if(morse_flipper_straight_down()) {
        mask |= 1U << 3;
    }
    if(morse_flipper_dah_down()) {
        mask |= 1U << 4;
    }
    if(morse_flipper_dit_down()) {
        mask |= 1U << 5;
    }

    return mask;
}

static const char* morse_flipper_input_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    size_t n = snprintf(buf, buf_sz, "raw ");

    if(app->input_mask & (1U << 0)) {
        n += snprintf(buf + n, buf_sz - n, "lt ");
    }
    if(app->input_mask & (1U << 1)) {
        n += snprintf(buf + n, buf_sz - n, "ok ");
    }
    if(app->input_mask & (1U << 2)) {
        n += snprintf(buf + n, buf_sz - n, "bk ");
    }
    if(app->input_mask & (1U << 3)) {
        n += snprintf(buf + n, buf_sz - n, "p3 ");
    }
    if(app->input_mask & (1U << 4)) {
        n += snprintf(buf + n, buf_sz - n, "p5 ");
    }
    if(app->input_mask & (1U << 5)) {
        n += snprintf(buf + n, buf_sz - n, "p7 ");
    }

    if(n == 4U) {
        snprintf(buf, buf_sz, "raw -");
    }

    return buf;
}

static void morse_flipper_tone_nudge(MorseFlipperApp* app, int dir) {
    int idx = (int)app->tone_idx + dir;

    if(idx < 0) {
        idx = 0;
    }
    if(idx >= (int)COUNT_OF(morse_flipper_tones)) {
        idx = (int)COUNT_OF(morse_flipper_tones) - 1;
    }
    if(idx == (int)app->tone_idx) {
        return;
    }

    app->tone_idx = (uint8_t)idx;
    app->preview_ticks = MORSE_FLIPPER_PREVIEW_TICKS;

    if(app->tone_on && app->speaker_owned && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_start(morse_flipper_current_tone(app)->hz, MORSE_FLIPPER_VOLUME);
    }

    morse_flipper_save_config(app);
    morse_flipper_update_sidetone(app);
    view_port_update(app->view_port);
}

static void morse_flipper_poll(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();
    bool old_tone = app->tone_on;
    bool old_busy = app->speaker_busy;
    uint8_t old_mask = app->input_mask;
    bool old_transport = app->transport_connected;

    if(app->pc_mode == MorseFlipperPcModeMidi && app->midi_rx_pending) {
        app->midi_rx_pending = false;
        morse_flipper_handle_midi_rx(app);
    }

    app->transport_connected = morse_flipper_transport_connected(app);
    if(!old_transport && app->transport_connected) {
        morse_flipper_resync_transport_notes(app);
    }

    app->input_mask = morse_flipper_read_input_mask(app);
    morse_flipper_sync_gpio_inputs(app, now_ms);
    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);
    morse_flipper_update_sidetone(app);

    if(old_tone != app->tone_on || old_busy != app->speaker_busy || old_mask != app->input_mask ||
       old_transport != app->transport_connected) {
        view_port_update(app->view_port);
    }
}

static void morse_flipper_draw(Canvas* canvas, void* ctx) {
    MorseFlipperApp* app = ctx;
    char tone_line[32];
    char input_line[32];
    char trace_line1[32];
    char trace_line2[32];
    char trace_line3[32];
    char trace_line4[32];
    char run_line[32];
    char pc_line[32];
    char pc_pad_line[32];
    char pc_str_line[32];
    char trainer_line[32];
    char trainer_line2[32];
    char trainer_line3[32];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->screen == MorseFlipperScreenRun) {
        snprintf(
            run_line,
            sizeof(run_line),
            "%s %s %s",
            morse_flipper_current_tone(app)->name,
            morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)),
            morse_flipper_hand_name(app));

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 14, "Button Run");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 8, 28, run_line);
        canvas_draw_str(canvas, 8, 40, morse_flipper_button_map_line(app));
        canvas_draw_str(canvas, 8, 52, morse_flipper_input_line(app, input_line, sizeof(input_line)));
        canvas_draw_str(canvas, 8, 62, "btn+gpio swap");
        return;
    }

    if(app->screen == MorseFlipperScreenPc) {
        snprintf(pc_line, sizeof(pc_line), "< %s >", morse_flipper_pc_mode_name(app->pc_mode));
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 18, 14, "PC Mode");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 20, 30, pc_line);
        canvas_draw_str(canvas, 8, 44, morse_flipper_pc_state_name(app));
        if(app->pc_mode == MorseFlipperPcModeKeyboard) {
            snprintf(
                pc_pad_line,
                sizeof(pc_pad_line),
                "pad %s",
                morse_pc_paddle_preset_name(app->pc_paddle_preset));
            snprintf(
                pc_str_line,
                sizeof(pc_str_line),
                "str %s hold R keys",
                morse_pc_straight_preset_name(app->pc_straight_preset));
            canvas_draw_str(canvas, 8, 54, pc_pad_line);
            canvas_draw_str(canvas, 8, 63, pc_str_line);
        } else {
            snprintf(
                tone_line,
                sizeof(tone_line),
                "wpm %u  %s",
                morse_flipper_current_wpm(app),
                morse_flipper_hand_name(app));
            canvas_draw_str(canvas, 8, 54, tone_line);
            canvas_draw_str(canvas, 8, 63, "L str O/B key U/D mode");
        }
        return;
    }

    if(app->screen == MorseFlipperScreenPcKeys) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 6, 14, "USB Key Presets");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            pc_pad_line,
            sizeof(pc_pad_line),
            "%c Paddle <%s>",
            app->pc_keys_row == 0U ? '>' : ' ',
            morse_pc_paddle_preset_name(app->pc_paddle_preset));
        snprintf(
            pc_str_line,
            sizeof(pc_str_line),
            "%c Straight <%s>",
            app->pc_keys_row == 1U ? '>' : ' ',
            morse_pc_straight_preset_name(app->pc_straight_preset));
        canvas_draw_str(canvas, 2, 30, pc_pad_line);
        canvas_draw_str(canvas, 2, 44, pc_str_line);
        canvas_draw_str(canvas, 2, 58, "U/D row L/R pick");
        canvas_draw_str(canvas, 2, 64, "OK/Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenTrainer) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 14, "Koch Group");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "%c lesson %u/%u",
            app->trainer_row == 0U ? '>' : ' ',
            (unsigned)morse_trainer_lesson(&app->trainer),
            (unsigned)morse_trainer_lesson_count());
        snprintf(
            trainer_line2,
            sizeof(trainer_line2),
            "%c size %u  groups %u",
            app->trainer_row == 1U ? '>' : ' ',
            (unsigned)morse_trainer_group_size(&app->trainer),
            (unsigned)morse_trainer_session_groups(&app->trainer));
        snprintf(
            trainer_line3,
            sizeof(trainer_line3),
            "%c chars %s",
            app->trainer_row == 2U ? '>' : ' ',
            morse_trainer_charset(&app->trainer));
        canvas_draw_str(canvas, 8, 26, trainer_line);
        canvas_draw_str(canvas, 8, 36, trainer_line2);
        canvas_draw_str(canvas, 8, 46, trainer_line3);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 58, morse_trainer_last_group(&app->trainer));
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas,
            8,
            64,
            app->trainer_playback_active ? "U/D row Bk home" : "L/R tweak OK play");
        return;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        snprintf(
            trace_line1,
            sizeof(trace_line1),
            "tr %s %s %s",
            morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)),
            (app->input_source == MorseFlipperInputSourcePaddle) ? "pad" : "str",
            morse_flipper_hand_name(app));
        snprintf(
            trace_line2,
            sizeof(trace_line2),
            "pd %u%u out %u%u rep %ld",
            app->keyer.paddle_down[MorseKeyerPaddleDit] ? 1U : 0U,
            app->keyer.paddle_down[MorseKeyerPaddleDah] ? 1U : 0U,
            app->keyer.output_active[MorseKeyerPaddleDit] ? 1U : 0U,
            app->keyer.output_active[MorseKeyerPaddleDah] ? 1U : 0U,
            (long)app->keyer.next_repeat_paddle);
        snprintf(
            trace_line3,
            sizeof(trace_line3),
            "set %u fifo %u pulse %u",
            app->keyer.set_queue_len,
            app->keyer.fifo_queue_len,
            app->keyer.pulse_active ? 1U : 0U);
        snprintf(
            trace_line4,
            sizeof(trace_line4),
            "n %lu/%lu/%lu %s",
            (unsigned long)app->note_sources[0],
            (unsigned long)app->note_sources[1],
            (unsigned long)app->note_sources[2],
            morse_flipper_gpio_hand_map(app));

        canvas_draw_str(canvas, 2, 10, trace_line1);
        canvas_draw_str(canvas, 2, 20, morse_flipper_input_line(app, input_line, sizeof(input_line)));
        canvas_draw_str(canvas, 2, 30, trace_line2);
        canvas_draw_str(canvas, 2, 40, trace_line3);
        canvas_draw_str(canvas, 2, 50, trace_line4);
        canvas_draw_str(canvas, 2, 60, "key live R home");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 14, "Morse Flipper");

    snprintf(
        tone_line,
        sizeof(tone_line),
        "< %s > %s",
        morse_flipper_current_tone(app)->name,
        morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, morse_flipper_pc_state_name(app));
    canvas_draw_str(canvas, 8, 42, tone_line);
    canvas_draw_str(canvas, 8, 52, morse_flipper_input_line(app, input_line, sizeof(input_line)));
    canvas_draw_str(canvas, 8, 62, morse_flipper_status_line(app));
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

    static MorseFlipperApp app;
    app = (MorseFlipperApp){
        .q = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .view_port = view_port_alloc(),
        .gui = furi_record_open(RECORD_GUI),
        .exit_requested = false,
        .previous_usb_config = NULL,
        .hid_cfg =
            {
                .vid = 0x6666U,
                .pid = 0x434BU,
                .manuf = "YO3GND",
                .product = "Morse Flipper Kbd",
            },
        .tone_on = false,
        .speaker_owned = false,
        .speaker_busy = false,
        .transport_connected = false,
        .left_down = false,
        .ok_down = false,
        .back_down = false,
        .trainer_playback_active = false,
        .trainer_playback_mark = false,
        .midi_rx_pending = false,
        .screen = MorseFlipperScreenHome,
        .pc_mode = MorseFlipperPcModeOff,
        .pc_paddle_preset = 0U,
        .pc_straight_preset = 0U,
        .pc_keys_row = 0U,
        .handedness = MorseFlipperHandednessNormal,
        .input_source = MorseFlipperInputSourceStraight,
        .keyer_mode = MorseKeyerModeStraight,
        .trainer_row = 0U,
        .trainer_char_idx = 0U,
        .trainer_mark_idx = 0U,
        .vail_mode_active = false,
        .vail_speed_active = false,
        .vail_tone_active = false,
        .vail_keyer_mode = MorseKeyerModeStraight,
        .vail_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS,
        .vail_tone_idx = 0U,
        .keyer = {0},
        .tone_idx = 0U,
        .preview_ticks = 0U,
        .input_mask = 0U,
        .trainer_next_at = 0U,
        .paddle_sources = {0U, 0U},
        .note_sources = {0U, 0U, 0U},
        .trainer = {0},
    };

    morse_flipper_load_config(&app);
    morse_keyer_init(&app.keyer, app.keyer_mode, MORSE_FLIPPER_DEFAULT_DIT_MS);
    morse_trainer_init(&app.trainer);
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
            if(app.screen == MorseFlipperScreenPcKeys) {
                if(event.key == InputKeyUp &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    app.pc_keys_row = (app.pc_keys_row == 0U) ? 1U : 0U;
                    view_port_update(app.view_port);
                }

                if(event.key == InputKeyDown &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    app.pc_keys_row = (app.pc_keys_row == 0U) ? 1U : 0U;
                    view_port_update(app.view_port);
                }

                if(event.key == InputKeyLeft &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    morse_flipper_cycle_pc_key_preset(&app, -1);
                }

                if(event.key == InputKeyRight &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    morse_flipper_cycle_pc_key_preset(&app, 1);
                }

                if((event.key == InputKeyOk || event.key == InputKeyBack) &&
                   (event.type == InputTypeShort || event.type == InputTypeLong ||
                    event.type == InputTypePress)) {
                    morse_flipper_enter_screen(&app, MorseFlipperScreenPc, furi_get_tick());
                }
                continue;
            }

            if(app.screen == MorseFlipperScreenPc) {
                if(event.key == InputKeyLeft || event.key == InputKeyOk ||
                   event.key == InputKeyBack) {
                    morse_flipper_handle_active_keying_event(&app, &event);
                }

                if(event.key == InputKeyUp &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    morse_flipper_cycle_pc_mode(&app, -1);
                }

                if(event.key == InputKeyDown &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    morse_flipper_cycle_pc_mode(&app, 1);
                }

                if(event.key == InputKeyRight &&
                   (event.type == InputTypeShort || event.type == InputTypeLong)) {
                    if(app.pc_mode == MorseFlipperPcModeKeyboard &&
                       event.type == InputTypeLong) {
                        morse_flipper_enter_screen(&app, MorseFlipperScreenPcKeys, furi_get_tick());
                    } else {
                        morse_flipper_enter_screen(&app, MorseFlipperScreenHome, furi_get_tick());
                    }
                }
                continue;
            }

            if(app.screen == MorseFlipperScreenTrainer) {
                if(event.key == InputKeyOk &&
                   (event.type == InputTypeShort || event.type == InputTypeLong)) {
                    morse_flipper_start_trainer_group(&app, furi_get_tick());
                }

                if(event.key == InputKeyUp &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    app.trainer_row = (app.trainer_row == 0U) ? 2U : (app.trainer_row - 1U);
                    view_port_update(app.view_port);
                }

                if(event.key == InputKeyDown &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    app.trainer_row = (app.trainer_row + 1U) % 3U;
                    view_port_update(app.view_port);
                }

                if(event.key == InputKeyLeft &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    morse_flipper_cycle_trainer_value(&app, -1);
                }

                if(event.key == InputKeyRight &&
                   (event.type == InputTypeShort || event.type == InputTypeRepeat)) {
                    morse_flipper_cycle_trainer_value(&app, 1);
                }

                if(event.key == InputKeyBack &&
                   (event.type == InputTypeShort || event.type == InputTypeLong)) {
                    app.trainer_playback_active = false;
                    app.trainer_playback_mark = false;
                    morse_flipper_enter_screen(&app, MorseFlipperScreenHome, furi_get_tick());
                }
                continue;
            }

            if(app.screen == MorseFlipperScreenRun) {
                morse_flipper_handle_active_keying_event(&app, &event);
                continue;
            }

            if(app.screen == MorseFlipperScreenTrace) {
                morse_flipper_handle_active_keying_event(&app, &event);
                continue;
            }

            if(event.key == InputKeyOk) {
                if(event.type == InputTypeShort) {
                    morse_flipper_enter_screen(&app, MorseFlipperScreenRun, furi_get_tick());
                } else if(event.type == InputTypeLong) {
                    morse_flipper_enter_screen(&app, MorseFlipperScreenPc, furi_get_tick());
                }
            }

            if(event.type == InputTypePress) {
                if(event.key == InputKeyLeft) {
                    morse_flipper_tone_nudge(&app, -1);
                } else if(event.key == InputKeyRight) {
                    morse_flipper_tone_nudge(&app, 1);
                }
            }

            if(event.key == InputKeyLeft && event.type == InputTypeLong) {
                morse_flipper_enter_screen(&app, MorseFlipperScreenTrainer, furi_get_tick());
            }

            if(event.key == InputKeyUp && event.type == InputTypeShort) {
                morse_flipper_toggle_source(&app);
            }

            if(event.key == InputKeyUp && event.type == InputTypeLong) {
                morse_flipper_enter_screen(&app, MorseFlipperScreenTrace, furi_get_tick());
            }

            if(event.key == InputKeyDown && event.type == InputTypeShort) {
                morse_flipper_cycle_mode(&app);
            }

            if(event.key == InputKeyDown && event.type == InputTypeLong) {
                morse_flipper_toggle_handedness(&app);
            }

            if(event.key == InputKeyBack) {
                if(event.type == InputTypePress || event.type == InputTypeShort ||
                   event.type == InputTypeLong) {
                    morse_flipper_clear_button_keying(&app, furi_get_tick());
                    running = false;
                }
            }
        }

        morse_flipper_poll(&app);
        morse_flipper_tick_trainer_playback(&app, furi_get_tick());

        if(app.preview_ticks > 0U) {
            app.preview_ticks--;
            morse_flipper_update_sidetone(&app);
        }
    }

    morse_flipper_clear_button_keying(&app, furi_get_tick());
    morse_flipper_set_pc_mode(&app, MorseFlipperPcModeOff);
    morse_keyer_reset(&app.keyer);
    morse_flipper_drain_keyer_events(&app);
    morse_flipper_release_all_notes(&app);
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
