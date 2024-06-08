#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <string.h>

#include "keyer.h"
#include "morse_flipper_cw_decoder.h"
#include "morse_flipper_paths.h"
#include "morse_flipper_radio.h"
#include "morse_flipper_rf.h"
#include "morse_flipper_straight_trainer.h"
#include "pc_keys.h"
#include "trainer.h"
#include "trainer_files.h"
#include "usb/morse_usb_midi.h"

#define MORSE_FLIPPER_VOLUME 0.7f
#define MORSE_FLIPPER_POLL_MS 5
#define MORSE_FLIPPER_PREVIEW_TICKS 8
#define MORSE_FLIPPER_CONFIG_PATH APP_DATA_PATH("config.bin")
#define MORSE_FLIPPER_CONFIG_VERSION 2
#define MORSE_FLIPPER_DEFAULT_DIT_MS 100U
#define MORSE_FLIPPER_SESSION_SETTLE_MS 700U
#define MORSE_FLIPPER_SESSION_RESULT_MS 160U
#define MORSE_FLIPPER_SESSION_ADVANCE_MS 1000U
#define MORSE_FLIPPER_STRAIGHT_SETTLE_MS 700U
#define MORSE_FLIPPER_RF_TX_TAIL_DITS 7U
#define MORSE_FLIPPER_RF_RX_ARM_DITS 12U
#define MORSE_FLIPPER_RF_RX_ARM_MIN_MS 1000U
/* Temporary experiment. Leave this gate in place so we can flip RF live decode work on/off. */
#define MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_DECODERS 1U
/* Temporary experiment. Leave this gate in place so we can bypass SubGHz hardware and compare feel. */
#define MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_RADIO 1U

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
    MorseFlipperInputSourceButtons = 2,
} MorseFlipperInputSource;

typedef enum {
    MorseFlipperScreenHome = 0,
    MorseFlipperScreenRun = 1,
    MorseFlipperScreenTrace = 2,
    MorseFlipperScreenPc = 3,
    MorseFlipperScreenPcKeys = 4,
    MorseFlipperScreenTrainer = 5,
    MorseFlipperScreenSession = 6,
    MorseFlipperScreenBrowse = 7,
    MorseFlipperScreenRf = 8,
    MorseFlipperScreenStraight = 9,
    MorseFlipperScreenMenu = 10,
} MorseFlipperScreen;

typedef enum {
    MorseFlipperViewMenu = 0,
    MorseFlipperViewLive,
} MorseFlipperView;

typedef enum {
    MorseFlipperSceneMenuMain = 0,
    MorseFlipperSceneMenuTraining,
    MorseFlipperSceneMenuSettings,
    MorseFlipperSceneRun,
    MorseFlipperSceneRf,
    MorseFlipperSceneSession,
    MorseFlipperSceneStraight,
    MorseFlipperSceneBrowse,
    MorseFlipperSceneHome,
    MorseFlipperSceneTrainer,
    MorseFlipperScenePc,
    MorseFlipperScenePcKeys,
    MorseFlipperSceneTrace,
    MorseFlipperSceneNum,
} MorseFlipperScene;

typedef enum {
    MorseFlipperPcModeOff = 0,
    MorseFlipperPcModeMidi = 1,
    MorseFlipperPcModeKeyboard = 2,
} MorseFlipperPcMode;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t spare0;
    uint16_t local_dit_ms;
} MorseFlipperConfig;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t spare0;
    uint8_t spare1;
} MorseFlipperConfigV1;

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
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    View* live_view;
    Gui* gui;
    volatile bool exit_requested;
    FuriHalUsbInterface* previous_usb_config;
    FuriHalUsbHidConfig hid_cfg;
    bool tone_on;
    bool speaker_owned;
    bool speaker_busy;
    float speaker_hz;
    bool transport_connected;
    bool left_down;
    bool ok_down;
    bool back_down;
    bool session_log_pending;
    bool trainer_playback_active;
    bool trainer_playback_mark;
    bool session_started;
    bool session_round_pending;
    bool session_result_hold;
    bool session_result_tone;
    bool session_result_good;
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
    uint32_t straight_next_at;
    uint32_t straight_last_input_at;
    uint32_t straight_mark_started_at;
    uint32_t session_last_input_at;
    uint32_t session_result_until;
    uint32_t session_next_group_at;
    uint32_t rf_tx_tail_until;
    uint32_t rf_rx_arm_until;
    uint32_t rf_tx_edge_at;
    uint32_t gpio_edge_at;
    uint32_t paddle_sources[MorseKeyerPaddleCount];
    uint32_t note_sources[3];
    MorseTrainer trainer;
    MorseTrainerCustomSets custom_sets;
    MorseTrainerSessionLines session_lines;
    MorseTrainerStraightStats straight_stats;
    uint8_t session_line_idx;
    bool straight_playback_active;
    bool straight_playback_mark;
    bool straight_wait_answer;
    bool straight_done;
    bool straight_key_down;
    bool rf_manual_active;
    bool rf_live_active;
    bool rf_tx_level;
    bool rf_tx_gap_flushed;
    bool gpio_level;
    bool gpio_gap_flushed;
    uint8_t straight_mark_idx;
    uint8_t straight_return_screen;
    uint8_t rf_edit_digit;
    char rf_edit_khz[8];
    char rf_rx_text[64];
    char rf_tx_text[64];
    char gpio_text[64];
    MorseFlipperRf rf;
    MorseFlipperRadio radio;
    MorseFlipperCwDecoder rf_decoder;
    MorseFlipperCwDecoder tx_decoder;
    MorseFlipperCwDecoder gpio_decoder;
    MorseFlipperStraightTrainer straight_trainer;
} MorseFlipperApp;

typedef struct {
    MorseFlipperApp* app;
    uint32_t bump;
} MorseFlipperLiveModel;

static void morse_flipper_set_paddle_source(
    MorseFlipperApp* app,
    uint8_t paddle,
    uint32_t source_mask,
    bool active,
    uint32_t now_ms);
static void morse_flipper_set_note_source(
    MorseFlipperApp* app,
    uint8_t note,
    uint32_t source_mask,
    bool active);
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
static void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir);
static void morse_flipper_apply_trainer_charset_choice(MorseFlipperApp* app);
static void morse_flipper_drop_live_keying_for_playback(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_begin_group_playback(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_start_session(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_reset_session_runtime(MorseFlipperApp* app);
static void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms);
static bool morse_flipper_session_repeat_active(const MorseFlipperApp* app);
static void morse_flipper_tick_session(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_leave_session(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms);
static void morse_flipper_draw(Canvas* canvas, void* ctx);
static void morse_flipper_view_dirty(MorseFlipperApp* app);
static void morse_flipper_scene_open(MorseFlipperApp* app, uint32_t scene);
static void morse_flipper_scene_back(MorseFlipperApp* app);
static void morse_flipper_live_draw(Canvas* canvas, void* model);

static void morse_flipper_view_dirty(MorseFlipperApp* app) {
    if(app == NULL || app->live_view == NULL) return;

    with_view_model(app->live_view, MorseFlipperLiveModel * m, { m->bump++; }, true);
}

static void morse_flipper_live_draw(Canvas* canvas, void* model) {
    MorseFlipperLiveModel* m = model;

    if(m == NULL || m->app == NULL) return;
    morse_flipper_draw(canvas, m->app);
}

static void morse_flipper_scene_open(MorseFlipperApp* app, uint32_t scene) {
    if(app == NULL || app->scene_manager == NULL) return;
    scene_manager_next_scene(app->scene_manager, scene);
}

static void morse_flipper_scene_back(MorseFlipperApp* app) {
    if(app == NULL || app->scene_manager == NULL) return;

    if(scene_manager_previous_scene(app->scene_manager)) return;

    scene_manager_stop(app->scene_manager);
    if(app->view_dispatcher) view_dispatcher_stop(app->view_dispatcher);
}

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

    return app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
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

static float morse_flipper_active_tone_hz(const MorseFlipperApp* app) {
    float hz = morse_flipper_current_tone(app)->hz;

    if(app->session_result_tone) return app->session_result_good ? (hz * 1.35f) : (hz * 0.68f);
    return hz;
}

static bool morse_flipper_any_active_notes(const MorseFlipperApp* app) {
    for(size_t i = 0; i < COUNT_OF(app->note_sources); i++) {
        if(app->note_sources[i] != 0U) {
            return true;
        }
    }

    return false;
}

static bool morse_flipper_straight_like_mode(const MorseFlipperApp* app) {
    uint8_t mode = morse_flipper_current_keyer_mode(app);

    return mode == MorseKeyerModePassthrough || mode == MorseKeyerModeStraight;
}

static bool morse_flipper_live_keyer_phase(const MorseFlipperApp* app) {
    if(app == NULL) {
        return false;
    }

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace ||
       app->screen == MorseFlipperScreenPc) {
        return true;
    }

    if(app->screen == MorseFlipperScreenSession) {
        return morse_flipper_session_repeat_active(app);
    }

    if(app->screen == MorseFlipperScreenRf) {
        return app->rf_live_active;
    }

    return false;
}

static bool morse_flipper_button_source_active(const MorseFlipperApp* app) {
    return app != NULL && app->input_source == MorseFlipperInputSourceButtons &&
           morse_flipper_live_keyer_phase(app);
}

static bool morse_flipper_button_paddle_keying_active(const MorseFlipperApp* app) {
    return morse_flipper_button_source_active(app) && !morse_flipper_straight_like_mode(app);
}

static bool morse_flipper_button_straight_keying_active(const MorseFlipperApp* app) {
    return morse_flipper_button_source_active(app) && morse_flipper_straight_like_mode(app);
}

static const char* morse_flipper_status_line(const MorseFlipperApp* app) {
    if(app->speaker_busy) {
        return "speaker busy";
    }

    if(app->input_source == MorseFlipperInputSourceButtons) {
        if(morse_flipper_straight_like_mode(app)) {
            return "src btn ok str";
        }
        if(app->handedness == MorseFlipperHandednessSwapped) {
            return "src btn hand swp";
        }
        return "src btn hand norm";
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        if(app->handedness == MorseFlipperHandednessSwapped) {
            return "src pad hand swp";
        }
        return "src pad hand norm";
    }

    return "src p3 straight";
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

static const char* morse_flipper_free_practice_hint(const MorseFlipperApp* app) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        return morse_flipper_button_paddle_keying_active(app) ? "O/B key  hold L" :
                                                                "OK str  Bk back";
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        return "P7/P5 key  Bk back";
    }

    return "P3 key  Bk back";
}

static const char* morse_flipper_trace_hint(const MorseFlipperApp* app) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        return morse_flipper_button_paddle_keying_active(app) ? "key live hold L" :
                                                                "OK str  Bk back";
    }

    return "gpio live  Bk back";
}

