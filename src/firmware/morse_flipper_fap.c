#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <m-array.h>
#include <string.h>

#include "keyer.h"
#include "morse_flipper_audio_pwm.h"
#include "morse_flipper_cw_decoder.h"
#include "morse_flipper_gpio.h"
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
#define MORSE_FLIPPER_CONFIG_VERSION 6
#define MORSE_FLIPPER_TONE_OFF_IDX 0xFFU
#define MORSE_FLIPPER_DEFAULT_DIT_MS 100U
#define MORSE_FLIPPER_SESSION_SETTLE_MS 1000U
#define MORSE_FLIPPER_SESSION_RESULT_MS 160U
#define MORSE_FLIPPER_STRAIGHT_SETTLE_MS 700U
#define MORSE_FLIPPER_RF_TX_TAIL_DITS 2U
#define MORSE_FLIPPER_RF_LIVE_DECODERS 0U
#define _SHOW_ANSWER_WHILE_TRAINING_LCWO 1U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S 6U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S     3U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S     10U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S 3U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S 15U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S 3U

#define MORSE_SOURCE_STRAIGHT_GPIO (1UL << 0)
#define MORSE_SOURCE_STRAIGHT_BTN  (1UL << 1)
#define MORSE_SOURCE_KEYER_DIT     (1UL << 2)
#define MORSE_SOURCE_KEYER_DAH     (1UL << 3)

#define MORSE_PADDLE_SOURCE_GPIO_DIT (1UL << 0)
#define MORSE_PADDLE_SOURCE_GPIO_DAH (1UL << 1)
#define MORSE_PADDLE_SOURCE_BTN_OK   (1UL << 2)
#define MORSE_PADDLE_SOURCE_BTN_BACK (1UL << 3)

static const GpioPin* const morse_flipper_gpio_pins[MORSE_FLIPPER_GPIO_PIN_COUNT] = {
    &gpio_ext_pa7,
    &gpio_ext_pa6,
    &gpio_ext_pa4,
    &gpio_ext_pb3,
    &gpio_ext_pb2,
    &gpio_ext_pc3,
    &gpio_ext_pc1,
    &gpio_ext_pc0,
};

static const GpioPin* morse_flipper_straight_pin = &gpio_ext_pa6;
static const GpioPin* morse_flipper_dit_pin = &gpio_ext_pc3;
static const GpioPin* morse_flipper_dah_pin = &gpio_ext_pb3;
static const GpioPin* morse_flipper_ground_pin = &gpio_ext_pa7;

typedef struct MorseFlipperApp {
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
    MorseFlipperScreenHelp = 11,
    MorseFlipperScreenAbout = 12,
} MorseFlipperScreen;

typedef enum {
    MorseFlipperViewMenu = 0,
    MorseFlipperViewLive,
    MorseFlipperViewSettings,
    MorseFlipperViewWidget,
} MorseFlipperView;

typedef enum {
    MorseFlipperSceneMenuMain = 0,
    MorseFlipperSceneMenuTraining,
    MorseFlipperSceneMenuSettings,
    MorseFlipperSceneMenuHelp,
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
    MorseFlipperSceneGpio,
    MorseFlipperSceneHelp,
    MorseFlipperSceneAbout,
    MorseFlipperSceneNum,
} MorseFlipperScene;

typedef enum {
    MorseFlipperHelpFirstSteps = 0,
    MorseFlipperHelpInputKeys,
    MorseFlipperHelpLcwo,
    MorseFlipperHelpPrepping,
    MorseFlipperHelpContact,
    MorseFlipperHelpContesting,
    MorseFlipperHelpUsbLive,
    MorseFlipperHelpMovingForward,
    MorseFlipperHelpCount,
} MorseFlipperHelpTopic;

typedef enum {
    MorseFlipperSettingWpm = 0,
    MorseFlipperSettingInput,
    MorseFlipperSettingKeyer,
    MorseFlipperSettingSwap,
    MorseFlipperSettingTone,
    MorseFlipperSettingGpio,
} MorseFlipperSettingIndex;

typedef enum {
    MorseFlipperGpioSettingStraight = 0,
    MorseFlipperGpioSettingDit,
    MorseFlipperGpioSettingDah,
    MorseFlipperGpioSettingGround,
} MorseFlipperGpioSettingIndex;

typedef enum {
    MorseFlipperTrainerSettingLesson = 0,
    MorseFlipperTrainerSettingWpm,
    MorseFlipperTrainerSettingFarnsworth,
    MorseFlipperTrainerSettingAnswerTimeout,
    MorseFlipperTrainerSettingGroupPause,
    MorseFlipperTrainerSettingGroupSize,
    MorseFlipperTrainerSettingGroups,
    MorseFlipperTrainerSettingChars,
} MorseFlipperTrainerSettingIndex;

typedef enum {
    MorseFlipperPcModeOff = 0,
    MorseFlipperPcModeKeyboard = 1,
    MorseFlipperPcModeMouse = 2,
    MorseFlipperPcModeMidi = 3,
} MorseFlipperPcMode;

typedef enum {
    MorseFlipperBacklightAuto = 0,
    MorseFlipperBacklightHold,
} MorseFlipperBacklightMode;

typedef enum {
    MorseFlipperUsbSettingConnection = 0,
    MorseFlipperUsbSettingPaddle,
    MorseFlipperUsbSettingStraight,
    MorseFlipperUsbSettingMouseSwap,
} MorseFlipperUsbSettingIndex;

static const char* const morse_flipper_usb_mode_names[] = {
    "None",
    "Keyboard",
    "Mouse",
    "MIDI",
};

static const NotificationSequence morse_flipper_led_good_twice = {
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    &message_delay_50,
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    NULL,
};

static const NotificationSequence morse_flipper_led_bad_twice = {
    &message_red_255,
    &message_delay_50,
    &message_red_0,
    &message_delay_50,
    &message_red_255,
    &message_delay_50,
    &message_red_0,
    NULL,
};

static const NotificationMessage morse_flipper_msg_green_96 = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 96U,
};

static const NotificationSequence morse_flipper_led_miss_twice = {
    &message_red_255,
    &morse_flipper_msg_green_96,
    &message_blue_0,
    &message_delay_50,
    &message_red_0,
    &message_green_0,
    &message_delay_50,
    &message_red_255,
    &morse_flipper_msg_green_96,
    &message_blue_0,
    &message_delay_50,
    &message_red_0,
    &message_green_0,
    NULL,
};