static const char* morse_flipper_pc_hint(const MorseFlipperApp* app) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        return morse_flipper_button_paddle_keying_active(app) ? "O/B key U/D mode" :
                                                                "OK str U/D mode";
    }

    return "gpio key U/D mode";
}

static const char* morse_flipper_source_short_name(const MorseFlipperApp* app) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        return "btn";
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        return "pad";
    }

    return "str";
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
        return morse_pc_paddle_preset_key(app->pc_paddle_preset, note, false);
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
    morse_flipper_view_dirty(app);
}

static void morse_flipper_apply_vail_mode(MorseFlipperApp* app, uint8_t program) {
    uint8_t mode = morse_keyer_mode_valid(program) ? program : MorseKeyerModePassthrough;

    if(app->vail_mode_active && app->vail_keyer_mode == mode) {
        return;
    }

    app->vail_mode_active = true;
    app->vail_keyer_mode = mode;
    morse_flipper_refresh_keyer(app, furi_get_tick());
    morse_flipper_view_dirty(app);
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

    morse_flipper_view_dirty(app);
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
    MorseFlipperConfigV1 config_v1;
    uint16_t got = 0U;

    if(storage_file_open(file, MORSE_FLIPPER_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        got = storage_file_read(file, &config, sizeof(config));
        if(got == sizeof(config) && config.version == MORSE_FLIPPER_CONFIG_VERSION) {
            if(config.tone_idx < COUNT_OF(morse_flipper_tones)) {
                app->tone_idx = config.tone_idx;
            }

            if(config.keyer_mode >= MorseKeyerModeStraight &&
               config.keyer_mode <= MorseKeyerModeKeyahead) {
                app->keyer_mode = config.keyer_mode;
            }

            if(config.handedness <= MorseFlipperHandednessSwapped) {
                app->handedness = config.handedness;
            }

            if(config.spare0 <= MorseFlipperInputSourceButtons) {
                app->input_source = config.spare0;
            }

            morse_trainer_set_lesson(&app->trainer, config.trainer_lesson);
            morse_trainer_set_group_size(&app->trainer, config.trainer_group_size);
            morse_trainer_set_session_groups(&app->trainer, config.trainer_session_groups);
            if(config.local_dit_ms != 0U) {
                app->trainer.local_dit_ms = config.local_dit_ms;
            }
        } else if(got == sizeof(config_v1)) {
            memcpy(&config_v1, &config, sizeof(config_v1));
            if(config_v1.version == 1U) {
                if(config_v1.tone_idx < COUNT_OF(morse_flipper_tones)) {
                    app->tone_idx = config_v1.tone_idx;
                }

                if(config_v1.keyer_mode >= MorseKeyerModeStraight &&
                   config_v1.keyer_mode <= MorseKeyerModeKeyahead) {
                    app->keyer_mode = config_v1.keyer_mode;
                }
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
        .handedness = app->handedness,
        .trainer_lesson = morse_trainer_lesson(&app->trainer),
        .trainer_group_size = morse_trainer_group_size(&app->trainer),
        .trainer_session_groups = morse_trainer_session_groups(&app->trainer),
        .spare0 = app->input_source,
        .local_dit_ms = app->trainer.local_dit_ms,
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

static void morse_flipper_append_text(char* dst, size_t dst_sz, const char* add) {
    size_t len;
    size_t add_len;

    if(dst == NULL || dst_sz < 2U || add == NULL || add[0] == '\0') return;

    len = strlen(dst);
    add_len = strlen(add);
    if(add_len >= dst_sz) {
        memcpy(dst, add + add_len - (dst_sz - 1U), dst_sz - 1U);
        dst[dst_sz - 1U] = '\0';
        return;
    }

    if(len + add_len >= dst_sz) {
        size_t drop = len + add_len - (dst_sz - 1U);
        memmove(dst, dst + drop, len - drop + 1U);
        len = strlen(dst);
    }

    memcpy(dst + len, add, add_len + 1U);
}

static void morse_flipper_decoder_drain_into(
    MorseFlipperCwDecoder* decoder,
    char* out,
    size_t out_sz) {
    if(decoder == NULL || out == NULL || out_sz < 2U) return;

    if(morse_flipper_cw_decoder_output(decoder)[0] != '\0') {
        morse_flipper_append_text(out, out_sz, morse_flipper_cw_decoder_output(decoder));
        morse_flipper_cw_decoder_clear_output(decoder);
    }
}

#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_RADIO
static char morse_flipper_tx_symbol(const MorseFlipperApp* app) {
    if(app->note_sources[1] != 0U) return '.';
    if(app->note_sources[2] != 0U) return '-';
    if(app->note_sources[0] != 0U) return 'K';
    return '?';
}
#endif

#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_DECODERS
static bool morse_flipper_tx_decoder_allowed(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->trainer_playback_active || app->straight_playback_active) return false;
    if(app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app)) return false;
    return true;
}
#endif

static void morse_flipper_feed_tx_edge(MorseFlipperApp* app, bool level, uint32_t now_ms) {
    uint32_t dt;

    if(app == NULL) return;
    if(level == app->rf_tx_level) return;

    if(app->rf_tx_edge_at != 0U) {
        dt = now_ms - app->rf_tx_edge_at;
#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_DECODERS
        if(dt != 0U && morse_flipper_tx_decoder_allowed(app)) {
            if(app->rf_tx_level) {
                morse_flipper_cw_decoder_feed_mark(&app->tx_decoder, (uint16_t)dt);
            } else {
                morse_flipper_cw_decoder_feed_space(&app->tx_decoder, (uint16_t)dt);
            }
            morse_flipper_decoder_drain_into(&app->tx_decoder, app->rf_tx_text, sizeof(app->rf_tx_text));
        }
#else
        UNUSED(dt);
#endif
    }

    app->rf_tx_level = level;
    app->rf_tx_edge_at = now_ms;
    app->rf_tx_gap_flushed = level;
#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_RADIO
    morse_flipper_rf_handle_tx(&app->rf, level, morse_flipper_tx_symbol(app));
#endif
}

static void morse_flipper_feed_gpio_edge(MorseFlipperApp* app, bool level, uint32_t now_ms) {
    uint32_t dt;

    if(app == NULL) return;
    if(level == app->gpio_level) return;

    if(app->gpio_edge_at != 0U) {
        dt = now_ms - app->gpio_edge_at;
        if(dt != 0U) {
            if(app->gpio_level) {
                morse_flipper_cw_decoder_feed_mark(&app->gpio_decoder, (uint16_t)dt);
            } else {
                morse_flipper_cw_decoder_feed_space(&app->gpio_decoder, (uint16_t)dt);
            }
            morse_flipper_decoder_drain_into(&app->gpio_decoder, app->gpio_text, sizeof(app->gpio_text));
        }
    }

    app->gpio_level = level;
    app->gpio_edge_at = now_ms;
    app->gpio_gap_flushed = level;
}

static void morse_flipper_feed_straight_mark(MorseFlipperApp* app, uint16_t mark_ms) {
    char elem;

    if(app == NULL || mark_ms == 0U) return;

    elem = mark_ms >= (morse_flipper_current_dit_ms(app) * 2U) ? '-' : '.';
    morse_flipper_straight_trainer_feed(&app->straight_trainer, elem, mark_ms);
    app->straight_last_input_at = furi_get_tick();

    if(strlen(morse_flipper_straight_trainer_answer(&app->straight_trainer)) >=
       strlen(morse_flipper_straight_trainer_target_morse(&app->straight_trainer))) {
        app->straight_wait_answer = false;
        app->straight_done = true;
        app->straight_trainer.active = false;
        morse_trainer_note_straight_attempt(
            &app->straight_stats,
            morse_flipper_straight_trainer_target_char(&app->straight_trainer),
            morse_flipper_straight_trainer_average_mark_error_ms(&app->straight_trainer),
            morse_flipper_straight_trainer_average_drift_percent(&app->straight_trainer),
            morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
            morse_flipper_straight_trainer_answer(&app->straight_trainer));
    }
}

static void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms) {
    MorseFlipperApp* app = ctx;

    if(app == NULL || duration_ms == 0U) return;

#if MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_DECODERS
    UNUSED(level);
    return;
#else
    morse_flipper_rf_capture_rx_timing(&app->rf, level, duration_ms);
    if(level) {
        morse_flipper_cw_decoder_feed_mark(&app->rf_decoder, duration_ms);
    } else {
        morse_flipper_cw_decoder_feed_space(&app->rf_decoder, duration_ms);
    }
    morse_flipper_decoder_drain_into(&app->rf_decoder, app->rf_rx_text, sizeof(app->rf_rx_text));
#endif
}

static void morse_flipper_tone_stop(MorseFlipperApp* app) {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    app->tone_on = false;
    app->speaker_owned = false;
    app->speaker_busy = false;
    app->speaker_hz = 0.0f;
}

static void morse_flipper_tone_start(MorseFlipperApp* app) {
    float hz;

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

    hz = morse_flipper_active_tone_hz(app);
    furi_hal_speaker_start(hz, MORSE_FLIPPER_VOLUME);
    app->tone_on = true;
    app->speaker_hz = hz;
    app->speaker_busy = false;
}

static void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    bool want_tone = morse_flipper_any_active_notes(app) || (app->preview_ticks > 0U) ||
                     app->trainer_playback_mark || app->straight_playback_mark ||
                     app->session_result_tone;

    if(want_tone) {
        float hz = morse_flipper_active_tone_hz(app);

        morse_flipper_tone_start(app);
        if(app->tone_on && app->speaker_owned && furi_hal_speaker_is_mine() &&
           app->speaker_hz != hz) {
            furi_hal_speaker_start(hz, MORSE_FLIPPER_VOLUME);
            app->speaker_hz = hz;
            app->speaker_busy = false;
        }
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
    if(app->screen == screen) return;

    if(app->screen == MorseFlipperScreenSession && screen != MorseFlipperScreenSession) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenStraight && screen != MorseFlipperScreenStraight) {
        morse_flipper_reset_straight_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenRf && screen != MorseFlipperScreenRf) {
        app->rf_live_active = false;
        app->rf_manual_active = false;
        app->rf_rx_arm_until = 0U;
        morse_flipper_radio_sync_live(&app->radio, morse_flipper_rf_frequency_hz(&app->rf), false, false);
        morse_flipper_radio_set_tx_level(&app->radio, false);
    }

    morse_flipper_clear_button_keying(app, now_ms);

    if(screen == MorseFlipperScreenSession && app->screen != MorseFlipperScreenSession) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(screen == MorseFlipperScreenStraight && app->screen != MorseFlipperScreenStraight) {
        morse_flipper_reset_straight_state(app, now_ms);
    }

    if(screen == MorseFlipperScreenTrace && app->screen != MorseFlipperScreenTrace) {
        app->rf_tx_text[0] = '\0';
        app->gpio_text[0] = '\0';
        morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
        morse_flipper_cw_decoder_init(&app->gpio_decoder, morse_flipper_current_dit_ms(app));
        app->rf_tx_edge_at = 0U;
        app->gpio_edge_at = 0U;
        app->rf_tx_gap_flushed = true;
        app->gpio_gap_flushed = true;
    }

    app->screen = screen;
    if(screen == MorseFlipperScreenHome) {
        morse_flipper_poll(app);
    }
    morse_flipper_view_dirty(app);
}

static void morse_flipper_toggle_source(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    morse_flipper_drop_live_keying_for_playback(app, now_ms);

    if(app->input_source == MorseFlipperInputSourceStraight) {
        app->input_source = MorseFlipperInputSourcePaddle;
    } else if(app->input_source == MorseFlipperInputSourcePaddle) {
        app->input_source = MorseFlipperInputSourceButtons;
    } else {
        app->input_source = MorseFlipperInputSourceStraight;
    }

    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_cycle_pc_mode(MorseFlipperApp* app, int dir) {
    int next = (int)app->pc_mode + dir;

    if(next < (int)MorseFlipperPcModeOff) {
        next = (int)MorseFlipperPcModeKeyboard;
    } else if(next > (int)MorseFlipperPcModeKeyboard) {
        next = (int)MorseFlipperPcModeOff;
    }

    morse_flipper_set_pc_mode(app, (uint8_t)next);
    morse_flipper_view_dirty(app);
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

    morse_flipper_view_dirty(app);
}

static bool morse_flipper_training_playback_active(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenStraight) return app->straight_playback_active;
    return app->screen == MorseFlipperScreenSession && app->trainer_playback_active;
}

static bool morse_flipper_straight_answer_down(const MorseFlipperApp* app) {
    if(app == NULL) return false;

    if(app->input_source == MorseFlipperInputSourceStraight) {
        return morse_flipper_straight_down();
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        return morse_flipper_logical_dit_down(app) || morse_flipper_logical_dah_down(app);
    }

    return false;
}

static void morse_flipper_reset_session_runtime(MorseFlipperApp* app) {
    if(app == NULL) return;

    app->session_started = false;
    app->session_round_pending = false;
    app->session_result_hold = false;
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_last_input_at = 0U;
    app->session_result_until = 0U;
    app->session_next_group_at = 0U;
}

static void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->straight_playback_active = false;
    app->straight_playback_mark = false;
    app->straight_wait_answer = false;
    app->straight_done = false;
    app->straight_key_down = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = 0U;
    app->straight_last_input_at = 0U;
    app->straight_mark_started_at = 0U;
    app->straight_trainer.active = false;
    app->straight_trainer.answer[0] = '\0';
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_update_sidetone(app);
    morse_flipper_clear_button_keying(app, now_ms);
}