static const uint8_t morse_flipper_input_values[] = {
    MorseFlipperInputSourceButtons,
    MorseFlipperInputSourceStraight,
    MorseFlipperInputSourcePaddle,
};

static const char* const morse_flipper_input_names[] = {
    "buttons",
    "straight",
    "paddle",
};

static const uint8_t morse_flipper_keyer_values[] = {
    MorseKeyerModeStraight,
    MorseKeyerModeBug,
    MorseKeyerModePlainIambic,
    MorseKeyerModeIambicA,
    MorseKeyerModeIambicB,
    MorseKeyerModeUltimatic,
    MorseKeyerModeKeyahead,
};

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
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_custom_set_idx;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    uint8_t usb_mouse_invert;
    uint8_t trainer_farn_wpm;
    uint8_t trainer_to_s;
    uint8_t trainer_gap_s;
} MorseFlipperConfig;

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
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_custom_set_idx;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    uint8_t usb_mouse_invert;
} MorseFlipperConfigV5;

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
} MorseFlipperConfigV2;

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
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
} MorseFlipperConfigV3;

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
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_custom_set_idx;
} MorseFlipperConfigV4;

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
    VariableItemList* settings_list;
    Widget* widget;
    VariableItem* trainer_items[MorseFlipperTrainerSettingChars + 1U];
    View* live_view;
    Gui* gui;
    DialogsApp* dialogs;
    NotificationApp* notifications;
    FuriString* help_text;
    volatile bool exit_requested;
    FuriHalUsbInterface* previous_usb_config;
    FuriHalUsbHidConfig hid_cfg;
    bool tone_on;
    bool sp_owned;
    bool sp_busy;
    float sp_hz;
    bool transport_connected;
    bool mouse_invert;
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
    uint8_t pc_pref;
    uint8_t pc_paddle_preset;
    uint8_t pc_straight_preset;
    uint8_t pc_keys_row;
    uint8_t handedness;
    uint8_t in_src;
    uint8_t keyer_mode;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_row;
    uint8_t help_topic;
    uint8_t help_page;
    uint8_t trainer_farn_wpm;
    uint8_t trainer_to_s;
    uint8_t trainer_gap_s;
    uint8_t trainer_char_idx;
    uint8_t trainer_mark_idx;
    uint8_t session_wait_draw_s;
    uint8_t gpio_edit_straight_idx;
    uint8_t gpio_edit_dit_idx;
    uint8_t gpio_edit_dah_idx;
    uint8_t gpio_edit_ground_idx;
    bool vail_mode_active;
    bool vail_speed_active;
    bool vail_tone_active;
    uint8_t vail_keyer_mode;
    uint16_t vail_dit_ms;
    uint8_t vail_tone_idx;
    MorseKeyer keyer;
    uint8_t tone_idx;
    uint8_t prev_n;
    uint8_t input_mask;
    uint32_t trainer_next_at;
    uint32_t straight_next_at;
    uint32_t straight_last_input_at;
    uint32_t straight_mark_started_at;
    uint32_t session_last_input_at;
    uint32_t session_result_until;
    uint32_t session_next_group_at;
    uint32_t rf_tail_at;
    uint32_t rf_tx_edge_at;
    uint32_t gpio_edge_at;
    uint32_t paddle_sources[MorseKeyerPaddleCount];
    uint32_t note_src[3];
    MorseTrainer trainer;
    MorseTrainerCustomSets custom_sets;
    MorseTrainerSessionLines session_lines;
    MorseTrainerStraightStats straight_stats;
    uint8_t session_line_idx;
    bool straight_playback_active;
    bool sk_play_mark;
    bool sk_wait;
    bool sk_done;
    bool sk_down;
    bool rf_man;
    bool rf_live;
    bool rf_tx_level;
    bool rf_tx_gap_flushed;
    bool gpio_level;
    bool gpio_gap_flushed;
    uint8_t sk_mark_i;
    uint8_t sk_ret_scr;
    uint8_t rf_edit_digit;
    uint8_t backlight;
    char rf_edit_khz[8];
    char rf_rx_text[64];
    char rf_tx_text[64];
    char gpio_text[64];
    MorseFlipperAudioPwm audio_pwm;
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

typedef struct {
    const char* label;
    uint8_t current_value_index;
    FuriString* current_value_text;
    uint8_t values_count;
    VariableItemChangeCallback change_callback;
    void* context;
} MorseFlipperVilItem;

ARRAY_DEF(MorseFlipperVilArray, MorseFlipperVilItem, M_POD_OPLIST);

typedef struct {
    MorseFlipperVilArray_t items;
    uint8_t position;
    uint8_t window_position;
} MorseFlipperVilModel;