static void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->trainer_playback_active = false;
    app->trainer_playback_mark = false;
    app->session_log_pending = false;

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    morse_flipper_reset_session_runtime(app);
    morse_trainer_reset_session(&app->trainer);

    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_update_sidetone(app);
}

static bool morse_flipper_session_repeat_active(const MorseFlipperApp* app) {
    return app && app->screen == MorseFlipperScreenSession && app->session_started &&
           !app->trainer_playback_active && app->session_next_group_at == 0U &&
           strcmp(morse_trainer_phase_name(&app->trainer), "repeat") == 0;
}

static bool morse_flipper_session_idle_view(const MorseFlipperApp* app) {
    const char* ph;

    if(!app || app->screen != MorseFlipperScreenSession) return false;

    ph = morse_trainer_phase_name(&app->trainer);
    return !app->session_started || (!app->trainer_playback_active && !app->session_round_pending &&
           !app->session_result_hold && !app->session_result_tone &&
           app->session_next_group_at == 0U && !morse_trainer_session_active(&app->trainer) &&
           strcmp(ph, "repeat") != 0);
}

static bool morse_flipper_session_live_keying(const MorseFlipperApp* app) {
    return app &&
           (app->paddle_sources[MorseKeyerPaddleDit] != 0U ||
            app->paddle_sources[MorseKeyerPaddleDah] != 0U ||
            app->note_sources[0] != 0U || app->note_sources[1] != 0U ||
            app->note_sources[2] != 0U);
}

static void morse_flipper_queue_session_feedback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || !app->session_round_pending) return;

    app->session_round_pending = false;
    app->session_result_hold = true;
    app->session_result_tone = true;
    app->session_result_good = !morse_trainer_last_failed(&app->trainer);
    app->session_result_until = now_ms + MORSE_FLIPPER_SESSION_RESULT_MS;
    app->session_next_group_at =
        morse_trainer_session_active(&app->trainer) ? (now_ms + MORSE_FLIPPER_SESSION_ADVANCE_MS) :
                                                      0U;
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_drop_live_keying_for_playback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_GPIO_DIT, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_GPIO_DAH, false, now_ms);
}

static void morse_flipper_begin_group_playback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) {
        return;
    }

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    app->trainer_playback_active = true;
    app->trainer_playback_mark = false;
    app->trainer_char_idx = 0U;
    app->trainer_mark_idx = 0U;
    app->trainer_next_at = now_ms;
    if(app->screen == MorseFlipperScreenSession) {
        app->session_round_pending = true;
        app->session_result_hold = false;
        app->session_result_tone = false;
        app->session_result_good = false;
        app->session_last_input_at = now_ms;
        app->session_result_until = 0U;
        app->session_next_group_at = 0U;
    }
    morse_flipper_view_dirty(app);
}

static void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_reset_straight_state(app, now_ms);
    morse_flipper_straight_trainer_start(
        &app->straight_trainer, morse_trainer_charset(&app->trainer), morse_flipper_current_dit_ms(app));
    app->straight_playback_active = true;
    app->straight_playback_mark = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = now_ms;
    morse_flipper_view_dirty(app);
}

static void morse_flipper_start_session(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_reset_session_state(app, now_ms);
    morse_trainer_start_session(&app->trainer);
    app->session_started = true;
    app->session_log_pending = true;
    morse_flipper_begin_group_playback(app, now_ms);
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
        morse_flipper_view_dirty(app);
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
            morse_trainer_finish_listen(&app->trainer);
            if(app->screen == MorseFlipperScreenSession) {
                app->session_last_input_at = now_ms;
            }
        }
        morse_flipper_view_dirty(app);
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
    morse_flipper_view_dirty(app);
}

static void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms) {
    const char* morse;
    uint32_t dit_ms;
    size_t len;

    if(app == NULL) return;

    if(app->straight_playback_active && now_ms >= app->straight_next_at) {
        morse = morse_flipper_straight_trainer_target_morse(&app->straight_trainer);
        dit_ms = morse_flipper_current_dit_ms(app);
        len = strlen(morse);

        if(!app->straight_playback_mark) {
            if(app->straight_mark_idx >= len) {
                app->straight_playback_active = false;
                app->straight_wait_answer = true;
                app->straight_next_at = 0U;
                morse_flipper_view_dirty(app);
                return;
            }

            app->straight_playback_mark = true;
            app->straight_next_at =
                now_ms + (morse[app->straight_mark_idx] == '-' ? (dit_ms * 3U) : dit_ms);
            morse_flipper_view_dirty(app);
            return;
        }

        app->straight_playback_mark = false;
        app->straight_mark_idx++;
        if(app->straight_mark_idx >= len) {
            app->straight_playback_active = false;
            app->straight_wait_answer = true;
            app->straight_next_at = 0U;
        } else {
            app->straight_next_at = now_ms + dit_ms;
        }
        morse_flipper_view_dirty(app);
    }

    if(app->straight_wait_answer &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
        uint32_t settle = morse_flipper_current_dit_ms(app) * 6U;

        if(settle < MORSE_FLIPPER_STRAIGHT_SETTLE_MS) settle = MORSE_FLIPPER_STRAIGHT_SETTLE_MS;
        if(now_ms - app->straight_last_input_at >= settle) {
            app->straight_wait_answer = false;
            app->straight_done = true;
            app->straight_trainer.active = false;
            morse_trainer_note_straight_attempt(
                &app->straight_stats,
                morse_flipper_straight_trainer_target_char(&app->straight_trainer),
                morse_flipper_straight_trainer_average_mark_error_ms(&app->straight_trainer),
                morse_flipper_straight_trainer_average_drift_percent(&app->straight_trainer),
                morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
                morse_flipper_straight_trainer_answer(&app->straight_trainer));
            morse_flipper_view_dirty(app);
        }
    }
}

static void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms) {
    bool hold_tx;
    bool level;
    uint32_t rx_idle_ms;

    if(app == NULL) return;

    if(!app->rf_live_active) {
#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_RADIO
        morse_flipper_radio_sync_live(
            &app->radio, morse_flipper_rf_frequency_hz(&app->rf), false, false);
        morse_flipper_radio_set_tx_level(&app->radio, false);
#endif
        return;
    }

    level = morse_flipper_any_active_notes(app);
    rx_idle_ms = (uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_RX_ARM_DITS;
    if(rx_idle_ms < MORSE_FLIPPER_RF_RX_ARM_MIN_MS) rx_idle_ms = MORSE_FLIPPER_RF_RX_ARM_MIN_MS;
    if(level) {
        app->rf_tx_tail_until =
            now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_TX_TAIL_DITS);
        app->rf_rx_arm_until = now_ms + rx_idle_ms;
    }

    hold_tx =
        level || now_ms < app->rf_tx_tail_until || now_ms < app->rf_rx_arm_until;
#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_RADIO
    morse_flipper_radio_sync_live(
        &app->radio, morse_flipper_rf_frequency_hz(&app->rf), true, hold_tx);
    morse_flipper_radio_set_tx_level(&app->radio, level);
#else
    UNUSED(hold_tx);
#endif
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
    case 2:
        next = (int)morse_trainer_session_groups(&app->trainer) + dir;
        morse_trainer_set_session_groups(&app->trainer, (uint8_t)next);
        break;
    default:
        next = (int)app->trainer.custom_set_idx + dir;
        if(next < 0) {
            next = (int)app->custom_sets.count;
        } else if(next > (int)app->custom_sets.count) {
            next = 0;
        }
        app->trainer.custom_set_idx = (uint8_t)next;
        morse_flipper_apply_trainer_charset_choice(app);
        break;
    }

    morse_flipper_view_dirty(app);
}

static void morse_flipper_tick_session(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t dt;

    if(app == NULL || app->screen != MorseFlipperScreenSession || !app->session_started) return;

    if(app->session_result_tone && now_ms >= app->session_result_until) app->session_result_tone = false;

    if(app->session_next_group_at != 0U && now_ms >= app->session_next_group_at) {
        app->session_next_group_at = 0U;
        if(morse_trainer_next_session_group(&app->trainer)) {
            morse_flipper_begin_group_playback(app, now_ms);
        } else {
            morse_flipper_view_dirty(app);
        }
        return;
    }

    if(!app->session_round_pending || app->trainer_playback_active) return;

    if(strcmp(morse_trainer_phase_name(&app->trainer), "done") == 0) {
        morse_flipper_queue_session_feedback(app, now_ms);
        return;
    }

    if(!morse_flipper_session_repeat_active(app) || morse_trainer_answer(&app->trainer)[0] == '\0')
        return;

    if(morse_flipper_session_live_keying(app)) {
        app->session_last_input_at = now_ms;
        return;
    }

    dt = morse_flipper_current_dit_ms(app) * 6U;
    if(dt < MORSE_FLIPPER_SESSION_SETTLE_MS) dt = MORSE_FLIPPER_SESSION_SETTLE_MS;

    if(now_ms - app->session_last_input_at < dt) return;

    morse_trainer_score_repeat(&app->trainer);
    morse_flipper_queue_session_feedback(app, now_ms);
}

static void morse_flipper_leave_session(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    morse_flipper_reset_session_state(app, now_ms);
    morse_flipper_scene_back(app);
}

static void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenSession) {
        morse_flipper_leave_session(app, now_ms);
        return;
    }

    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_scene_back(app);
}

static void morse_flipper_apply_trainer_charset_choice(MorseFlipperApp* app) {
    if(app == NULL) {
        return;
    }

    if(app->trainer.custom_set_idx == 0U || app->custom_sets.count == 0U) {
        app->trainer.custom_name[0] = '\0';
        app->trainer.charset_override[0] = '\0';
        return;
    }

    if(app->trainer.custom_set_idx > app->custom_sets.count) {
        app->trainer.custom_set_idx = 0U;
        app->trainer.custom_name[0] = '\0';
        app->trainer.charset_override[0] = '\0';
        return;
    }

    strncpy(
        app->trainer.custom_name,
        app->custom_sets.sets[app->trainer.custom_set_idx - 1U].name,
        sizeof(app->trainer.custom_name) - 1U);
    app->trainer.custom_name[sizeof(app->trainer.custom_name) - 1U] = '\0';
    strncpy(
        app->trainer.charset_override,
        app->custom_sets.sets[app->trainer.custom_set_idx - 1U].chars,
        sizeof(app->trainer.charset_override) - 1U);
    app->trainer.charset_override[sizeof(app->trainer.charset_override) - 1U] = '\0';
}

static void morse_flipper_cycle_mode(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    app->keyer_mode = morse_keyer_next_ui_mode(app->keyer_mode);
    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_view_dirty(app);
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

    morse_flipper_save_config(app);
    morse_flipper_resync_button_paddles(app, now_ms);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
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
        if((app->screen == MorseFlipperScreenTrainer ||
            app->screen == MorseFlipperScreenSession) &&
           event.type == MorseKeyerEventPress &&
           strcmp(morse_trainer_phase_name(&app->trainer), "repeat") == 0) {
            morse_trainer_feed_element(
                &app->trainer, event.paddle == MorseKeyerPaddleDit ? '.' : '-');
        }

        if(app->screen == MorseFlipperScreenSession &&
           strcmp(morse_trainer_phase_name(&app->trainer), "repeat") == 0) {
            app->session_last_input_at = furi_get_tick();
        }

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
    bool btn_src = morse_flipper_button_source_active(app);
    bool btn_str = morse_flipper_button_straight_keying_active(app);
    bool btn_pad = morse_flipper_button_paddle_keying_active(app);

    if(btn_src && event->key == InputKeyLeft) {
        if(event->type == InputTypePress) {
            app->left_down = true;
        } else if(event->type == InputTypeRelease) {
            app->left_down = false;
        } else if(event->type == InputTypeLong) {
            morse_flipper_leave_live_screen(app, now_ms);
        }
        return;
    }

    if(btn_src && event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            app->ok_down = true;
            if(btn_str) {
                morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, true);
            } else if(btn_pad) {
                morse_flipper_resync_button_paddles(app, now_ms);
            }
        } else if(event->type == InputTypeRelease) {
            app->ok_down = false;
            if(btn_str) {
                morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
            } else if(btn_pad) {
                morse_flipper_resync_button_paddles(app, now_ms);
            }
        }
        return;
    }

    if(btn_pad && event->key == InputKeyBack) {
        if(event->type == InputTypePress) {
            app->back_down = true;
            morse_flipper_resync_button_paddles(app, now_ms);
        } else if(event->type == InputTypeRelease) {
            app->back_down = false;
            morse_flipper_resync_button_paddles(app, now_ms);
        }
        return;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_live_screen(app, now_ms);
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
        morse_flipper_scene_back(app);
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
    bool straight_active = false;
    bool dit_active = false;
    bool dah_active = false;

    if(app->input_source == MorseFlipperInputSourceStraight) {
        straight_active = morse_flipper_straight_down();
    } else if(app->input_source == MorseFlipperInputSourcePaddle) {
        dit_active = morse_flipper_logical_dit_down(app);
        dah_active = morse_flipper_logical_dah_down(app);
    }

    if(morse_flipper_training_playback_active(app) ||
       (app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app))) {
        straight_active = false;
        dit_active = false;
        dah_active = false;
    }

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
    morse_flipper_view_dirty(app);
}