static void morse_flipper_set_paddle_source( MorseFlipperApp* app, uint8_t paddle, uint32_t source_mask, bool active, uint32_t now_ms);
static void morse_flipper_set_note_source( MorseFlipperApp* app, uint8_t note, uint32_t source_mask, bool active);
static void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_btn_clear(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_poll(MorseFlipperApp* app);
static void morse_flipper_release_all_notes(MorseFlipperApp* app);
static void morse_flipper_save_config(const MorseFlipperApp* app);
static void morse_flipper_set_pc_mode(MorseFlipperApp* app, uint8_t mode);
static void morse_flipper_handle_midi_rx(MorseFlipperApp* app);
static void morse_flipper_update_sidetone(MorseFlipperApp* app);
static void morse_flipper_cycle_pc_mode(MorseFlipperApp* app, int dir);
static void morse_flipper_midi_rx_ready(void* context);
static void morse_flipper_cycle_pc_key_preset(MorseFlipperApp* app, int dir);
static void morse_flipper_release_mouse_buttons(void);
static void morse_flipper_toggle_source(MorseFlipperApp* app);
static bool morse_flipper_training_playback_active(const MorseFlipperApp* app);
static bool morse_flipper_straight_answer_down(const MorseFlipperApp* app);
static void morse_flipper_cycle_mode(MorseFlipperApp* app);
static void morse_flipper_toggle_handedness(MorseFlipperApp* app);
static void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_help_open(MorseFlipperApp* app);
static void morse_flipper_about_open(MorseFlipperApp* app);
static void morse_flipper_cycle_train(MorseFlipperApp* app, int dir);
static void morse_flipper_pick_charset(MorseFlipperApp* app);
static void morse_flipper_drop_live_play(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_group_play(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_start_session(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_reset_session_runtime(MorseFlipperApp* app);
static void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms);
static bool morse_flipper_session_repeat_active(const MorseFlipperApp* app);
static bool morse_flipper_session_idle_view(const MorseFlipperApp* app);
static void morse_flipper_tick_session(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_leave_session(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms);
static bool morse_flipper_session_wait_key_down(const MorseFlipperApp* app);
static bool morse_flipper_straight_down(void);
static bool morse_flipper_logical_dit_down(const MorseFlipperApp* app);
static bool morse_flipper_logical_dah_down(const MorseFlipperApp* app);
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
static uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app);
static void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms);
static uint32_t morse_flipper_settings_list_state(VariableItemList* list);
static void morse_flipper_settings_list_restore(VariableItemList* list, uint32_t state);
static uint8_t morse_flipper_input_value_index(uint8_t source);
static uint8_t morse_flipper_keyer_value_index(uint8_t mode);
static uint16_t morse_flipper_wpm_to_dit_ms(uint8_t wpm);
static uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app);
static uint32_t morse_flipper_note_source_for_paddle(uint8_t paddle);
static uint8_t morse_flipper_note_for_paddle(uint8_t paddle);
static const GpioPin* morse_flipper_gpio_pin_ptr(uint8_t pin_idx);
static void morse_flipper_gpio_bind_from_app(const MorseFlipperApp* app);
static void morse_flipper_gpio_reset_candidates(void);
static void morse_flipper_gpio_apply(MorseFlipperApp* app);
static void morse_flipper_gpio_alert(MorseFlipperApp* app, MorseFlipperGpioRule rule);
static bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t straight,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    MorseFlipperGpioRule* out_rule);
static const char* morse_flipper_status_line( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static const char* morse_flipper_free_practice_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static const char* morse_flipper_trace_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static const char* morse_flipper_pc_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz);
static const char* morse_flipper_straight_wait_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static void morse_flipper_settings_noop_enter(void* context, uint32_t index);
static void morse_flipper_settings_enter_callback(void* context, uint32_t index);
static void morse_flipper_settings_wpm_changed(VariableItem* item);
static void morse_flipper_settings_input_changed(VariableItem* item);
static void morse_flipper_settings_keyer_changed(VariableItem* item);
static void morse_flipper_settings_swap_changed(VariableItem* item);
static void morse_flipper_settings_tone_changed(VariableItem* item);
static void morse_flipper_settings_gpio_straight_changed(VariableItem* item);
static void morse_flipper_settings_gpio_dit_changed(VariableItem* item);
static void morse_flipper_settings_gpio_dah_changed(VariableItem* item);
static void morse_flipper_settings_gpio_ground_changed(VariableItem* item);
static void morse_flipper_settings_usb_mode_changed(VariableItem* item);
static void morse_flipper_settings_usb_paddle_changed(VariableItem* item);
static void morse_flipper_settings_usb_straight_changed(VariableItem* item);
static void morse_flipper_settings_usb_mouse_swap_changed(VariableItem* item);
static void morse_flipper_scene_menu_pick(void* ctx, uint32_t idx);
static void morse_flipper_trainer_lesson_changed(VariableItem* item);
static void morse_flipper_trainer_wpm_changed(VariableItem* item);
static void morse_flipper_trainer_farnsworth_changed(VariableItem* item);
static void morse_flipper_trainer_answer_timeout_changed(VariableItem* item);
static void morse_flipper_trainer_group_pause_changed(VariableItem* item);
static void morse_flipper_trainer_group_size_changed(VariableItem* item);
static void morse_flipper_trainer_groups_changed(VariableItem* item);
static void morse_flipper_trainer_chars_changed(VariableItem* item);
static void morse_flipper_trainer_menu_refresh(MorseFlipperApp* app);

MorseFlipperApp* morse_flipper_boot(void);
ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app);
void morse_flipper_shutdown(MorseFlipperApp* app);

static uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app) {
    if(app == NULL) return MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace ||
       app->screen == MorseFlipperScreenPc || app->screen == MorseFlipperScreenStraight)
        return MorseFlipperBacklightHold;

    if(app->screen == MorseFlipperScreenRf)
        return app->rf_live ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenSession)
        return app->session_started ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    return MorseFlipperBacklightAuto;
}

static void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t want;

    if(app == NULL || app->notifications == NULL) return;
    UNUSED(now_ms);

    want = morse_flipper_backlight_mode(app);
    if(want != app->backlight) {
        if(app->backlight != MorseFlipperBacklightAuto)
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

        app->backlight = want;

        if(want != MorseFlipperBacklightAuto)
            notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }
}

static uint8_t morse_flipper_input_value_index(uint8_t source) {
    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_input_values); i++) {
        if(morse_flipper_input_values[i] == source) {
            return i;
        }
    }

    return 0U;
}

static uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app) {
    uint16_t dit;
    uint8_t wpm;

    if(app == NULL) return 0U;
    dit = app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    wpm = (uint8_t)((1200U + (dit / 2U)) / dit);
    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    return wpm;
}

static void morse_flipper_train_fix(MorseFlipperApp* app) {
    uint8_t w;

    if(app == NULL) return;

    w = morse_flipper_local_wpm(app);
    if(app->trainer_farn_wpm == 0U) app->trainer_farn_wpm = w;
    if(app->trainer_farn_wpm > w) app->trainer_farn_wpm = w;

    if(app->trainer_to_s == 0U) app->trainer_to_s = MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S;
    if(app->trainer_to_s < MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S) app->trainer_to_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S;
    if(app->trainer_to_s > MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S) app->trainer_to_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S;

    if(app->trainer_gap_s == 0U) app->trainer_gap_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S;
    if(app->trainer_gap_s < MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S) app->trainer_gap_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S;
    if(app->trainer_gap_s > MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S) app->trainer_gap_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S;
}