static void morse_flipper_poll(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();
    bool old_tone = app->tone_on;
    bool old_busy = app->speaker_busy;
    uint8_t old_mask = app->input_mask;
    bool old_transport = app->transport_connected;
    bool raw_straight;
    bool tx_now;

    if(app->pc_mode == MorseFlipperPcModeMidi && app->midi_rx_pending) {
        app->midi_rx_pending = false;
        morse_flipper_handle_midi_rx(app);
    }

    if(app->screen == MorseFlipperScreenSession && app->session_started && app->session_log_pending &&
       !morse_trainer_session_active(&app->trainer) &&
       strcmp(morse_trainer_phase_name(&app->trainer), "done") == 0) {
        if(morse_trainer_session_completed(&app->trainer)) {
            morse_trainer_append_session_log(&app->trainer);
            morse_trainer_load_session_lines(&app->session_lines);
            app->session_line_idx =
                app->session_lines.count == 0U ? 0U : (app->session_lines.count - 1U);
        }
        app->session_log_pending = false;
    }

    if(app->screen == MorseFlipperScreenSession && app->session_started &&
       strcmp(morse_trainer_phase_name(&app->trainer), "repeat") == 0) {
        morse_trainer_tick(&app->trainer, MORSE_FLIPPER_POLL_MS);
    }

    morse_flipper_tick_session(app, now_ms);
    morse_flipper_tick_straight(app, now_ms);

    app->transport_connected = morse_flipper_transport_connected(app);
    if(!old_transport && app->transport_connected) {
        morse_flipper_resync_transport_notes(app);
    }

    app->input_mask = morse_flipper_read_input_mask(app);
    morse_flipper_sync_gpio_inputs(app, now_ms);
    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);

    raw_straight = morse_flipper_straight_down();
    morse_flipper_feed_gpio_edge(app, raw_straight, now_ms);

    if(!app->gpio_level && !app->gpio_gap_flushed && app->gpio_edge_at != 0U) {
        uint32_t gap = now_ms - app->gpio_edge_at;
        if(gap >= (morse_flipper_current_dit_ms(app) * 5U) / 2U) {
            morse_flipper_cw_decoder_feed_space(&app->gpio_decoder, (uint16_t)gap);
            morse_flipper_decoder_drain_into(&app->gpio_decoder, app->gpio_text, sizeof(app->gpio_text));
            app->gpio_gap_flushed = true;
        }
    }

    raw_straight = morse_flipper_straight_answer_down(app);
    if(app->screen == MorseFlipperScreenStraight && !raw_straight &&
       app->straight_mark_started_at != 0U && app->straight_key_down) {
        app->straight_key_down = false;
        morse_flipper_feed_straight_mark(app, (uint16_t)(now_ms - app->straight_mark_started_at));
        app->straight_mark_started_at = 0U;
    } else if(app->screen == MorseFlipperScreenStraight && raw_straight &&
              !app->straight_key_down && app->straight_wait_answer) {
        app->straight_key_down = true;
        app->straight_mark_started_at = now_ms;
    }

    tx_now = morse_flipper_any_active_notes(app);
    morse_flipper_feed_tx_edge(app, tx_now, now_ms);
    if(!app->rf_tx_level && !app->rf_tx_gap_flushed && app->rf_tx_edge_at != 0U) {
        uint32_t gap = now_ms - app->rf_tx_edge_at;
        if(gap >= (morse_flipper_current_dit_ms(app) * 5U) / 2U) {
#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_DECODERS
            if(morse_flipper_tx_decoder_allowed(app)) {
                morse_flipper_cw_decoder_feed_space(&app->tx_decoder, (uint16_t)gap);
                morse_flipper_decoder_drain_into(&app->tx_decoder, app->rf_tx_text, sizeof(app->rf_tx_text));
            }
#endif
            app->rf_tx_gap_flushed = true;
        }
    }

    morse_flipper_tick_live_rf(app, now_ms);
#if !MORSE_FLIPPER_RF_EXPERIMENT_DISABLE_DECODERS
    morse_flipper_radio_drain_rx(&app->radio);
#endif
    morse_flipper_update_sidetone(app);

    if(old_tone != app->tone_on || old_busy != app->speaker_busy || old_mask != app->input_mask ||
       old_transport != app->transport_connected) {
        morse_flipper_view_dirty(app);
    }
}

static void morse_flipper_draw_left_exit_hint(Canvas* canvas) {
    canvas_draw_box(canvas, 124, 32, 1, 1);
    canvas_draw_box(canvas, 125, 31, 1, 3);
    canvas_draw_box(canvas, 126, 30, 1, 5);
    canvas_draw_box(canvas, 127, 29, 1, 7);
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
    char browse_line[32];

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
        canvas_draw_str(canvas, 8, 14, "Free Practice");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 8, 28, run_line);
        if(app->input_source == MorseFlipperInputSourceButtons &&
           morse_flipper_button_paddle_keying_active(app)) {
            canvas_draw_str(canvas, 8, 40, morse_flipper_button_map_line(app));
        } else {
            canvas_draw_str(canvas, 8, 40, morse_flipper_free_practice_hint(app));
        }
        canvas_draw_str(canvas, 8, 52, morse_flipper_input_line(app, input_line, sizeof(input_line)));
        canvas_draw_str(canvas, 8, 62, morse_flipper_status_line(app));
        if(morse_flipper_button_paddle_keying_active(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
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
            canvas_draw_str(canvas, 8, 63, morse_flipper_pc_hint(app));
        }
        if(morse_flipper_button_paddle_keying_active(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
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
        canvas_set_font(canvas, FontSecondary);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 12, "Koch Setup");
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
            "%c size %u",
            app->trainer_row == 1U ? '>' : ' ',
            (unsigned)morse_trainer_group_size(&app->trainer));
        snprintf(
            trainer_line3,
            sizeof(trainer_line3),
            "%c groups %u",
            app->trainer_row == 2U ? '>' : ' ',
            (unsigned)morse_trainer_session_groups(&app->trainer));
        snprintf(
            tone_line,
            sizeof(tone_line),
            "%c chars %s",
            app->trainer_row == 3U ? '>' : ' ',
            app->trainer.custom_set_idx == 0U ? "lesson" : app->trainer.custom_name);
        canvas_draw_str(canvas, 8, 24, trainer_line);
        canvas_draw_str(canvas, 8, 34, trainer_line2);
        canvas_draw_str(canvas, 8, 44, trainer_line3);
        canvas_draw_str(canvas, 8, 54, tone_line);
        canvas_draw_str(canvas, 8, 64, "OK sess  Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenStraight) {
        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "1ch %c %s",
            morse_flipper_straight_trainer_target_char(&app->straight_trainer),
            morse_flipper_straight_trainer_target_morse(&app->straight_trainer));
        snprintf(
            trainer_line2,
            sizeof(trainer_line2),
            "ans %s",
            morse_flipper_straight_trainer_answer(&app->straight_trainer));
        snprintf(
            trainer_line3,
            sizeof(trainer_line3),
            "err %u drift %u%%",
            (unsigned)morse_flipper_straight_trainer_average_mark_error_ms(&app->straight_trainer),
            (unsigned)morse_flipper_straight_trainer_average_drift_percent(&app->straight_trainer));
        snprintf(
            tone_line,
            sizeof(tone_line),
            "best %c/%u  worst %c/%u",
            app->straight_stats.have_best ? app->straight_stats.best_char : '-',
            (unsigned)app->straight_stats.best_error_ms,
            app->straight_stats.have_worst ? app->straight_stats.worst_char : '-',
            (unsigned)app->straight_stats.worst_error_ms);
        canvas_draw_str(canvas, 2, 10, trainer_line);
        canvas_draw_str(canvas, 2, 22, trainer_line2);
        canvas_draw_str(canvas, 2, 34, trainer_line3);
        canvas_draw_str(
            canvas,
            2,
            46,
            morse_flipper_straight_trainer_error_bars(&app->straight_trainer)[0] != '\0' ?
                morse_flipper_straight_trainer_error_bars(&app->straight_trainer) :
                morse_flipper_straight_trainer_timing_view(&app->straight_trainer));
        canvas_draw_str(canvas, 2, 58, tone_line);
        if(app->straight_playback_active) {
            canvas_draw_str(canvas, 2, 64, "playing  Bk back");
        } else if(app->straight_wait_answer) {
            canvas_draw_str(
                canvas,
                2,
                64,
                app->input_source == MorseFlipperInputSourceButtons ? "OK key  Bk back" :
                (app->input_source == MorseFlipperInputSourceStraight ? "P3 key  Bk back" :
                 (app->input_source == MorseFlipperInputSourcePaddle ? "P7/P5 key Bk back" :
                                                                      "need key Bk back")));
        } else {
            canvas_draw_str(canvas, 2, 64, "OK play  Bk back");
        }
        return;
    }

    if(app->screen == MorseFlipperScreenSession) {
        canvas_set_font(canvas, FontSecondary);
        if(morse_flipper_session_idle_view(app)) {
            snprintf(
                trainer_line,
                sizeof(trainer_line),
                "sess 0/%u idle",
                (unsigned)morse_trainer_session_total(&app->trainer));
            snprintf(
                trainer_line2,
                sizeof(trainer_line2),
                "lesson %u  size %u",
                (unsigned)morse_trainer_lesson(&app->trainer),
                (unsigned)morse_trainer_group_size(&app->trainer));
            canvas_draw_str(canvas, 8, 10, trainer_line);
            canvas_draw_str(canvas, 8, 22, trainer_line2);
            canvas_draw_str(canvas, 8, 34, "fail 0 miss 0");
            canvas_draw_str(canvas, 8, 64, "OK start hold OK 1ch");
            return;
        }

        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "sess %u/%u %s",
            (unsigned)morse_trainer_session_index(&app->trainer),
            (unsigned)morse_trainer_session_total(&app->trainer),
            morse_trainer_phase_name(&app->trainer));
        snprintf(
            trainer_line2,
            sizeof(trainer_line2),
            "lesson %u  size %u",
            (unsigned)morse_trainer_lesson(&app->trainer),
            (unsigned)morse_trainer_group_size(&app->trainer));
        snprintf(
            trainer_line3,
            sizeof(trainer_line3),
            "fail %u miss %u%s",
            (unsigned)morse_trainer_session_fail_count(&app->trainer),
            (unsigned)morse_trainer_session_consecutive_missed(&app->trainer),
            morse_trainer_session_aborted(&app->trainer) ? " abort" : "");
        canvas_draw_str(canvas, 8, 10, trainer_line);
        canvas_draw_str(canvas, 8, 22, trainer_line2);
        canvas_draw_str(canvas, 8, 34, trainer_line3);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 50, morse_trainer_last_group(&app->trainer));
        canvas_set_font(canvas, FontSecondary);
        if(strcmp(morse_trainer_phase_name(&app->trainer), "repeat") == 0 ||
           strcmp(morse_trainer_phase_name(&app->trainer), "done") == 0) {
            snprintf(
                trainer_line,
                sizeof(trainer_line),
                "ans %s %d%s",
                morse_trainer_answer(&app->trainer),
                (int)morse_trainer_last_score(&app->trainer),
                morse_trainer_last_failed(&app->trainer) ? " fail" : "");
            canvas_draw_str(canvas, 2, 64, trainer_line);
            if(morse_flipper_button_paddle_keying_active(app)) {
                morse_flipper_draw_left_exit_hint(canvas);
            }
        } else {
            canvas_draw_str(
                canvas,
                8,
                64,
                app->trainer_playback_active ? "playing" : "OK start hold OK 1ch");
        }
        return;
    }

    if(app->screen == MorseFlipperScreenBrowse) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 14, "Session Logs");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            browse_line,
            sizeof(browse_line),
            "%u/%u",
            (unsigned)(app->session_line_idx + 1U),
            (unsigned)app->session_lines.count);
        canvas_draw_str(canvas, 96, 14, browse_line);
        if(app->session_lines.count != 0U) {
            canvas_draw_str(canvas, 2, 32, app->session_lines.lines[app->session_line_idx]);
        } else {
            canvas_draw_str(canvas, 2, 32, "no saved sessions");
        }
        canvas_draw_str(canvas, 2, 62, "U/D pick  R/Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenRf) {
        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "rf %s %s",
            morse_flipper_rf_frequency_text(&app->rf),
            app->rf_live_active ? "live" : "idle");
        canvas_draw_str(canvas, 2, 10, trainer_line);
        if(app->rf_manual_active) {
            snprintf(
                trainer_line2,
                sizeof(trainer_line2),
                "khz %s [%u]",
                app->rf_edit_khz,
                (unsigned)app->rf_edit_digit);
            canvas_draw_str(canvas, 2, 22, trainer_line2);
            canvas_draw_str(canvas, 2, 34, "L/R move U/D digit");
            canvas_draw_str(canvas, 2, 46, "OK apply");
            canvas_draw_str(canvas, 2, 64, "Bk cancel");
            return;
        }

        if(app->rf_live_active) {
            snprintf(
                trainer_line2,
                sizeof(trainer_line2),
                "src %s mode %s",
                morse_flipper_source_short_name(app),
                morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));
            canvas_draw_str(canvas, 2, 22, trainer_line2);
            snprintf(
                trainer_line3,
                sizeof(trainer_line3),
                "rx %.28s",
                app->rf_rx_text[0] ? app->rf_rx_text : "-");
            canvas_draw_str(canvas, 2, 34, trainer_line3);
            snprintf(
                tone_line,
                sizeof(tone_line),
                "tx %.28s",
                app->rf_tx_text[0] ? app->rf_tx_text : "-");
            canvas_draw_str(canvas, 2, 46, tone_line);
            canvas_draw_str(
                canvas,
                2,
                58,
                morse_flipper_rf_last_error(&app->rf)[0] ? morse_flipper_rf_last_error(&app->rf) :
                                                           morse_flipper_rf_rx_log_line(&app->rf, morse_flipper_rf_rx_log_count(&app->rf) ?
                                                                                                 (morse_flipper_rf_rx_log_count(&app->rf) - 1U) :
                                                                                                 0U));
            canvas_draw_str(
                canvas,
                2,
                64,
                morse_flipper_button_paddle_keying_active(app) ? "live key hold L" : "live key Bk back");
            if(morse_flipper_button_paddle_keying_active(app)) {
                morse_flipper_draw_left_exit_hint(canvas);
            }
            return;
        }

        canvas_draw_str(canvas, 2, 22, "L/R step  U band");
        canvas_draw_str(canvas, 2, 34, "OK edit  D live");
        canvas_draw_str(canvas, 2, 46, morse_flipper_rf_last_error(&app->rf)[0] ?
                                            morse_flipper_rf_last_error(&app->rf) :
                                            morse_flipper_rf_manual_khz_text(&app->rf));
        canvas_draw_str(canvas, 2, 58, app->gpio_text[0] ? app->gpio_text : "gpio -");
        canvas_draw_str(canvas, 2, 64, "Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        snprintf(
            trace_line1,
            sizeof(trace_line1),
            "tr %s %s %s",
            morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)),
            morse_flipper_source_short_name(app),
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
            "tx %.12s gp %.11s",
            app->rf_tx_text[0] ? app->rf_tx_text : "-",
            app->gpio_text[0] ? app->gpio_text : "-");

        canvas_draw_str(canvas, 2, 10, trace_line1);
        canvas_draw_str(canvas, 2, 20, morse_flipper_input_line(app, input_line, sizeof(input_line)));
        canvas_draw_str(canvas, 2, 30, trace_line2);
        canvas_draw_str(canvas, 2, 40, trace_line3);
        canvas_draw_str(canvas, 2, 50, trace_line4);
        canvas_draw_str(canvas, 2, 60, morse_flipper_trace_hint(app));
        if(morse_flipper_button_paddle_keying_active(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 14, "Main settings");

    snprintf(
        tone_line,
        sizeof(tone_line),
        "< %s > %s",
        morse_flipper_current_tone(app)->name,
        morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, morse_flipper_pc_state_name(app));
    canvas_draw_str(canvas, 8, 42, tone_line);
    canvas_draw_str(canvas, 8, 52, morse_flipper_status_line(app));
    canvas_draw_str(canvas, 8, 62, "L/R tone U src D/hD");
}

static void morse_flipper_scene_menu_pick(void* ctx, uint32_t idx) {
    MorseFlipperApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, idx);
}

static void morse_flipper_scene_show_live(MorseFlipperApp* app, uint8_t screen) {
    morse_flipper_enter_screen(app, screen, furi_get_tick());
    view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewLive);
}