static void morse_flipper_set_local_wpm(MorseFlipperApp* app, uint8_t wpm) {
    if(app == NULL) return;

    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;

    app->trainer.local_dit_ms = morse_flipper_wpm_to_dit_ms(wpm);
    morse_flipper_train_fix(app);
    morse_flipper_cw_decoder_init(&app->rf_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->gpio_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_refresh_keyer(app, furi_get_tick());
}

static uint16_t morse_flipper_trainer_farnsworth_unit_ms(const MorseFlipperApp* app) {
    uint32_t w;
    uint32_t farn;
    uint32_t dit;
    uint32_t total;
    uint32_t spare;

    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS;

    w = morse_flipper_local_wpm(app);
    farn = app->trainer_farn_wpm;
    dit = app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    if(farn == 0U || farn >= w) return (uint16_t)dit;

    total = 60000U / farn;
    if(total <= 31U * dit) return (uint16_t)dit;

    spare = total - (31U * dit);
    return (uint16_t)((spare + 9U) / 19U);
}

static uint16_t morse_flipper_trainer_char_gap_ms(const MorseFlipperApp* app) {
    uint16_t dit_ms;

    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS * 3U;

    dit_ms = app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    if(app->screen != MorseFlipperScreenSession) return (uint16_t)(dit_ms * 3U);
    return (uint16_t)(morse_flipper_trainer_farnsworth_unit_ms(app) * 3U);
}

static uint8_t morse_flipper_keyer_value_index(uint8_t mode) {
    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_keyer_values); i++) {
        if(morse_flipper_keyer_values[i] == mode) {
            return i;
        }
    }

    return 0U;
}

static uint16_t morse_flipper_wpm_to_dit_ms(uint8_t wpm) {
    if(wpm < 1U) {
        return MORSE_FLIPPER_DEFAULT_DIT_MS;
    }

    return (1200U + (wpm / 2U)) / wpm;
}

static const GpioPin* morse_flipper_gpio_pin_ptr(uint8_t pin_idx) {
    if(!morse_flipper_gpio_pin_valid(pin_idx)) {
        return morse_flipper_gpio_pins[morse_flipper_gpio_default_straight()];
    }

    return morse_flipper_gpio_pins[pin_idx];
}

static uint8_t morse_flipper_gpio_no_p2(uint8_t pin_idx, uint8_t def) {
    return pin_idx == MorseFlipperGpioPinP2 ? def : pin_idx;
}

static void morse_flipper_gpio_bind_from_app(const MorseFlipperApp* app) {
    morse_flipper_straight_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_p2(app->gpio_straight_idx, morse_flipper_gpio_default_straight()));
    morse_flipper_dit_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_p2(app->gpio_dit_idx, morse_flipper_gpio_default_dit()));
    morse_flipper_dah_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_p2(app->gpio_dah_idx, morse_flipper_gpio_default_dah()));
    morse_flipper_ground_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_p2(app->gpio_ground_idx, morse_flipper_gpio_default_ground()));
}

static void morse_flipper_gpio_reset_candidates(void) {
    for(size_t i = 0; i < COUNT_OF(morse_flipper_gpio_pins); i++) {
        furi_hal_gpio_init(morse_flipper_gpio_pins[i], GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
}

static void morse_flipper_gpio_apply(MorseFlipperApp* app) {
    morse_flipper_gpio_bind_from_app(app);
    morse_flipper_gpio_reset_candidates();

    furi_hal_gpio_init(morse_flipper_straight_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_ground_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(morse_flipper_ground_pin, false);
}

static void morse_flipper_gpio_alert(MorseFlipperApp* app, MorseFlipperGpioRule rule) {
    DialogMessage* msg;

    if(app == NULL || app->dialogs == NULL || rule == MorseFlipperGpioRuleOk) {
        return;
    }

    msg = dialog_message_alloc();
    dialog_message_set_header(msg, "GPIO conflict", 64, 10, AlignCenter, AlignTop);
    dialog_message_set_text( msg, morse_flipper_gpio_rule_text(rule), 64, 30, AlignCenter, AlignTop);
    dialog_message_set_buttons(msg, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, msg);
    dialog_message_free(msg);
}

static bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t straight,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    MorseFlipperGpioRule* out_rule) {
    MorseFlipperGpioRule rule = morse_flipper_gpio_validate(straight, dit, dah, ground);

    if(out_rule != NULL) {
        *out_rule = rule;
    }

    if(rule != MorseFlipperGpioRuleOk) {
        return false;
    }

    app->gpio_straight_idx = straight;
    app->gpio_dit_idx = dit;
    app->gpio_dah_idx = dah;
    app->gpio_ground_idx = ground;
    morse_flipper_gpio_apply(app);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
    morse_flipper_save_config(app);
    return true;
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

    if(app->tone_idx >= COUNT_OF(morse_flipper_tones)) {
        return &morse_flipper_tones[0];
    }

    return &morse_flipper_tones[app->tone_idx];
}

static const char* morse_flipper_tone_name(const MorseFlipperApp* app) {
    if(app != NULL && app->tone_idx == MORSE_FLIPPER_TONE_OFF_IDX) return "Off";
    return morse_flipper_current_tone(app)->name;
}

static float morse_flipper_active_tone_hz(const MorseFlipperApp* app) {
    float hz = morse_flipper_current_tone(app)->hz;

    if(app->session_result_tone) return app->session_result_good ? (hz * 1.35f) : (hz * 0.68f);
    return hz;
}

static bool morse_flipper_buzz_ok(const MorseFlipperApp* app)
{
    if(app == NULL) return false;
    return app->tone_idx != MORSE_FLIPPER_TONE_OFF_IDX;
}

static bool morse_flipper_use_pwm_buzzer(const MorseFlipperApp* app)
{
    if(app == NULL) return false;
    if(!morse_flipper_buzz_ok(app)) return false;
    return app->screen == MorseFlipperScreenRun && app->audio_pwm.running;
}

static bool morse_flipper_any_active_notes(const MorseFlipperApp* app) {
    for(size_t i = 0; i < COUNT_OF(app->note_src); i++) {
        if(app->note_src[i] != 0U) {
            return true;
        }
    }

    return false;
}

static bool morse_flipper_straight_like_mode(const MorseFlipperApp* app) {
    uint8_t mode = morse_flipper_current_keyer_mode(app);

    return mode == MorseKeyerModePassthrough || mode == MorseKeyerModeStraight;
}

static bool morse_flipper_session_running_view(const MorseFlipperApp* app);

typedef struct {
    bool live;
    bool btn;
    bool btn_str;
    bool btn_pad;
    bool back_key;
    bool back_exit;
    bool left_hint;
} MorseFlipperInputGate;

static MorseFlipperInputGate morse_flipper_input_gate(const MorseFlipperApp* app) {
    MorseFlipperInputGate g = {0};

    if(app == NULL) return g;

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace ||
       app->screen == MorseFlipperScreenPc) {
        g.live = true;
    } else if(app->screen == MorseFlipperScreenSession) {
        g.live = morse_flipper_session_repeat_active(app) || morse_flipper_session_running_view(app);
    } else if(app->screen == MorseFlipperScreenRf) {
        g.live = app->rf_live;
    } else if(app->screen == MorseFlipperScreenStraight) {
        g.live = app->sk_wait;
    }

    if(!g.live || app->in_src != MorseFlipperInputSourceButtons) {
        g.back_exit = g.live;
        return g;
    }

    g.btn = true;
    if(app->screen == MorseFlipperScreenStraight) {
        g.btn_str = true;
        g.back_exit = true;
        return g;
    }

    if(morse_flipper_straight_like_mode(app)) g.btn_str = true;
    else g.btn_pad = true;

    g.back_key = g.btn_pad;
    g.back_exit = !g.back_key;
    g.left_hint = g.back_key;
    return g;
}

static bool morse_flipper_live_back_is_key(const MorseFlipperApp* app) {
    return morse_flipper_input_gate(app).back_key;
}

static bool morse_flipper_live_back_exits(const MorseFlipperApp* app) {
    return morse_flipper_input_gate(app).back_exit;
}

static bool morse_flipper_live_left_hint(const MorseFlipperApp* app) {
    return morse_flipper_input_gate(app).left_hint;
}

static bool morse_flipper_button_paddle_keying_active(const MorseFlipperApp* app) {
    return morse_flipper_input_gate(app).btn_pad;
}

static const char* morse_flipper_status_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->sp_busy) {
        snprintf(buf, buf_sz, "speaker busy");
        return buf;
    }

    if(app->in_src == MorseFlipperInputSourceButtons) {
        if(morse_flipper_straight_like_mode(app)) {
            snprintf(buf, buf_sz, "src btn ok str");
            return buf;
        }
        if(app->handedness == MorseFlipperHandednessSwapped) {
            snprintf(buf, buf_sz, "src btn hand swp");
            return buf;
        }
        snprintf(buf, buf_sz, "src btn hand norm");
        return buf;
    }

    if(app->in_src == MorseFlipperInputSourcePaddle) {
        if(app->handedness == MorseFlipperHandednessSwapped) {
            snprintf(
                buf,
                buf_sz,
                "src %s/%s swp",
                morse_flipper_gpio_name(app->gpio_dit_idx),
                morse_flipper_gpio_name(app->gpio_dah_idx));
            return buf;
        }
        snprintf(
            buf,
            buf_sz,
            "src %s/%s norm",
            morse_flipper_gpio_name(app->gpio_dit_idx),
            morse_flipper_gpio_name(app->gpio_dah_idx));
        return buf;
    }

    snprintf(buf, buf_sz, "src %s straight", morse_flipper_gpio_name(app->gpio_straight_idx));
    return buf;
}