static bool morse_flipper_live_input(InputEvent* event, void* ctx) {
    MorseFlipperApp* app = ctx;
    uint32_t now_ms = furi_get_tick();

    if(app->screen == MorseFlipperScreenPcKeys) {
        if(event->key == InputKeyUp &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            app->pc_keys_row = app->pc_keys_row == 0U ? 1U : 0U;
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyDown &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            app->pc_keys_row = app->pc_keys_row == 0U ? 1U : 0U;
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyLeft &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_cycle_pc_key_preset(app, -1);
            return true;
        }

        if(event->key == InputKeyRight &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_cycle_pc_key_preset(app, 1);
            return true;
        }

        if((event->key == InputKeyOk || event->key == InputKeyBack) &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }

        return false;
    }

    if(app->screen == MorseFlipperScreenPc) {
        if(event->key == InputKeyLeft || event->key == InputKeyOk || event->key == InputKeyBack) {
            morse_flipper_handle_active_keying_event(app, event);
            return true;
        }

        if(event->key == InputKeyUp && event->type == InputTypeShort) {
            morse_flipper_cycle_pc_mode(app, -1);
            return true;
        }

        if(event->key == InputKeyDown && event->type == InputTypeShort) {
            morse_flipper_cycle_pc_mode(app, 1);
            return true;
        }

        if(event->key == InputKeyRight && event->type == InputTypeLong) {
            if(app->pc_mode == MorseFlipperPcModeKeyboard)
                morse_flipper_scene_open(app, MorseFlipperScenePcKeys);
            return true;
        }

        return false;
    }

    if(app->screen == MorseFlipperScreenTrainer) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            morse_flipper_scene_open(app, MorseFlipperSceneSession);
            return true;
        }

        if(event->key == InputKeyUp &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            app->trainer_row = app->trainer_row == 0U ? 3U : (app->trainer_row - 1U);
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyDown &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            app->trainer_row = (app->trainer_row + 1U) % 4U;
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyLeft &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_cycle_trainer_value(app, -1);
            return true;
        }

        if(event->key == InputKeyRight &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_cycle_trainer_value(app, 1);
            return true;
        }

        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            app->trainer_playback_active = false;
            app->trainer_playback_mark = false;
            app->session_log_pending = false;
            morse_flipper_scene_back(app);
            return true;
        }

        return false;
    }

    if(app->screen == MorseFlipperScreenStraight) {
        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }

        if(app->straight_wait_answer && app->input_source == MorseFlipperInputSourceButtons &&
           event->key == InputKeyOk) {
            if(event->type == InputTypePress) {
                app->ok_down = true;
                app->straight_key_down = true;
                app->straight_mark_started_at = now_ms;
                morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, true);
                morse_flipper_view_dirty(app);
            } else if(event->type == InputTypeRelease) {
                uint16_t dt = 0U;

                app->ok_down = false;
                morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
                if(app->straight_key_down && app->straight_mark_started_at != 0U)
                    dt = (uint16_t)(now_ms - app->straight_mark_started_at);
                app->straight_key_down = false;
                app->straight_mark_started_at = 0U;
                if(dt != 0U) {
                    morse_flipper_feed_straight_mark(app, dt);
                    morse_flipper_view_dirty(app);
                }
            }
            return true;
        }

        if(!app->straight_playback_active && !app->straight_wait_answer && event->key == InputKeyOk &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_start_straight_round(app, now_ms);
            return true;
        }

        return false;
    }

    if(app->screen == MorseFlipperScreenSession) {
        if(morse_flipper_session_repeat_active(app)) {
            if(event->key == InputKeyLeft || event->key == InputKeyOk ||
               event->key == InputKeyBack) {
                morse_flipper_handle_active_keying_event(app, event);
                return true;
            }

            if(event->key == InputKeyBack &&
               (event->type == InputTypeShort || event->type == InputTypeLong) &&
               !morse_flipper_button_paddle_keying_active(app)) {
                morse_flipper_leave_session(app, now_ms);
                return true;
            }

            return false;
        }

        if(event->key == InputKeyOk && event->type == InputTypeShort &&
           !morse_trainer_session_active(&app->trainer) && !app->trainer_playback_active) {
            morse_flipper_start_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyOk && event->type == InputTypeLong &&
           !morse_trainer_session_active(&app->trainer) && !app->trainer_playback_active) {
            morse_flipper_scene_open(app, MorseFlipperSceneStraight);
            return true;
        }

        if(event->key == InputKeyLeft &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyUp && event->type == InputTypeLong &&
           !morse_trainer_session_active(&app->trainer) && !app->trainer_playback_active) {
            morse_trainer_load_session_lines(&app->session_lines);
            app->session_line_idx = app->session_lines.count == 0U ? 0U : (app->session_lines.count - 1U);
            morse_flipper_scene_open(app, MorseFlipperSceneBrowse);
            return true;
        }

        return false;
    }

    if(app->screen == MorseFlipperScreenBrowse) {
        if(event->key == InputKeyUp &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           app->session_lines.count != 0U) {
            app->session_line_idx = app->session_line_idx == 0U ?
                                        (app->session_lines.count - 1U) :
                                        (app->session_line_idx - 1U);
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyDown &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat) &&
           app->session_lines.count != 0U) {
            app->session_line_idx = (app->session_line_idx + 1U) % app->session_lines.count;
            morse_flipper_view_dirty(app);
            return true;
        }

        if((event->key == InputKeyRight || event->key == InputKeyBack) &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }

        return false;
    }

    if(app->screen == MorseFlipperScreenRf) {
        if(app->rf_manual_active) {
            if(event->key == InputKeyLeft &&
               (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
                app->rf_edit_digit = app->rf_edit_digit == 0U ? 5U : (app->rf_edit_digit - 1U);
                morse_flipper_view_dirty(app);
                return true;
            }

            if(event->key == InputKeyRight &&
               (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
                app->rf_edit_digit = (app->rf_edit_digit + 1U) % 6U;
                morse_flipper_view_dirty(app);
                return true;
            }

            if(event->key == InputKeyUp &&
               (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
                char* ch = &app->rf_edit_khz[app->rf_edit_digit];
                *ch = (*ch >= '9' || *ch < '0') ? '0' : (char)(*ch + 1);
                morse_flipper_view_dirty(app);
                return true;
            }

            if(event->key == InputKeyDown &&
               (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
                char* ch = &app->rf_edit_khz[app->rf_edit_digit];
                *ch = (*ch <= '0' || *ch > '9') ? '9' : (char)(*ch - 1);
                morse_flipper_view_dirty(app);
                return true;
            }

            if(event->key == InputKeyOk &&
               (event->type == InputTypeShort || event->type == InputTypeLong)) {
                morse_flipper_rf_set_manual_khz(&app->rf, app->rf_edit_khz);
                app->rf_manual_active = false;
                morse_flipper_view_dirty(app);
                return true;
            }

            if(event->key == InputKeyBack &&
               (event->type == InputTypeShort || event->type == InputTypeLong)) {
                app->rf_manual_active = false;
                morse_flipper_view_dirty(app);
                return true;
            }

            return false;
        }

        if(app->rf_live_active) {
            morse_flipper_handle_active_keying_event(app, event);
            return true;
        }

        if(event->key == InputKeyLeft &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_rf_step(&app->rf, -1);
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyRight &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_rf_step(&app->rf, 1);
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyUp &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_rf_next_band(&app->rf);
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyDown &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            uint32_t rx_idle_ms =
                (uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_RX_ARM_DITS;

            if(rx_idle_ms < MORSE_FLIPPER_RF_RX_ARM_MIN_MS) {
                rx_idle_ms = MORSE_FLIPPER_RF_RX_ARM_MIN_MS;
            }
            app->rf_live_active = true;
            app->rf_tx_tail_until = 0U;
            app->rf_rx_arm_until = now_ms + rx_idle_ms;
            app->rf_tx_edge_at = 0U;
            app->rf_tx_level = false;
            app->rf_tx_gap_flushed = true;
            app->rf_rx_text[0] = '\0';
            app->rf_tx_text[0] = '\0';
            morse_flipper_rf_reset_live(&app->rf);
            morse_flipper_cw_decoder_init(&app->rf_decoder, morse_flipper_current_dit_ms(app));
            morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyOk &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            strncpy(app->rf_edit_khz, morse_flipper_rf_manual_khz_text(&app->rf), 6U);
            app->rf_edit_khz[6] = '\0';
            app->rf_edit_digit = 0U;
            app->rf_manual_active = true;
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }

        return false;
    }

    if(app->screen == MorseFlipperScreenRun) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(app->screen == MorseFlipperScreenHome) {
        if(event->type == InputTypePress) {
            if(event->key == InputKeyLeft) {
                morse_flipper_tone_nudge(app, -1);
                return true;
            } else if(event->key == InputKeyRight) {
                morse_flipper_tone_nudge(app, 1);
                return true;
            }
        }

        if(event->key == InputKeyUp && event->type == InputTypeShort) {
            morse_flipper_toggle_source(app);
            return true;
        }

        if(event->key == InputKeyDown && event->type == InputTypeShort) {
            morse_flipper_cycle_mode(app);
            return true;
        }

        if(event->key == InputKeyDown && event->type == InputTypeLong) {
            morse_flipper_toggle_handedness(app);
            return true;
        }

        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }
    }

    return false;
}

static bool morse_flipper_custom_event_callback(void* context, uint32_t event) {
    MorseFlipperApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool morse_flipper_back_event_callback(void* context) {
    MorseFlipperApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void morse_flipper_tick_callback(void* context) {
    MorseFlipperApp* app = context;

    morse_flipper_poll(app);
    morse_flipper_tick_trainer_playback(app, furi_get_tick());

    if(app->preview_ticks > 0U) {
        app->preview_ticks--;
        morse_flipper_update_sidetone(app);
    }

    scene_manager_handle_tick_event(app->scene_manager);
}

static void morse_flipper_scene_menu_main_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuMain);

    morse_flipper_enter_screen(app, MorseFlipperScreenMenu, furi_get_tick());
    submenu_set_header(app->submenu, "Morse Flipper");
    submenu_add_item(app->submenu, "Free Practice", MorseFlipperSceneRun, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Radio TX & RX", MorseFlipperSceneRf, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Training", MorseFlipperSceneMenuTraining, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Settings", MorseFlipperSceneMenuSettings, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneRun && sel != MorseFlipperSceneRf &&
       sel != MorseFlipperSceneMenuTraining && sel != MorseFlipperSceneMenuSettings)
        sel = MorseFlipperSceneRun;
    submenu_set_selected_item(app->submenu, sel);
    view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewMenu);
}

static bool morse_flipper_scene_menu_main_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuMain, event.event);
        scene_manager_next_scene(app->scene_manager, event.event);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_main_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_menu_training_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuTraining);

    morse_flipper_enter_screen(app, MorseFlipperScreenMenu, furi_get_tick());
    submenu_set_header(app->submenu, "Training");
    submenu_add_item(app->submenu, "Koch - LCWO groups", MorseFlipperSceneSession, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Straight Key trainer", MorseFlipperSceneStraight, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Koch statistics", MorseFlipperSceneBrowse, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneSession && sel != MorseFlipperSceneStraight && sel != MorseFlipperSceneBrowse)
        sel = MorseFlipperSceneSession;
    submenu_set_selected_item(app->submenu, sel);
    view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewMenu);
}