static const char* morse_flipper_pc_mode_name(uint8_t mode) {
    switch(mode) {
    case MorseFlipperPcModeKeyboard:
        return "keyboard";
    case MorseFlipperPcModeMouse:
        return "mouse";
    case MorseFlipperPcModeMidi:
        return "midi";
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

static const char* morse_flipper_free_practice_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->in_src == MorseFlipperInputSourceButtons) {
        snprintf( buf, buf_sz, "%s", morse_flipper_live_back_is_key(app) ? "O/B key hold L" : "OK str Bk back");
        return buf;
    }

    if(app->in_src == MorseFlipperInputSourcePaddle) {
        snprintf(
            buf,
            buf_sz,
            "%s/%s key  Bk back",
            morse_flipper_gpio_name(app->gpio_dit_idx),
            morse_flipper_gpio_name(app->gpio_dah_idx));
        return buf;
    }

    snprintf(buf, buf_sz, "%s key  Bk back", morse_flipper_gpio_name(app->gpio_straight_idx));
    return buf;
}

static const char* morse_flipper_trace_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->in_src == MorseFlipperInputSourceButtons) {
        snprintf( buf, buf_sz, "%s", morse_flipper_live_back_is_key(app) ? "key live hold L" : "OK str Bk back");
        return buf;
    }

    snprintf(buf, buf_sz, "gpio live  Bk back");
    return buf;
}

static const char* morse_flipper_pc_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->in_src == MorseFlipperInputSourceButtons) {
        snprintf( buf, buf_sz, "%s", morse_flipper_live_back_is_key(app) ? "O/B key U/D mode" : "OK str U/D mode");
        return buf;
    }

    snprintf(buf, buf_sz, "gpio key U/D mode");
    return buf;
}

static const char* morse_flipper_straight_wait_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->in_src == MorseFlipperInputSourceButtons) {
        snprintf(buf, buf_sz, "OK key  Bk back");
        return buf;
    }

    if(app->in_src == MorseFlipperInputSourceStraight) {
        snprintf(buf, buf_sz, "%s key  Bk back", morse_flipper_gpio_name(app->gpio_straight_idx));
        return buf;
    }

    snprintf(
        buf,
        buf_sz,
        "%s/%s key Bk back",
        morse_flipper_gpio_name(app->gpio_dit_idx),
        morse_flipper_gpio_name(app->gpio_dah_idx));
    return buf;
}

static const char* morse_flipper_source_short_name( const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->in_src == MorseFlipperInputSourceButtons) {
        snprintf(buf, buf_sz, "btn");
        return buf;
    }

    if(app->in_src == MorseFlipperInputSourcePaddle) {
        snprintf(
            buf,
            buf_sz,
            "%s/%s",
            morse_flipper_gpio_name(app->gpio_dit_idx),
            morse_flipper_gpio_name(app->gpio_dah_idx));
        return buf;
    }

    snprintf(buf, buf_sz, "%s", morse_flipper_gpio_name(app->gpio_straight_idx));
    return buf;
}

static bool morse_flipper_session_left_exit_active(const MorseFlipperApp* app) {
    return morse_flipper_live_left_hint(app);
}

static bool morse_flipper_session_running_view(const MorseFlipperApp* app) {
    if(app == NULL || app->screen != MorseFlipperScreenSession) return false;
    return !morse_flipper_session_idle_view(app);
}

#include "morse_flipper_session.h"
#include "morse_flipper_session.c"