static bool morse_flipper_scene_menu_training_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuTraining, event.event);
        scene_manager_next_scene(app->scene_manager, event.event);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_training_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_menu_settings_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuSettings);

    morse_flipper_enter_screen(app, MorseFlipperScreenMenu, furi_get_tick());
    submenu_set_header(app->submenu, "Settings");
    submenu_add_item(app->submenu, "Main settings", MorseFlipperSceneHome, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Koch - LCWO", MorseFlipperSceneTrainer, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "USB connection", MorseFlipperScenePc, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "USB Settings", MorseFlipperScenePcKeys, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Trace menu", MorseFlipperSceneTrace, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneHome && sel != MorseFlipperSceneTrainer && sel != MorseFlipperScenePc &&
       sel != MorseFlipperScenePcKeys && sel != MorseFlipperSceneTrace)
        sel = MorseFlipperSceneHome;
    submenu_set_selected_item(app->submenu, sel);
    view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewMenu);
}

static bool morse_flipper_scene_menu_settings_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuSettings, event.event);
        scene_manager_next_scene(app->scene_manager, event.event);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_settings_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static bool morse_flipper_scene_live_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static void morse_flipper_scene_live_on_exit(void* context) {
    UNUSED(context);
}

static void morse_flipper_scene_run_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenRun);
}

static void morse_flipper_scene_rf_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenRf);
}

static void morse_flipper_scene_session_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenSession);
}

static void morse_flipper_scene_straight_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenStraight);
}

static void morse_flipper_scene_browse_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenBrowse);
}

static void morse_flipper_scene_home_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenHome);
}

static void morse_flipper_scene_trainer_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenTrainer);
}

static void morse_flipper_scene_pc_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenPc);
}

static void morse_flipper_scene_pc_keys_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenPcKeys);
}

static void morse_flipper_scene_trace_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_show_live(app, MorseFlipperScreenTrace);
}

static const AppSceneOnEnterCallback morse_flipper_scene_on_enter_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_enter,
    morse_flipper_scene_menu_training_on_enter,
    morse_flipper_scene_menu_settings_on_enter,
    morse_flipper_scene_run_on_enter,
    morse_flipper_scene_rf_on_enter,
    morse_flipper_scene_session_on_enter,
    morse_flipper_scene_straight_on_enter,
    morse_flipper_scene_browse_on_enter,
    morse_flipper_scene_home_on_enter,
    morse_flipper_scene_trainer_on_enter,
    morse_flipper_scene_pc_on_enter,
    morse_flipper_scene_pc_keys_on_enter,
    morse_flipper_scene_trace_on_enter,
};

static const AppSceneOnEventCallback morse_flipper_scene_on_event_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_event,
    morse_flipper_scene_menu_training_on_event,
    morse_flipper_scene_menu_settings_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
};

static const AppSceneOnExitCallback morse_flipper_scene_on_exit_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_exit,
    morse_flipper_scene_menu_training_on_exit,
    morse_flipper_scene_menu_settings_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
};

static const SceneManagerHandlers morse_flipper_scene_handlers = {
    .on_enter_handlers = morse_flipper_scene_on_enter_handlers,
    .on_event_handlers = morse_flipper_scene_on_event_handlers,
    .on_exit_handlers = morse_flipper_scene_on_exit_handlers,
    .scene_num = MorseFlipperSceneNum,
};

int32_t morse_flipper_fap(void* p) {
    UNUSED(p);

    static MorseFlipperApp app;
    app = (MorseFlipperApp){
        .q = NULL,
        .view_port = NULL,
        .view_dispatcher = NULL,
        .scene_manager = NULL,
        .submenu = NULL,
        .live_view = NULL,
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
        .session_log_pending = false,
        .trainer_playback_active = false,
        .trainer_playback_mark = false,
        .session_started = false,
        .session_round_pending = false,
        .session_result_hold = false,
        .session_result_tone = false,
        .session_result_good = false,
        .midi_rx_pending = false,
        .screen = MorseFlipperScreenMenu,
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
        .straight_next_at = 0U,
        .straight_last_input_at = 0U,
        .straight_mark_started_at = 0U,
        .session_last_input_at = 0U,
        .session_result_until = 0U,
        .session_next_group_at = 0U,
        .rf_tx_tail_until = 0U,
        .rf_rx_arm_until = 0U,
        .rf_tx_edge_at = 0U,
        .gpio_edge_at = 0U,
        .paddle_sources = {0U, 0U},
        .note_sources = {0U, 0U, 0U},
        .trainer = {0},
        .custom_sets = {0},
        .session_lines = {0},
        .straight_stats = {0},
        .session_line_idx = 0U,
        .straight_playback_active = false,
        .straight_playback_mark = false,
        .straight_wait_answer = false,
        .straight_done = false,
        .straight_key_down = false,
        .rf_manual_active = false,
        .rf_live_active = false,
        .rf_tx_level = false,
        .rf_tx_gap_flushed = true,
        .gpio_level = false,
        .gpio_gap_flushed = true,
        .straight_mark_idx = 0U,
        .rf_edit_digit = 0U,
        .rf_edit_khz = {0},
        .rf_rx_text = {0},
        .rf_tx_text = {0},
        .gpio_text = {0},
        .rf = {0},
        .radio = {0},
        .rf_decoder = {0},
        .tx_decoder = {0},
        .gpio_decoder = {0},
        .straight_trainer = {0},
    };

    morse_trainer_init(&app.trainer);
    morse_flipper_rf_init(&app.rf);
    morse_flipper_rf_load_settings(&app.rf, MORSE_FLIPPER_SUBGHZ_SETTINGS_PATH);
    morse_flipper_radio_init(&app.radio);
    morse_flipper_radio_set_rx_callback(&app.radio, morse_flipper_rf_rx_edge, &app);
    morse_flipper_cw_decoder_init(&app.rf_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.tx_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.gpio_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_straight_trainer_init(&app.straight_trainer);
    morse_trainer_load_custom_sets(&app.custom_sets);
    morse_trainer_load_session_lines(&app.session_lines);
    morse_trainer_load_straight_stats(&app.straight_stats);
    morse_flipper_apply_trainer_charset_choice(&app);
    morse_flipper_load_config(&app);
    morse_flipper_cw_decoder_init(&app.rf_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.tx_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.gpio_decoder, morse_flipper_current_dit_ms(&app));
    morse_keyer_init(&app.keyer, app.keyer_mode, morse_flipper_current_dit_ms(&app));
    morse_flipper_gpio_init();
    app.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app.view_dispatcher, &app);
    view_dispatcher_set_custom_event_callback(app.view_dispatcher, morse_flipper_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app.view_dispatcher, morse_flipper_back_event_callback);
    view_dispatcher_set_tick_event_callback(app.view_dispatcher, morse_flipper_tick_callback, MORSE_FLIPPER_POLL_MS);
    view_dispatcher_attach_to_gui(app.view_dispatcher, app.gui, ViewDispatcherTypeFullscreen);

    app.scene_manager = scene_manager_alloc(&morse_flipper_scene_handlers, &app);

    app.submenu = submenu_alloc();
    view_dispatcher_add_view(app.view_dispatcher, MorseFlipperViewMenu, submenu_get_view(app.submenu));

    app.live_view = view_alloc();
    view_set_context(app.live_view, &app);
    view_allocate_model(app.live_view, ViewModelTypeLockFree, sizeof(MorseFlipperLiveModel));
    with_view_model(app.live_view, MorseFlipperLiveModel * m, {
        m->app = &app;
        m->bump = 0U;
    }, false);
    view_set_draw_callback(app.live_view, morse_flipper_live_draw);
    view_set_input_callback(app.live_view, morse_flipper_live_input);
    view_dispatcher_add_view(app.view_dispatcher, MorseFlipperViewLive, app.live_view);

    scene_manager_next_scene(app.scene_manager, MorseFlipperSceneMenuMain);
    view_dispatcher_run(app.view_dispatcher);

    morse_flipper_clear_button_keying(&app, furi_get_tick());
    morse_flipper_set_pc_mode(&app, MorseFlipperPcModeOff);
    morse_flipper_radio_sync_live(&app.radio, morse_flipper_rf_frequency_hz(&app.rf), false, false);
    morse_flipper_radio_set_tx_level(&app.radio, false);
    morse_flipper_radio_deinit(&app.radio);
    morse_keyer_reset(&app.keyer);
    morse_flipper_drain_keyer_events(&app);
    morse_flipper_release_all_notes(&app);
    morse_flipper_tone_stop(&app);
    morse_flipper_save_config(&app);

    morse_flipper_gpio_deinit();
    if(app.view_dispatcher) {
        view_dispatcher_remove_view(app.view_dispatcher, MorseFlipperViewLive);
        view_dispatcher_remove_view(app.view_dispatcher, MorseFlipperViewMenu);
    }
    if(app.live_view) view_free(app.live_view);
    if(app.submenu) submenu_free(app.submenu);
    if(app.scene_manager) scene_manager_free(app.scene_manager);
    if(app.view_dispatcher) view_dispatcher_free(app.view_dispatcher);
    furi_record_close(RECORD_GUI);

    return 0;
}