static const char* morse_flipper_pc_state_name(const MorseFlipperApp* app) {
    bool up = false;

    if(app->pc_mode == MorseFlipperPcModeMidi) {
        return morse_usb_midi_is_connected() ? "usb midi up" : "usb midi wait";
    }

    if(app->pc_mode == MorseFlipperPcModeKeyboard) {
        up = furi_hal_hid_is_connected();
        return up ? "usb hid up" : "usb hid wait";
    }

    if(app->pc_mode == MorseFlipperPcModeMouse) {
        up = furi_hal_hid_is_connected();
        return up ? "usb mouse up" : "usb mouse wait";
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

    if(app->pc_mode == MorseFlipperPcModeKeyboard || app->pc_mode == MorseFlipperPcModeMouse) {
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

#include "morse_flipper_runtime.c"
#include "morse_flipper_rf_live.c"
#include "morse_flipper_help.c"

static void morse_flipper_tone_stop(MorseFlipperApp* app) {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    app->tone_on = false;
    app->sp_owned = false;
    app->sp_busy = false;
    app->sp_hz = 0.0f;
}

static void morse_flipper_tone_start(MorseFlipperApp* app) {
    float hz;

    if(app->tone_on) {
        return;
    }

    if(!app->sp_owned) {
        if(furi_hal_speaker_acquire(30U)) {
            app->sp_owned = true;
        } else {
            app->sp_busy = true;
            return;
        }
    }

    if(!furi_hal_speaker_is_mine()) {
        app->tone_on = false;
        app->sp_owned = false;
        app->sp_busy = true;
        return;
    }

    hz = morse_flipper_active_tone_hz(app);
    furi_hal_speaker_start(hz, MORSE_FLIPPER_VOLUME);
    app->tone_on = true;
    app->sp_hz = hz;
    app->sp_busy = false;
}

static void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    bool want_tx_tone = morse_flipper_any_active_notes(app) || (app->prev_n > 0U);
    bool want_aux_tone =
        app->trainer_playback_mark || app->sk_play_mark || app->session_result_tone;
    bool want_tone = want_aux_tone ||
                     (want_tx_tone && !morse_flipper_use_pwm_buzzer(app) &&
                      morse_flipper_buzz_ok(app));

    if(morse_flipper_use_pwm_buzzer(app)) {
        if(app->sp_owned || app->tone_on) {
            morse_flipper_tone_stop(app);
        }

        morse_flipper_audio_pwm_set_gate(&app->audio_pwm, want_tx_tone);
        if(!want_aux_tone) {
            app->tone_on =
                want_tx_tone || morse_flipper_audio_pwm_on(&app->audio_pwm);
            app->sp_busy = false;
            return;
        }
    }

    if(want_tone) {
        float hz = morse_flipper_active_tone_hz(app);

        morse_flipper_tone_start(app);
        if(app->tone_on && app->sp_owned && furi_hal_speaker_is_mine() &&
           app->sp_hz != hz) {
            furi_hal_speaker_start(hz, MORSE_FLIPPER_VOLUME);
            app->sp_hz = hz;
            app->sp_busy = false;
        }
    } else {
        morse_flipper_tone_stop(app);
    }
}

static void morse_flipper_set_note_source( MorseFlipperApp* app, uint8_t note, uint32_t source_mask, bool active) {
    uint32_t now_ms;

    if(note >= COUNT_OF(app->note_src)) {
        return;
    }

    uint32_t before = app->note_src[note];
    uint32_t after = active ? (before | source_mask) : (before & ~source_mask);

    if(before == after) {
        return;
    }

    app->note_src[note] = after;
    morse_flipper_update_sidetone(app);
    if(app->screen == MorseFlipperScreenRf && app->rf_live) {
        now_ms = furi_get_tick();
        app->rf_tail_at =
            now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_TX_TAIL_DITS);

        if(morse_flipper_any_active_notes(app) && !app->radio.tx_on) {
            morse_flipper_radio_sync_live( &app->radio, morse_flipper_rf_frequency_hz(&app->rf), true, true);
        }
        morse_flipper_radio_set_tx_level(&app->radio, morse_flipper_any_active_notes(app));
    }

    if(before == 0U && after != 0U) {
        morse_flipper_send_transport_note(app, note, true);
    } else if(before != 0U && after == 0U) {
        morse_flipper_send_transport_note(app, note, false);
    }
}

static void morse_flipper_release_all_notes(MorseFlipperApp* app) {
    uint32_t note_src[COUNT_OF(app->note_src)];

    for(size_t note = 0; note < COUNT_OF(app->note_src); note++) {
        note_src[note] = app->note_src[note];
        app->note_src[note] = 0U;
    }

    morse_flipper_update_sidetone(app);

    for(size_t note = 0; note < COUNT_OF(app->note_src); note++) {
        if(note_src[note] != 0U) {
            morse_flipper_send_transport_note(app, (uint8_t)note, false);
        }
    }

    if(app->screen == MorseFlipperScreenRf && app->rf_live) {
        uint32_t now_ms = furi_get_tick();
        app->rf_tail_at =
            now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_TX_TAIL_DITS);
        morse_flipper_radio_set_tx_level(&app->radio, false);
    }
}

static void morse_flipper_enter_screen( MorseFlipperApp* app, uint8_t screen, uint32_t now_ms) {
    if(app->screen == screen) return;

    if(app->screen == MorseFlipperScreenSession && screen != MorseFlipperScreenSession) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenStraight && screen != MorseFlipperScreenStraight) {
        morse_flipper_reset_straight_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenRf && screen != MorseFlipperScreenRf) {
        app->rf_live = false;
        app->rf_man = false;
        morse_flipper_radio_sync_live(&app->radio, morse_flipper_rf_frequency_hz(&app->rf), false, false);
        morse_flipper_radio_set_tx_level(&app->radio, false);
    }

    if(app->screen == MorseFlipperScreenRun && screen != MorseFlipperScreenRun) {
        morse_flipper_audio_pwm_stop(&app->audio_pwm);
    }

    morse_flipper_btn_clear(app, now_ms);

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

    if(screen == MorseFlipperScreenRun && app->screen != MorseFlipperScreenRun) {
        morse_flipper_audio_pwm_prepare(
            &app->audio_pwm,
            MORSE_FLIPPER_AUDIO_PWM_CARRIER_HZ,
            MORSE_FLIPPER_AUDIO_PWM_SAMPLE_RATE_HZ,
            MORSE_FLIPPER_AUDIO_PWM_TONE_HZ,
            MORSE_FLIPPER_AUDIO_PWM_FADE_MS,
            MORSE_FLIPPER_AUDIO_PWM_FADE_MS);
        morse_flipper_audio_pwm_start(&app->audio_pwm);
        morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    }

    app->screen = screen;
    if(screen == MorseFlipperScreenHome) {
        morse_flipper_poll(app);
    }
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_toggle_source(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    morse_flipper_drop_live_play(app, now_ms);

    if(app->in_src == MorseFlipperInputSourceStraight) {
        app->in_src = MorseFlipperInputSourcePaddle;
    } else if(app->in_src == MorseFlipperInputSourcePaddle) {
        app->in_src = MorseFlipperInputSourceButtons;
    } else {
        app->in_src = MorseFlipperInputSourceStraight;
    }

    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_cycle_pc_mode(MorseFlipperApp* app, int dir) {
    int next = (int)app->pc_mode + dir;

    if(next < (int)MorseFlipperPcModeOff) {
        next = (int)MorseFlipperPcModeMidi;
    } else if(next > (int)MorseFlipperPcModeMidi) {
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

    if(app->in_src == MorseFlipperInputSourceStraight) {
        return morse_flipper_straight_down();
    }

    if(app->in_src == MorseFlipperInputSourcePaddle) {
        return morse_flipper_logical_dit_down(app) || morse_flipper_logical_dah_down(app);
    }

    return false;
}

static void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->straight_playback_active = false;
    app->sk_play_mark = false;
    app->sk_wait = false;
    app->sk_done = false;
    app->sk_down = false;
    app->sk_mark_i = 0U;
    app->straight_next_at = 0U;
    app->straight_last_input_at = 0U;
    app->straight_mark_started_at = 0U;
    app->straight_trainer.active = false;
    app->straight_trainer.answer[0] = '\0';
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
   morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_update_sidetone(app);
    morse_flipper_btn_clear(app, now_ms);
}

static void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_reset_straight_state(app, now_ms);
    morse_flipper_straight_trainer_start( &app->straight_trainer, morse_trainer_charset(&app->trainer), morse_flipper_current_dit_ms(app));
    app->straight_playback_active = true;
    app->sk_play_mark = false;
    app->sk_mark_i = 0U;
    app->straight_next_at = now_ms;
    morse_flipper_view_dirty(app);
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
            app->trainer_next_at = now_ms + morse_flipper_trainer_char_gap_ms(app);
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

        if(!app->sk_play_mark) {
            if(app->sk_mark_i >= len) {
                app->straight_playback_active = false;
                app->sk_wait = true;
                app->straight_next_at = 0U;
                morse_flipper_view_dirty(app);
                return;
            }

            app->sk_play_mark = true;
            app->straight_next_at =
                now_ms + (morse[app->sk_mark_i] == '-' ? (dit_ms * 3U) : dit_ms);
            morse_flipper_view_dirty(app);
            return;
        }

        app->sk_play_mark = false;
        app->sk_mark_i++;
        if(app->sk_mark_i >= len) {
            app->straight_playback_active = false;
            app->sk_wait = true;
            app->straight_next_at = 0U;
        } else {
            app->straight_next_at = now_ms + dit_ms;
        }
        morse_flipper_view_dirty(app);
    }

    if(app->sk_wait &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
        uint32_t settle = morse_flipper_current_dit_ms(app) * 6U;

        if(settle < MORSE_FLIPPER_STRAIGHT_SETTLE_MS) settle = MORSE_FLIPPER_STRAIGHT_SETTLE_MS;
        if(now_ms - app->straight_last_input_at >= settle) {
            app->sk_wait = false;
            app->sk_done = true;
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

static void morse_flipper_cycle_train(MorseFlipperApp* app, int dir) {
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
        morse_flipper_pick_charset(app);
        break;
    }

    morse_flipper_view_dirty(app);
}

static void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenSession) {
        morse_flipper_leave_session(app, now_ms);
        return;
    }

    morse_flipper_btn_clear(app, now_ms);
    morse_flipper_scene_back(app);
}

static void morse_flipper_pick_charset(MorseFlipperApp* app) {
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

    strncpy( app->trainer.custom_name, app->custom_sets.sets[app->trainer.custom_set_idx - 1U].name, sizeof(app->trainer.custom_name) - 1U);
    app->trainer.custom_name[sizeof(app->trainer.custom_name) - 1U] = '\0';
    strncpy(
        app->trainer.charset_override,
        app->custom_sets.sets[app->trainer.custom_set_idx - 1U].chars,
        sizeof(app->trainer.charset_override) - 1U);
    app->trainer.charset_override[sizeof(app->trainer.charset_override) - 1U] = '\0';
}

static void morse_flipper_cycle_mode(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    morse_flipper_drop_live_play(app, now_ms);
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

static void morse_flipper_btn_pad_clear(MorseFlipperApp* app, uint32_t now_ms) {
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
}

static void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms) {
    morse_flipper_btn_pad_clear(app, now_ms);

    if(app->ok_down) {
        morse_flipper_set_paddle_source( app, morse_flipper_button_ok_paddle(app), MORSE_PADDLE_SOURCE_BTN_OK, true, now_ms);
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

static void morse_flipper_btn_clear(MorseFlipperApp* app, uint32_t now_ms) {
    app->left_down = false;
    app->ok_down = false;
    app->back_down = false;
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_btn_pad_clear(app, now_ms);
}

static uint32_t morse_flipper_note_source_for_paddle(uint8_t paddle) {
    return (paddle == MorseKeyerPaddleDit) ? MORSE_SOURCE_KEYER_DIT : MORSE_SOURCE_KEYER_DAH;
}

static uint8_t morse_flipper_note_for_paddle(uint8_t paddle) {
    return (paddle == MorseKeyerPaddleDit) ? 1U : 2U;
}

#include "morse_flipper_live_view.c"

#include "morse_flipper_flow.c"
#include "morse_flipper_settings.c"
#include "morse_flipper_scenes.c"

MorseFlipperApp* morse_flipper_boot(void) {
    static MorseFlipperApp app;
    app = (MorseFlipperApp){
        .q = NULL,
        .view_port = NULL,
        .view_dispatcher = NULL,
        .scene_manager = NULL,
        .submenu = NULL,
        .settings_list = NULL,
        .widget = NULL,
        .live_view = NULL,
        .gui = furi_record_open(RECORD_GUI),
        .dialogs = furi_record_open(RECORD_DIALOGS),
        .notifications = furi_record_open(RECORD_NOTIFICATION),
        .help_text = NULL,
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
        .sp_owned = false,
        .sp_busy = false,
        .transport_connected = false,
        .mouse_invert = false,
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
        .pc_pref = MorseFlipperPcModeOff,
        .pc_paddle_preset = 0U,
        .pc_straight_preset = 0U,
        .pc_keys_row = 0U,
        .handedness = MorseFlipperHandednessSwapped,
        .in_src = MorseFlipperInputSourceStraight,
        .keyer_mode = MorseKeyerModeStraight,
        .gpio_straight_idx = MorseFlipperGpioPinP3,
        .gpio_dit_idx = MorseFlipperGpioPinP7,
        .gpio_dah_idx = MorseFlipperGpioPinP5,
        .gpio_ground_idx = MorseFlipperGpioPinP3,
        .gpio_edit_straight_idx = MorseFlipperGpioPinP3,
        .gpio_edit_dit_idx = MorseFlipperGpioPinP7,
        .gpio_edit_dah_idx = MorseFlipperGpioPinP5,
        .gpio_edit_ground_idx = MorseFlipperGpioPinP3,
        .trainer_row = 0U,
        .trainer_char_idx = 0U,
        .trainer_mark_idx = 0U,
        .session_wait_draw_s = 0xFFU,
        .vail_mode_active = false,
        .vail_speed_active = false,
        .vail_tone_active = false,
        .vail_keyer_mode = MorseKeyerModeStraight,
        .vail_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS,
        .vail_tone_idx = 0U,
        .keyer = {0},
        .tone_idx = 0U,
        .prev_n = 0U,
        .input_mask = 0U,
        .trainer_next_at = 0U,
        .straight_next_at = 0U,
        .straight_last_input_at = 0U,
        .straight_mark_started_at = 0U,
        .session_last_input_at = 0U,
        .session_result_until = 0U,
        .session_next_group_at = 0U,
        .rf_tail_at = 0U,
        .rf_tx_edge_at = 0U,
        .gpio_edge_at = 0U,
        .paddle_sources = {0U, 0U},
        .note_src = {0U, 0U, 0U},
        .trainer = {0},
        .custom_sets = {0},
        .session_lines = {0},
        .straight_stats = {0},
        .session_line_idx = 0U,
        .straight_playback_active = false,
        .sk_play_mark = false,
        .sk_wait = false,
        .sk_done = false,
        .sk_down = false,
        .rf_man = false,
        .rf_live = false,
        .rf_tx_level = false,
        .rf_tx_gap_flushed = true,
        .gpio_level = false,
        .gpio_gap_flushed = true,
        .sk_mark_i = 0U,
        .rf_edit_digit = 0U,
        .backlight = MorseFlipperBacklightAuto,
        .rf_edit_khz = {0},
        .rf_rx_text = {0},
        .rf_tx_text = {0},
        .gpio_text = {0},
        .audio_pwm = {0},
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
    morse_flipper_audio_pwm_reset(&app.audio_pwm);
    morse_flipper_cw_decoder_init(&app.rf_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.tx_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.gpio_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_straight_trainer_init(&app.straight_trainer);
    morse_trainer_load_custom_sets(&app.custom_sets);
    morse_trainer_load_session_lines(&app.session_lines);
   morse_trainer_load_straight_stats(&app.straight_stats);
    morse_flipper_pick_charset(&app);
    morse_flipper_load_config(&app);
    morse_flipper_pick_charset(&app);
    morse_flipper_cw_decoder_init(&app.rf_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.tx_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.gpio_decoder, morse_flipper_current_dit_ms(&app));
    morse_keyer_init(&app.keyer, app.keyer_mode, morse_flipper_current_dit_ms(&app));
    morse_flipper_gpio_init(&app);
    morse_flipper_set_pc_mode(&app, app.pc_pref);
    app.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app.view_dispatcher, &app);
    view_dispatcher_set_custom_event_callback(app.view_dispatcher, morse_flipper_custom_evt);
    view_dispatcher_set_navigation_event_callback(app.view_dispatcher, morse_flipper_back_evt);
    view_dispatcher_set_tick_event_callback(app.view_dispatcher, morse_flipper_tick_callback, MORSE_FLIPPER_POLL_MS);
    view_dispatcher_attach_to_gui(app.view_dispatcher, app.gui, ViewDispatcherTypeFullscreen);

    app.scene_manager = scene_manager_alloc(&morse_flipper_scene_handlers, &app);

    app.submenu = submenu_alloc();
    view_dispatcher_add_view(app.view_dispatcher, MorseFlipperViewMenu, submenu_get_view(app.submenu));

    app.settings_list = variable_item_list_alloc();
    view_dispatcher_add_view( app.view_dispatcher, MorseFlipperViewSettings, variable_item_list_get_view(app.settings_list));

    app.widget = widget_alloc();
    app.help_text = furi_string_alloc();
    view_dispatcher_add_view(app.view_dispatcher, MorseFlipperViewWidget, widget_get_view(app.widget));

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
    return &app;
}

ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app) {
    if(app == NULL) return NULL;
    return app->view_dispatcher;
}

void morse_flipper_shutdown(MorseFlipperApp* app) {
    if(app == NULL) return;

    morse_flipper_btn_clear(app, furi_get_tick());
    morse_flipper_set_pc_mode(app, MorseFlipperPcModeOff);
    morse_flipper_radio_sync_live(&app->radio, morse_flipper_rf_frequency_hz(&app->rf), false, false);
    morse_flipper_radio_set_tx_level(&app->radio, false);
    morse_flipper_radio_deinit(&app->radio);
    morse_flipper_audio_pwm_stop(&app->audio_pwm);
    morse_keyer_reset(&app->keyer);
    morse_flipper_drain_keyer_events(app);
    morse_flipper_release_all_notes(app);
    morse_flipper_tone_stop(app);
    if(app->backlight != MorseFlipperBacklightAuto && app->notifications)
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    if(app->notifications) {
        notification_message(app->notifications, &sequence_reset_green);
        notification_message(app->notifications, &sequence_reset_red);
    }
    morse_flipper_save_config(app);

    morse_flipper_gpio_deinit();
    if(app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewWidget);
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewSettings);
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewLive);
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewMenu);
    }
    if(app->help_text) furi_string_free(app->help_text);
    if(app->widget) widget_free(app->widget);
    if(app->settings_list) variable_item_list_free(app->settings_list);
    if(app->live_view) view_free(app->live_view);
    if(app->submenu) submenu_free(app->submenu);
    if(app->scene_manager) scene_manager_free(app->scene_manager);
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
}
