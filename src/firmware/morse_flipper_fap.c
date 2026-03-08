#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
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
#include "morse_flipper_gpio_probe.h"
#include "morse_flipper_ham_keyer.h"
#include "morse_flipper_paths.h"
#include "morse_flipper_radio.h"
#include "morse_flipper_run_history.h"
#include "morse_flipper_rf.h"
#include "morse_flipper_straight_filter.h"
#include "morse_flipper_straight_trainer.h"
#include "pc_keys.h"
#include "trainer.h"
#include "trainer_files.h"
#include "usb/morse_usb_midi.h"

#define MORSE_FLIPPER_VOLUME 0.7f
#define MORSE_FLIPPER_POLL_MS 5
#define MORSE_FLIPPER_PREVIEW_TICKS 8
#define MORSE_FLIPPER_CONFIG_PATH APP_DATA_PATH("config.bin")
#define MORSE_FLIPPER_RF_CONFIG_PATH APP_DATA_PATH("rf.bin")
#define MORSE_FLIPPER_CONFIG_VERSION 11
#define MORSE_FLIPPER_DEFAULT_DIT_MS 100U
#define MORSE_FLIPPER_SESSION_SETTLE_MS 1000U
#define MORSE_FLIPPER_SESSION_RESULT_MS 160U
#define MORSE_FLIPPER_STRAIGHT_SETTLE_MS 700U
#define MORSE_FLIPPER_STRAIGHT_RELEASE_DEBOUNCE_MS 15U
#define MORSE_FLIPPER_AUDIO_WAIT_DRAW_MS 30U
#define MORSE_FLIPPER_RF_TX_TAIL_DITS 2U
#define MORSE_FLIPPER_RF_RSSI_WINDOW_MS 160U
#define MORSE_FLIPPER_RF_RSSI_PEAK_DECAY_MS 240U
#define MORSE_FLIPPER_RF_LIVE_DECODERS 0U
#define _SHOW_ANSWER_WHILE_TRAINING_LCWO 1U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S 6U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S     3U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S     10U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S 3U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S 15U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S 3U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_DEFAULT_S 6U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S     3U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S     10U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S        3U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S        15U
#define MORSE_FLIPPER_STRAIGHT_NEXT_DEFAULT_S    3U
#define MORSE_FLIPPER_STRAIGHT_CHARSET           "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define MORSE_FLIPPER_RF_FREQ_DIGITS             6U
#define MORSE_FLIPPER_TONE_OFF_IDX               0xFFU
#define MORSE_FLIPPER_DEFAULT_TONE_IDX           20U

#define MORSE_SOURCE_STRAIGHT_GPIO (1UL << 0)
#define MORSE_SOURCE_STRAIGHT_BTN  (1UL << 1)
#define MORSE_SOURCE_KEYER_DIT     (1UL << 2)
#define MORSE_SOURCE_KEYER_DAH     (1UL << 3)
#define MORSE_SOURCE_HAM_MACRO     (1UL << 4)

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
static const GpioPin* morse_flipper_ground_pin = NULL;
static const GpioPin* morse_flipper_ptt_pin = NULL;

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
    MorseFlipperAudioPathBuzzer = 0,
    MorseFlipperAudioPathGpioP2Hd = 1,
    MorseFlipperAudioPathVibration = 2,
} MorseFlipperAudioPath;

typedef enum {
    MorseFlipperScreenHome = 0,
    MorseFlipperScreenRun = 1,
    MorseFlipperScreenTrace = 2,
    MorseFlipperScreenTrainer = 3,
    MorseFlipperScreenSession = 4,
    MorseFlipperScreenRf = 5,
    MorseFlipperScreenRfRx = 6,
    MorseFlipperScreenRfFreq = 7,
    MorseFlipperScreenStraight = 8,
    MorseFlipperScreenSessionEnd = 9,
    MorseFlipperScreenMenu = 10,
    MorseFlipperScreenHelp = 11,
    MorseFlipperScreenAbout = 12,
    MorseFlipperScreenStartupProbe = 13,
    MorseFlipperScreenHamRun = 14,
    MorseFlipperScreenHamStartRefusal = 15,
    MorseFlipperScreenHamAssign = 16,
    MorseFlipperScreenHamAssignments = 17,
} MorseFlipperScreen;

typedef enum {
    MorseFlipperViewMenu = 0,
    MorseFlipperViewLive,
    MorseFlipperViewSettings,
    MorseFlipperViewWidget,
    MorseFlipperViewTextInput,
} MorseFlipperView;

typedef enum {
    MorseFlipperSceneMenuMain = 0,
    MorseFlipperSceneMenuTraining,
    MorseFlipperSceneMenuSettings,
    MorseFlipperSceneMenuHelp,
    MorseFlipperSceneMenuRf,
    MorseFlipperSceneMenuHam,
    MorseFlipperSceneRun,
    MorseFlipperSceneRf,
    MorseFlipperSceneRfRx,
    MorseFlipperSceneRfFreq,
    MorseFlipperSceneSession,
    MorseFlipperSceneStraight,
    MorseFlipperSceneSessionEnd,
    MorseFlipperSceneHome,
    MorseFlipperSceneAudioCfg,
    MorseFlipperSceneTrainer,
    MorseFlipperSceneStraightCfg,
    MorseFlipperScenePc,
    MorseFlipperSceneTrace,
    MorseFlipperSceneGpio,
    MorseFlipperSceneHelp,
    MorseFlipperSceneAbout,
    MorseFlipperSceneStartupProbe,
    MorseFlipperSceneHamRun,
    MorseFlipperSceneHamStartRefusal,
    MorseFlipperSceneHamConfigure,
    MorseFlipperSceneHamMessageActions,
    MorseFlipperSceneHamTextInput,
    MorseFlipperSceneHamAssign,
    MorseFlipperSceneHamAssignments,
    MorseFlipperSceneNum,
} MorseFlipperScene;

typedef enum {
    MorseFlipperHamMenuStart = 1,
    MorseFlipperHamMenuLogging,
    MorseFlipperHamMenuConfigure,
    MorseFlipperHamMenuAssignments,
} MorseFlipperHamMenuItem;

typedef enum {
    MorseFlipperHamConfigureAdd = 1,
    MorseFlipperHamConfigureMessageBase = 32,
} MorseFlipperHamConfigureItem;

typedef enum {
    MorseFlipperHamActionAssign = 1,
    MorseFlipperHamActionEdit,
    MorseFlipperHamActionDelete,
} MorseFlipperHamActionItem;

typedef enum {
    MorseFlipperHamTextModeNone = 0,
    MorseFlipperHamTextModeAdd,
    MorseFlipperHamTextModeEdit,
} MorseFlipperHamTextMode;

typedef enum {
    MorseFlipperCustomHamTextDone = 0x1A00,
} MorseFlipperCustomEvent;

typedef enum {
    MorseFlipperHelpFirstSteps = 0,
    MorseFlipperHelpInputKeys,
    MorseFlipperHelpConnectingPaddle,
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
    MorseFlipperSettingAudio,
    MorseFlipperSettingGpio,
} MorseFlipperSettingIndex;

typedef enum {
    MorseFlipperAudioSettingPath = 0,
    MorseFlipperAudioSettingTone,
    MorseFlipperAudioSettingP2Volume,
} MorseFlipperAudioSettingIndex;

typedef enum {
    MorseFlipperGpioSettingDit = 0,
    MorseFlipperGpioSettingDah,
    MorseFlipperGpioSettingGround,
    MorseFlipperGpioSettingPtt,
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

const char* const morse_flipper_usb_mode_names[] = {
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

const uint8_t morse_flipper_input_values[] = {
    MorseFlipperInputSourceButtons,
    MorseFlipperInputSourceStraight,
    MorseFlipperInputSourcePaddle,
};

const char* const morse_flipper_input_names[] = {
    "buttons",
    "straight",
    "paddle",
};

const char* const morse_flipper_audio_path_names[] = {
    "Buzzer",
    "P2 (HD)",
    "Vibration",
};

const uint8_t morse_flipper_keyer_values[] = {
    MorseKeyerModeStraight,
    MorseKeyerModeBug,
    MorseKeyerModePlainIambic,
    MorseKeyerModeIambicA,
    MorseKeyerModeIambicB,
    MorseKeyerModeUltimatic,
    MorseKeyerModeKeyahead,
};

const MorseFlipperTone morse_flipper_tones[] = {
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
    TextInput* text_input;
    VariableItemList* settings_list;
    Widget* widget;
    VariableItem* audio_cfg_items[MorseFlipperAudioSettingP2Volume + 1U];
    VariableItem* trainer_items[MorseFlipperTrainerSettingChars + 1U];
    VariableItem* straight_cfg_items[3];
    View* live_view;
    Gui* gui;
    DialogsApp* dialogs;
    NotificationApp* notifications;
    FuriString* help_text;
    volatile bool exit_requested;
    FuriHalUsbInterface* previous_usb_config;
    FuriHalUsbHidConfig hid_cfg;
    bool tone_on;
    bool speaker_owned;
    bool speaker_busy;
    float speaker_hz;
    bool transport_connected;
    bool mouse_invert;
    bool left_down;
    bool ok_down;
    bool back_down;
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
    uint8_t pc_mode_pref;
    uint8_t pc_paddle_preset;
    uint8_t pc_straight_preset;
    uint8_t scene;
    uint8_t handedness;
    uint8_t input_source;
    uint8_t audio_path;
    uint8_t keyer_mode;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t gpio_ptt_idx;
    uint8_t trainer_row;
    uint8_t help_topic;
    uint8_t help_page;
    uint8_t about_ok_count;
    uint8_t ham_selected_message;
    uint8_t ham_text_mode;
    uint8_t rf_freq_focus;
    uint8_t trainer_farnsworth_wpm;
    uint8_t trainer_answer_timeout_s;
    uint8_t trainer_group_pause_s;
    uint8_t straight_answer_timeout_s;
    uint8_t straight_next_delay_s;
    uint8_t trainer_char_idx;
    uint8_t trainer_mark_idx;
    uint8_t session_wait_draw_s;
    uint8_t gpio_edit_dit_idx;
    uint8_t gpio_edit_dah_idx;
    uint8_t gpio_edit_ground_idx;
    uint8_t gpio_edit_ptt_idx;
    uint8_t gpio_probe_state;
    uint8_t startup_gpio_probe_state;
    bool vail_mode_active;
    bool vail_speed_active;
    bool vail_tone_active;
    uint8_t vail_keyer_mode;
    uint16_t vail_dit_ms;
    uint8_t vail_tone_idx;
    MorseKeyer keyer;
    uint8_t tone_idx;
    uint8_t p2_volume_pct;
    uint8_t preview_ticks;
    uint8_t input_mask;
    uint32_t trainer_next_at;
    uint32_t straight_next_at;
    uint32_t straight_wait_started_at;
    uint32_t straight_last_input_at;
    uint32_t straight_mark_started_at;
    uint16_t run_dit_ms;
    uint16_t straight_dit_ms;
    uint16_t straight_session_total;
    uint16_t straight_session_good;
    uint32_t session_last_input_at;
    uint32_t session_result_until;
    uint32_t session_next_group_at;
    uint32_t session_complete_at;
    uint32_t rf_tx_tail_until;
    uint32_t rf_tx_edge_at;
    uint32_t rf_rssi_next_at;
    uint32_t rf_rssi_peak_decay_at;
    uint32_t gpio_edge_at;
    uint32_t gpio_probe_notice_until;
    uint32_t ptt_tail_until;
    uint32_t rf_edit_khz;
    int32_t rf_rssi_sum_dbm;
    uint32_t paddle_sources[MorseKeyerPaddleCount];
    uint32_t note_sources[3];
    MorseTrainer trainer;
    MorseFlipperHamKeyer ham_keyer;
    MorseTrainerCustomSets custom_sets;
    MorseTrainerStraightStats straight_stats;
    uint8_t straight_hist_cnt[36];
    uint16_t straight_hist_sum[36];
    char straight_worst_line[24];
    bool straight_playback_active;
    bool straight_playback_mark;
    bool straight_started;
    bool straight_wait_answer;
    bool straight_done;
    bool straight_key_down;
    bool rf_live_active;
    bool rf_tx_level;
    bool rf_tx_gap_flushed;
    bool rf_rssi_valid;
    bool rf_carrier_present;
    bool rf_monitor_tone;
    bool audio_wait_active;
    bool ptt_level;
    bool ham_key_level;
    bool gpio_level;
    bool gpio_gap_flushed;
    uint8_t straight_mark_idx;
    uint8_t straight_return_screen;
    uint8_t backlight_mode;
    uint8_t session_end_flash_phase;
    int8_t rf_rssi_dbm;
    int8_t rf_monitor_threshold_dbm;
    int8_t rf_rssi_peak_dbm;
    uint16_t rf_rssi_samples;
    uint16_t rf_rx_edges_window;
    uint16_t rf_rx_activity;
    bool ham_macro_active;
    bool ham_macro_mark;
    uint8_t ham_macro_char_idx;
    uint8_t ham_macro_mark_idx;
    uint8_t ham_macro_dir;
    uint32_t ham_macro_next_at;
    uint32_t ham_notice_until;
    char rf_rx_text[64];
    char rf_tx_text[64];
    char gpio_text[64];
    char ham_text_buffer[MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN + 1U];
    char ham_macro_text[MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN + 1U];
    char ham_notice[16];
    MorseFlipperRunHistory run_history;
    MorseFlipperAudioPwm audio_pwm;
    MorseFlipperStraightFilter straight_filter;
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

void morse_flipper_set_paddle_source( MorseFlipperApp* app, uint8_t paddle, uint32_t source_mask, bool active, uint32_t now_ms);
void morse_flipper_set_note_source( MorseFlipperApp* app, uint8_t note, uint32_t source_mask, bool active);
void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_poll(MorseFlipperApp* app);
void morse_flipper_release_all_notes(MorseFlipperApp* app);
void morse_flipper_load_config(MorseFlipperApp* app);
void morse_flipper_save_config(const MorseFlipperApp* app);
void morse_flipper_load_rf_config(MorseFlipperApp* app);
void morse_flipper_save_rf_config(const MorseFlipperApp* app);
uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app);
void morse_flipper_set_run_wpm(MorseFlipperApp* app, uint8_t wpm);
uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app);
void morse_flipper_clamp_trainer_settings(MorseFlipperApp* app);
void morse_flipper_clamp_straight_settings(MorseFlipperApp* app);
void morse_flipper_set_pc_mode(MorseFlipperApp* app, uint8_t mode);
void morse_flipper_handle_midi_rx(MorseFlipperApp* app);
void morse_flipper_update_sidetone(MorseFlipperApp* app);
void morse_flipper_midi_rx_ready(void* context);
static void morse_flipper_handle_active_keying_event( MorseFlipperApp* app, const InputEvent* event);
const char* morse_flipper_pc_state_name(const MorseFlipperApp* app);
uint8_t morse_flipper_nearest_tone_idx_for_midi(uint8_t midi_note);
bool morse_flipper_transport_connected(const MorseFlipperApp* app);
void morse_flipper_release_mouse_buttons(void);
void morse_flipper_send_transport_note(MorseFlipperApp* app, uint8_t note, bool active);
void morse_flipper_resync_transport_notes(MorseFlipperApp* app);
static void morse_flipper_toggle_source(MorseFlipperApp* app);
static bool morse_flipper_training_playback_active(const MorseFlipperApp* app);
static bool morse_flipper_straight_answer_down(const MorseFlipperApp* app);
static void morse_flipper_cycle_mode(MorseFlipperApp* app);
static void morse_flipper_tone_nudge(MorseFlipperApp* app, int dir);
bool morse_flipper_local_buzzer_enabled(const MorseFlipperApp* app);
bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app);
bool morse_flipper_use_pwm_buzzer(const MorseFlipperApp* app);
bool morse_flipper_any_active_notes(const MorseFlipperApp* app);
uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app);
void morse_flipper_sync_audio_output(MorseFlipperApp* app);
const char* morse_flipper_current_tone_name(const MorseFlipperApp* app);
float morse_flipper_active_tone_hz(const MorseFlipperApp* app);
static void morse_flipper_toggle_handedness(MorseFlipperApp* app);
static void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_help_open(MorseFlipperApp* app);
void morse_flipper_about_open(MorseFlipperApp* app);
static void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir);
void morse_flipper_apply_trainer_charset_choice(MorseFlipperApp* app);
static void morse_flipper_drop_live_keying_for_playback(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_begin_group_playback(MorseFlipperApp* app, uint32_t now_ms);
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
static void morse_flipper_gpio_probe_reset(MorseFlipperApp* app);
static void morse_flipper_gpio_probe_prepare(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_gpio_probe_tick(MorseFlipperApp* app, uint32_t now_ms);
uint8_t morse_flipper_gpio_probe_sample_raw(MorseFlipperApp* app);
static bool morse_flipper_gpio_probe_notice_active(const MorseFlipperApp* app);
static bool morse_flipper_gpio_probe_blocks_start(const MorseFlipperApp* app);
static bool morse_flipper_gpio_probe_use_straight(const MorseFlipperApp* app);
static void morse_flipper_feed_straight_mark(MorseFlipperApp* app, uint16_t mark_ms, uint32_t now_ms);
static void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_reset_straight_session(MorseFlipperApp* app);
static void morse_flipper_note_straight_session(MorseFlipperApp* app);
static void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_finish_straight_round(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_tick_ham_macro(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_ham_start_macro(
    MorseFlipperApp* app,
    const char* text,
    uint32_t now_ms);
void morse_flipper_ham_stop_macro(MorseFlipperApp* app);
static void morse_flipper_ham_gpio_apply(MorseFlipperApp* app);
void morse_flipper_ham_gpio_release(MorseFlipperApp* app);
static void morse_flipper_ham_log_append_text(
    MorseFlipperApp* app,
    const char* text,
    uint32_t now_ms);
static void morse_flipper_ham_log_append_marker(
    MorseFlipperApp* app,
    const char* marker,
    uint32_t now_ms);
void morse_flipper_ham_log_flush(MorseFlipperApp* app);
static void morse_flipper_ham_log_flush_if_idle(MorseFlipperApp* app, uint32_t now_ms);
static void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms);
void morse_flipper_draw(Canvas* canvas, void* ctx);
void morse_flipper_view_dirty(MorseFlipperApp* app);
void morse_flipper_scene_open(MorseFlipperApp* app, uint32_t scene);
void morse_flipper_scene_back(MorseFlipperApp* app);
void morse_flipper_live_draw(Canvas* canvas, void* model);
static uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app);
static void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms);
uint32_t morse_flipper_settings_list_state(VariableItemList* list);
void morse_flipper_settings_list_restore(VariableItemList* list, uint32_t state);
uint8_t morse_flipper_input_value_index(uint8_t source);
uint8_t morse_flipper_keyer_value_index(uint8_t mode);
uint16_t morse_flipper_wpm_to_dit_ms(uint8_t wpm);
uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app);
uint16_t morse_flipper_current_straight_dit_ms(const MorseFlipperApp* app);
void morse_flipper_drain_keyer_events(MorseFlipperApp* app);
uint32_t morse_flipper_note_source_for_paddle(uint8_t paddle);
uint8_t morse_flipper_note_for_paddle(uint8_t paddle);
uint8_t morse_flipper_gpio_straight_idx(const MorseFlipperApp* app);
void morse_flipper_gpio_sync_straight_idx(MorseFlipperApp* app);
static const GpioPin* morse_flipper_gpio_pin_ptr(uint8_t pin_idx);
static void morse_flipper_gpio_bind_from_app(const MorseFlipperApp* app);
static void morse_flipper_gpio_reset_candidates(void);
static void morse_flipper_gpio_apply(MorseFlipperApp* app);
void morse_flipper_gpio_alert(MorseFlipperApp* app, MorseFlipperGpioRule rule);
bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    uint8_t ptt,
    MorseFlipperGpioRule* out_rule);
void morse_flipper_sync_ptt(MorseFlipperApp* app, uint32_t now_ms);
static const char* morse_flipper_status_line( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static const char* morse_flipper_run_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static const char* morse_flipper_run_mode_line( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static const char* morse_flipper_run_input_name(const MorseFlipperApp* app);
static const char* morse_flipper_run_keyer_name(const MorseFlipperApp* app);
static const char* morse_flipper_run_usb_name(const MorseFlipperApp* app);
static const char* morse_flipper_rf_khz_line( const MorseFlipperApp* app, char* buf, size_t buf_sz);
static void morse_flipper_reset_run_state(MorseFlipperApp* app);
static const char* morse_flipper_trace_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz);
void morse_flipper_settings_noop_enter(void* context, uint32_t index);
void morse_flipper_settings_enter_callback(void* context, uint32_t index);
void morse_flipper_settings_wpm_changed(VariableItem* item);
void morse_flipper_settings_input_changed(VariableItem* item);
void morse_flipper_settings_keyer_changed(VariableItem* item);
void morse_flipper_settings_swap_changed(VariableItem* item);
void morse_flipper_settings_tone_changed(VariableItem* item);
void morse_flipper_settings_gpio_dit_changed(VariableItem* item);
void morse_flipper_settings_gpio_dah_changed(VariableItem* item);
void morse_flipper_settings_gpio_ground_changed(VariableItem* item);
void morse_flipper_settings_gpio_ptt_changed(VariableItem* item);
void morse_flipper_settings_usb_mode_changed(VariableItem* item);
void morse_flipper_settings_usb_paddle_changed(VariableItem* item);
void morse_flipper_settings_usb_straight_changed(VariableItem* item);
void morse_flipper_settings_usb_mouse_swap_changed(VariableItem* item);
void morse_flipper_scene_menu_pick(void* ctx, uint32_t idx);
void morse_flipper_scene_enter_now(MorseFlipperApp* app, uint32_t scene);
void morse_flipper_trainer_lesson_changed(VariableItem* item);
void morse_flipper_trainer_wpm_changed(VariableItem* item);
void morse_flipper_trainer_farnsworth_changed(VariableItem* item);
void morse_flipper_trainer_answer_timeout_changed(VariableItem* item);
void morse_flipper_trainer_group_pause_changed(VariableItem* item);
void morse_flipper_trainer_group_size_changed(VariableItem* item);
void morse_flipper_trainer_groups_changed(VariableItem* item);
void morse_flipper_trainer_chars_changed(VariableItem* item);
void morse_flipper_trainer_menu_refresh(MorseFlipperApp* app);
static bool morse_flipper_input_chunk_a(MorseFlipperApp* app, InputEvent* event);
static bool morse_flipper_input_chunk_b( MorseFlipperApp* app, InputEvent* event, uint32_t now_ms);

MorseFlipperApp* morse_flipper_boot(void);
ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app);
void morse_flipper_shutdown(MorseFlipperApp* app);

static uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app) {
    if(app == NULL) return MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace ||
       app->screen == MorseFlipperScreenHamRun ||
       app->screen == MorseFlipperScreenStraight)
        return MorseFlipperBacklightHold;

    if(app->screen == MorseFlipperScreenRf || app->screen == MorseFlipperScreenRfRx)
        return app->rf_live_active ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenSession ||
       app->screen == MorseFlipperScreenSessionEnd)
        return app->session_started ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    return MorseFlipperBacklightAuto;
}

static void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t want;

    if(app == NULL || app->notifications == NULL) return;
    UNUSED(now_ms);

    want = morse_flipper_backlight_mode(app);
    if(want != app->backlight_mode) {
        if(app->backlight_mode != MorseFlipperBacklightAuto)
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

        app->backlight_mode = want;

        if(want != MorseFlipperBacklightAuto)
            notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }
}

uint8_t morse_flipper_input_value_index(uint8_t source) {
    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_input_values); i++) {
        if(morse_flipper_input_values[i] == source) {
            return i;
        }
    }

    return 0U;
}

void morse_flipper_set_local_wpm(MorseFlipperApp* app, uint8_t wpm) {
    if(app == NULL) return;

    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;

    app->trainer.local_dit_ms = morse_flipper_wpm_to_dit_ms(wpm);
    morse_flipper_clamp_trainer_settings(app);
    morse_flipper_cw_decoder_init(&app->rf_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->gpio_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_refresh_keyer(app, furi_get_tick());
}

void morse_flipper_set_run_wpm(MorseFlipperApp* app, uint8_t wpm) {
    if(app == NULL) return;

    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;

    app->run_dit_ms = morse_flipper_wpm_to_dit_ms(wpm);
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
    morse_flipper_refresh_keyer(app, furi_get_tick());
    morse_flipper_view_dirty(app);
}

uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app)
{
    uint16_t dit;
    uint8_t wpm;

    if(app == NULL) return 0U;
    dit = app->straight_dit_ms ? app->straight_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    wpm = (uint8_t)((1200U + (dit / 2U)) / dit);
    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    return wpm;
}

void morse_flipper_set_straight_wpm(MorseFlipperApp* app, uint8_t wpm)
{
    if(app == NULL) return;

    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    app->straight_dit_ms = morse_flipper_wpm_to_dit_ms(wpm);
    morse_flipper_clamp_straight_settings(app);
}

static uint16_t morse_flipper_trainer_farnsworth_unit_ms(const MorseFlipperApp* app) {
    uint32_t w;
    uint32_t farn;
    uint32_t dit;
    uint32_t total;
    uint32_t spare;

    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS;

    w = morse_flipper_local_wpm(app);
    farn = app->trainer_farnsworth_wpm;
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

uint8_t morse_flipper_keyer_value_index(uint8_t mode) {
    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_keyer_values); i++) {
        if(morse_flipper_keyer_values[i] == mode) {
            return i;
        }
    }

    return 0U;
}

uint16_t morse_flipper_wpm_to_dit_ms(uint8_t wpm) {
    if(wpm < 1U) {
        return MORSE_FLIPPER_DEFAULT_DIT_MS;
    }

    return (1200U + (wpm / 2U)) / wpm;
}

uint8_t morse_flipper_gpio_straight_idx(const MorseFlipperApp* app) {
    if(app == NULL) {
        return morse_flipper_gpio_default_dit();
    }

    return morse_flipper_gpio_pin_valid(app->gpio_dit_idx) ? app->gpio_dit_idx :
                                                             morse_flipper_gpio_default_dit();
}

void morse_flipper_gpio_sync_straight_idx(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->gpio_straight_idx = morse_flipper_gpio_straight_idx(app);
}

static const GpioPin* morse_flipper_gpio_pin_ptr(uint8_t pin_idx) {
    if(!morse_flipper_gpio_pin_valid(pin_idx)) {
        return morse_flipper_gpio_pins[morse_flipper_gpio_default_dit()];
    }

    return morse_flipper_gpio_pins[pin_idx];
}

static uint8_t morse_flipper_gpio_no_reserved(uint8_t pin_idx, uint8_t def) {
    return (pin_idx == MorseFlipperGpioPinP2 || pin_idx == MorseFlipperGpioPinP15) ? def : pin_idx;
}

static bool morse_flipper_scene_supports_audio_pwm(uint8_t scene) {
    switch(scene) {
    case MorseFlipperSceneHamRun:
    case MorseFlipperSceneRun:
    case MorseFlipperSceneTrace:
    case MorseFlipperSceneSession:
    case MorseFlipperSceneSessionEnd:
    case MorseFlipperSceneStraight:
    case MorseFlipperSceneRf:
    case MorseFlipperSceneRfRx:
        return true;
    default:
        return false;
    }
}

static bool morse_flipper_audio_wait_transition(const MorseFlipperApp* app, uint8_t old_scene, uint8_t new_scene)
{
    if(app == NULL || app->audio_path != MorseFlipperAudioPathGpioP2Hd || old_scene == new_scene) return false;
    return morse_flipper_scene_supports_audio_pwm(old_scene) ||
           morse_flipper_scene_supports_audio_pwm(new_scene);
}

uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app)
{
    uint8_t pct = app ? app->p2_volume_pct : 100U;

    if(pct < 10U) pct = 10U;
    if(pct > 100U) pct = 100U;
    return pct;
}

bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app)
{
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenHamRun) return false;
    return app->audio_path == MorseFlipperAudioPathGpioP2Hd &&
           morse_flipper_scene_supports_audio_pwm(app->scene);
}

static const GpioPin* morse_flipper_gpio_ground_pin_ptr(uint8_t pin_idx) {
    if(pin_idx == MORSE_FLIPPER_GPIO_PIN_NONE) {
        return NULL;
    }

    if(!morse_flipper_gpio_pin_valid(pin_idx)) {
        return morse_flipper_gpio_pins[morse_flipper_gpio_default_ground()];
    }

    return morse_flipper_gpio_pins[pin_idx];
}

static const GpioPin* morse_flipper_gpio_ptt_pin_ptr(const MorseFlipperApp* app) {
    uint8_t ptt;

    if(app == NULL) return NULL;

    ptt = app->gpio_ptt_idx;
    if(ptt == MORSE_FLIPPER_GPIO_PIN_NONE) return NULL;
    if(ptt != MorseFlipperGpioPinP15) return NULL;
    if(ptt == morse_flipper_gpio_straight_idx(app) || ptt == app->gpio_dit_idx ||
       ptt == app->gpio_dah_idx || ptt == app->gpio_ground_idx)
        return NULL;

    return morse_flipper_gpio_pins[ptt];
}

static void morse_flipper_gpio_bind_from_app(const MorseFlipperApp* app) {
    morse_flipper_straight_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_reserved(
            morse_flipper_gpio_straight_idx(app), morse_flipper_gpio_default_dit()));
    morse_flipper_dit_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_reserved(app->gpio_dit_idx, morse_flipper_gpio_default_dit()));
    morse_flipper_dah_pin = morse_flipper_gpio_pin_ptr(
        morse_flipper_gpio_no_reserved(app->gpio_dah_idx, morse_flipper_gpio_default_dah()));
    morse_flipper_ground_pin = morse_flipper_gpio_ground_pin_ptr(
        morse_flipper_gpio_no_reserved(
            app->gpio_ground_idx, morse_flipper_gpio_default_ground()));
    morse_flipper_ptt_pin = morse_flipper_gpio_ptt_pin_ptr(app);
}

static void morse_flipper_gpio_reset_candidates(void) {
    for(size_t i = 0; i < COUNT_OF(morse_flipper_gpio_pins); i++) {
        furi_hal_gpio_init(morse_flipper_gpio_pins[i], GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
}

static void morse_flipper_gpio_apply(MorseFlipperApp* app) {
    morse_flipper_gpio_bind_from_app(app);
    morse_flipper_gpio_reset_candidates();

    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    if(morse_flipper_ground_pin != NULL) {
        furi_hal_gpio_init( morse_flipper_ground_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(morse_flipper_ground_pin, false);
    }
    if(morse_flipper_ptt_pin != NULL) {
        furi_hal_gpio_init(morse_flipper_ptt_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_write(morse_flipper_ptt_pin, false);
    }

    if(app != NULL) {
        app->ptt_level = false;
        app->ptt_tail_until = 0U;
    }
}

void morse_flipper_gpio_alert(MorseFlipperApp* app, MorseFlipperGpioRule rule) {
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

bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    uint8_t ptt,
    MorseFlipperGpioRule* out_rule) {
    MorseFlipperGpioRule rule = morse_flipper_gpio_validate(dit, dah, ground);

    if(out_rule != NULL) {
        *out_rule = rule;
    }

    if(rule != MorseFlipperGpioRuleOk) {
        return false;
    }

    app->gpio_dit_idx = dit;
    app->gpio_dah_idx = dah;
    app->gpio_ground_idx = ground;
    app->gpio_ptt_idx = ptt == MorseFlipperGpioPinP15 ? MorseFlipperGpioPinP15 : MORSE_FLIPPER_GPIO_PIN_NONE;
    morse_flipper_gpio_sync_straight_idx(app);
    morse_flipper_gpio_apply(app);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
    morse_flipper_save_config(app);
    return true;
}

void morse_flipper_sync_ptt(MorseFlipperApp* app, uint32_t now_ms) {
    bool tx_active;
    bool want_high;
    bool want_key;
    const GpioPin* ham_key_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP15];
    const GpioPin* ham_ptt_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP16];

    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenHamRun) {
        tx_active = (app->note_sources[0] != 0U) || (app->note_sources[1] != 0U) ||
                    (app->note_sources[2] != 0U);
        want_key = app->ham_keyer.break_in_enabled && tx_active;
        if(app->ham_keyer.break_in_enabled && tx_active)
            app->ptt_tail_until = now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * 7U);
        want_high = app->ham_keyer.break_in_enabled &&
                    (tx_active || (app->ptt_tail_until != 0U && now_ms < app->ptt_tail_until));
        if(!want_high) app->ptt_tail_until = 0U;

        if(app->ham_key_level != want_key) {
            furi_hal_gpio_write(ham_key_pin, want_key);
            app->ham_key_level = want_key;
        }
        if(app->ptt_level != want_high) {
            furi_hal_gpio_write(ham_ptt_pin, want_high);
            app->ptt_level = want_high;
        }
        return;
    }

    if(app->screen != MorseFlipperScreenRun || morse_flipper_ptt_pin == NULL) {
        app->ptt_tail_until = 0U;
        want_high = false;
    } else {
        tx_active = (app->note_sources[0] != 0U) || (app->note_sources[1] != 0U) ||
                    (app->note_sources[2] != 0U) || (app->preview_ticks > 0U);
        if(tx_active) app->ptt_tail_until = now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * 7U);
        want_high = tx_active || (app->ptt_tail_until != 0U && now_ms < app->ptt_tail_until);
        if(!want_high) app->ptt_tail_until = 0U;
    }

    if(app->ptt_level == want_high) return;
    if(morse_flipper_ptt_pin != NULL) furi_hal_gpio_write(morse_flipper_ptt_pin, want_high);
    app->ptt_level = want_high;
}

uint8_t morse_flipper_current_keyer_mode(const MorseFlipperApp* app) {
    if(app != NULL && app->screen == MorseFlipperScreenStraight) {
        return MorseKeyerModeStraight;
    }

    if(app->vail_mode_active) {
        return app->vail_keyer_mode;
    }

    return app->keyer_mode;
}

uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app) {
    if(app != NULL &&
       (app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenHamRun) &&
       app->run_dit_ms != 0U) {
        return app->run_dit_ms;
    }

    if(app->vail_speed_active) {
        return app->vail_dit_ms;
    }

    return app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
}

uint16_t morse_flipper_current_straight_dit_ms(const MorseFlipperApp* app)
{
    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS;
    return app->straight_dit_ms ? app->straight_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
}

uint8_t morse_flipper_current_wpm(const MorseFlipperApp* app) {
    uint16_t dit_ms = morse_flipper_current_dit_ms(app);

    if(dit_ms == 0U) {
        return 0U;
    }

    return (uint8_t)((1200U + (dit_ms / 2U)) / dit_ms);
}

static bool morse_flipper_straight_like_mode(const MorseFlipperApp* app) {
    uint8_t mode = morse_flipper_current_keyer_mode(app);

    return mode == MorseKeyerModePassthrough || mode == MorseKeyerModeStraight;
}

static bool morse_flipper_gpio_probe_screen(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    return app->screen == MorseFlipperScreenSession || app->screen == MorseFlipperScreenStraight;
}

static bool morse_flipper_gpio_probe_before_start(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenSession) return !app->session_started;
    if(app->screen == MorseFlipperScreenStraight) return !app->straight_started;
    return false;
}

static uint8_t morse_flipper_gpio_probe_sample(const MorseFlipperApp* app) {
    if(app == NULL || app->input_source != MorseFlipperInputSourcePaddle ||
       morse_flipper_ground_pin == NULL || !morse_flipper_gpio_probe_screen(app)) {
        return MorseFlipperGpioProbeOk;
    }

    return morse_flipper_gpio_probe_sample_raw((MorseFlipperApp*)app);
}

uint8_t morse_flipper_gpio_probe_sample_raw(MorseFlipperApp* app) {
    bool dit_follows = true;
    bool dah_follows = true;

    if(app == NULL || morse_flipper_ground_pin == NULL) {
        return MorseFlipperGpioProbeOk;
    }

    furi_hal_gpio_init(morse_flipper_dit_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_dah_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(morse_flipper_ground_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    for(uint8_t i = 0U; i < 3U; i++) {
        furi_hal_gpio_write(morse_flipper_ground_pin, false);
        furi_delay_us(10U);
        if(furi_hal_gpio_read(morse_flipper_dit_pin)) dit_follows = false;
        if(furi_hal_gpio_read(morse_flipper_dah_pin)) dah_follows = false;

        furi_hal_gpio_write(morse_flipper_ground_pin, true);
        furi_delay_us(10U);
        if(!furi_hal_gpio_read(morse_flipper_dit_pin)) dit_follows = false;
        if(!furi_hal_gpio_read(morse_flipper_dah_pin)) dah_follows = false;
    }

    morse_flipper_gpio_apply(app);
    return morse_flipper_gpio_probe_paddle(dit_follows, dah_follows);
}

static void morse_flipper_gpio_probe_reset(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->gpio_probe_state = MorseFlipperGpioProbeOk;
    app->gpio_probe_notice_until = 0U;
}

static void morse_flipper_gpio_probe_prepare(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_gpio_probe_reset(app);
    if(!morse_flipper_gpio_probe_screen(app)) return;

    app->gpio_probe_state = morse_flipper_gpio_probe_sample(app);
    if(app->screen == MorseFlipperScreenSession &&
       app->gpio_probe_state == MorseFlipperGpioProbeGroundToDah) {
        app->gpio_probe_notice_until = now_ms + 1000U;
    }
}

static void morse_flipper_gpio_probe_tick(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t state;

    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenSession && app->gpio_probe_notice_until != 0U &&
       now_ms >= app->gpio_probe_notice_until) {
        app->gpio_probe_notice_until = 0U;
        morse_flipper_view_dirty(app);
    }

    if(!morse_flipper_gpio_probe_screen(app) || !morse_flipper_gpio_probe_before_start(app) ||
       app->input_source != MorseFlipperInputSourcePaddle ||
       !morse_flipper_gpio_probe_blocks(app->gpio_probe_state)) {
        return;
    }

    state = morse_flipper_gpio_probe_sample(app);
    if(state == app->gpio_probe_state) return;

    app->gpio_probe_state = state;
    if(app->screen == MorseFlipperScreenSession &&
       app->gpio_probe_state == MorseFlipperGpioProbeGroundToDah) {
        app->gpio_probe_notice_until = now_ms + 1000U;
    } else {
        app->gpio_probe_notice_until = 0U;
    }
    morse_flipper_view_dirty(app);
}

static bool morse_flipper_gpio_probe_notice_active(const MorseFlipperApp* app) {
    return app != NULL && app->screen == MorseFlipperScreenSession && !app->session_started &&
           app->gpio_probe_notice_until != 0U && furi_get_tick() < app->gpio_probe_notice_until;
}

static bool morse_flipper_gpio_probe_blocks_start(const MorseFlipperApp* app) {
    return app != NULL && app->input_source == MorseFlipperInputSourcePaddle &&
           morse_flipper_gpio_probe_screen(app) &&
           morse_flipper_gpio_probe_blocks(app->gpio_probe_state);
}

static bool morse_flipper_gpio_probe_use_straight(const MorseFlipperApp* app) {
    return app != NULL && app->input_source == MorseFlipperInputSourcePaddle &&
           morse_flipper_gpio_probe_screen(app) &&
           morse_flipper_gpio_probe_forces_straight(app->gpio_probe_state);
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

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace) {
        g.live = true;
    } else if(app->screen == MorseFlipperScreenSession) {
        g.live = morse_flipper_session_repeat_active(app) || morse_flipper_session_running_view(app);
    } else if(app->screen == MorseFlipperScreenRf || app->screen == MorseFlipperScreenRfRx) {
        g.live = app->rf_live_active;
    } else if(app->screen == MorseFlipperScreenStraight) {
        g.live = app->straight_wait_answer;
    }

    if(!g.live || app->input_source != MorseFlipperInputSourceButtons) {
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

static bool morse_flipper_live_left_hint(const MorseFlipperApp* app) {
    if(app != NULL && app->screen == MorseFlipperScreenHamRun) return true;
    return morse_flipper_input_gate(app).left_hint;
}

static const char* morse_flipper_status_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->speaker_busy) {
        snprintf(buf, buf_sz, "speaker busy");
        return buf;
    }

    if(app->input_source == MorseFlipperInputSourceButtons) {
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

    if(app->input_source == MorseFlipperInputSourcePaddle) {
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

    snprintf( buf, buf_sz, "src %s straight", morse_flipper_gpio_name(morse_flipper_gpio_straight_idx(app)));
    return buf;
}

static const char* morse_flipper_hand_name(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? "swp" : "norm";
}

uint8_t morse_flipper_ok_button_paddle(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? MorseKeyerPaddleDit :
                                                                MorseKeyerPaddleDah;
}

uint8_t morse_flipper_back_button_paddle(const MorseFlipperApp* app) {
    return (app->handedness == MorseFlipperHandednessSwapped) ? MorseKeyerPaddleDah :
                                                                MorseKeyerPaddleDit;
}

static const char* morse_flipper_run_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        snprintf(
            buf,
            buf_sz,
            "%s",
            morse_flipper_live_back_is_key(app) ? "Left clear; hold L exit" :
                                                  "Bk exit; Left clear");
        return buf;
    }

    snprintf(buf, buf_sz, "Bk exit; Left clear");
    return buf;
}

static int8_t morse_flipper_rssi_dbm_round(float rssi)
{
    if(rssi >= 0.0f) return (int8_t)(rssi + 0.5f);
    return (int8_t)(rssi - 0.5f);
}

static int8_t morse_flipper_rf_clamp_dbm(int8_t dbm)
{
    if(dbm < -115) return -115;
    if(dbm > -50) return -50;
    return dbm;
}

static const char* morse_flipper_run_mode_line( const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    const char* input;
    const char* keyer;

    if(app == NULL || buf == NULL || buf_sz == 0U) return "---";

    input = morse_flipper_run_input_name(app);
    keyer = morse_flipper_run_keyer_name(app);
    if(morse_flipper_straight_like_mode(app) || strcmp(keyer, "---") == 0) {
        snprintf(buf, buf_sz, "%u wpm  %s", (unsigned)morse_flipper_current_wpm(app), input);
    } else {
        snprintf(
            buf,
            buf_sz,
            "%u wpm  %s  %s",
            (unsigned)morse_flipper_current_wpm(app),
            input,
            keyer);
    }

    return buf;
}

static const char* morse_flipper_run_input_name(const MorseFlipperApp* app) {
    if(app == NULL) return "---";
    if(app->input_source == MorseFlipperInputSourceButtons) return "buttons";
    if(app->input_source == MorseFlipperInputSourcePaddle) return "paddles";
    return "straight";
}

static const char* morse_flipper_run_keyer_name(const MorseFlipperApp* app) {
    uint8_t mode;

    if(app == NULL) return "---";
    if(morse_flipper_straight_like_mode(app)) return "---";

    mode = morse_flipper_current_keyer_mode(app);
    switch(mode) {
    case MorseKeyerModeBug:
        return "bug";
    case MorseKeyerModeElBug:
        return "elbug";
    case MorseKeyerModeSingleDot:
        return "single-dot";
    case MorseKeyerModeUltimatic:
        return "ultimatic";
    case MorseKeyerModePlainIambic:
        return "plain";
    case MorseKeyerModeIambicA:
        return "elekey-a";
    case MorseKeyerModeIambicB:
        return "elekey-b";
    case MorseKeyerModeKeyahead:
        return "keyahead";
    default:
        return "---";
    }
}

static const char* morse_flipper_run_usb_name(const MorseFlipperApp* app) {
    if(app == NULL) return "USB off";

    switch(app->pc_mode) {
    case MorseFlipperPcModeMidi:
        return "MIDI";
    case MorseFlipperPcModeKeyboard:
        return "Kbd";
    case MorseFlipperPcModeMouse:
        return "Mouse";
    default:
        return "USB off";
    }
}

static bool morse_flipper_rf_tx_allowed_hz(uint32_t hz)
{
    return furi_hal_subghz_is_frequency_valid(hz) && furi_hal_region_is_frequency_allowed(hz);
}

static bool morse_flipper_rf_tx_allowed_khz(uint32_t khz)
{
    return morse_flipper_rf_tx_allowed_hz(khz * 1000U);
}

static void morse_flipper_rf_reset_edit(MorseFlipperApp* app)
{
    if(app == NULL) return;
    app->rf_edit_khz = morse_flipper_rf_frequency_khz(&app->rf);
    app->rf_freq_focus = 0U;
}

static uint32_t morse_flipper_rf_edit_place(uint8_t focus)
{
    static const uint32_t places[MORSE_FLIPPER_RF_FREQ_DIGITS] = {
        100000U, 10000U, 1000U, 100U, 10U, 1U};

    if(focus >= MORSE_FLIPPER_RF_FREQ_DIGITS) return 1U;
    return places[focus];
}

static void morse_flipper_rf_bump_focus(MorseFlipperApp* app, int dir)
{
    uint8_t focus;

    if(app == NULL) return;
    focus = app->rf_freq_focus % MORSE_FLIPPER_RF_FREQ_DIGITS;
    if(dir < 0) {
        app->rf_freq_focus =
            focus == 0U ? (MORSE_FLIPPER_RF_FREQ_DIGITS - 1U) : (uint8_t)(focus - 1U);
    } else {
        app->rf_freq_focus = (uint8_t)((focus + 1U) % MORSE_FLIPPER_RF_FREQ_DIGITS);
    }
}

static void morse_flipper_rf_bump_digit(MorseFlipperApp* app, int dir)
{
    uint32_t place;
    uint32_t khz;
    uint8_t digit;
    uint8_t next_digit;
    int32_t delta;

    if(app == NULL) return;

    place = morse_flipper_rf_edit_place(app->rf_freq_focus);
    khz = app->rf_edit_khz % 1000000U;
    digit = (uint8_t)((khz / place) % 10U);
    next_digit = dir < 0 ? (uint8_t)((digit + 9U) % 10U) : (uint8_t)((digit + 1U) % 10U);
    delta = ((int32_t)next_digit - (int32_t)digit) * (int32_t)place;
    app->rf_edit_khz = (uint32_t)((int32_t)khz + delta);
}

static void morse_flipper_rf_commit_edit(MorseFlipperApp* app)
{
    uint32_t khz;

    if(app == NULL) return;

    khz = app->rf_edit_khz % 1000000U;
    if(!morse_flipper_rf_tx_allowed_khz(khz)) khz = MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ;
    app->rf_edit_khz = khz;
    morse_flipper_rf_set_frequency_hz(&app->rf, khz * 1000U);
    morse_flipper_save_rf_config(app);
}

static const char* morse_flipper_rf_khz_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    unsigned long khz = MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ;

    if(buf == NULL || buf_sz == 0U) return "433150 khz";
    if(app != NULL) khz = (unsigned long)morse_flipper_rf_frequency_khz(&app->rf);
    snprintf(buf, buf_sz, "%lu khz", khz);
    return buf;
}

static const char* morse_flipper_rf_rssi_line(const MorseFlipperApp* app, char* buf, size_t buf_sz)
{
    if(buf == NULL || buf_sz == 0U) return "";

    if(app == NULL || !app->rf_rssi_valid) {
        snprintf(buf, buf_sz, "cs0 --/--");
        return buf;
    }

    snprintf(
        buf,
        buf_sz,
        "cs%d %d/%d",
        app->rf_carrier_present ? 1 : 0,
        (int)app->rf_rssi_dbm,
        (int)app->rf_rssi_peak_dbm);
    return buf;
}

static const char* morse_flipper_trace_hint( const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        snprintf( buf, buf_sz, "%s", morse_flipper_live_back_is_key(app) ? "key live hold L" : "OK str Bk back");
        return buf;
    }

    snprintf(buf, buf_sz, "gpio live  Bk back");
    return buf;
}

static const char* morse_flipper_source_short_name( const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        snprintf(buf, buf_sz, "btn");
        return buf;
    }

    if(app->input_source == MorseFlipperInputSourcePaddle) {
        snprintf(
            buf,
            buf_sz,
            "%s/%s",
            morse_flipper_gpio_name(app->gpio_dit_idx),
            morse_flipper_gpio_name(app->gpio_dah_idx));
        return buf;
    }

    snprintf(buf, buf_sz, "%s", morse_flipper_gpio_name(morse_flipper_gpio_straight_idx(app)));
    return buf;
}

static bool morse_flipper_session_left_exit_active(const MorseFlipperApp* app) {
    return morse_flipper_live_left_hint(app);
}

static void morse_flipper_reset_run_state(MorseFlipperApp* app) {
    if(app == NULL) return;

    morse_flipper_run_history_reset(&app->run_history);
    morse_flipper_audio_pwm_set_gate(&app->audio_pwm, false);
    morse_flipper_straight_filter_reset(&app->straight_filter);
    app->rf_tx_text[0] = '\0';
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
}

void morse_flipper_sync_audio_output(MorseFlipperApp* app)
{
    if(app == NULL) return;

    if(app->audio_pwm.running) morse_flipper_audio_pwm_stop(&app->audio_pwm);

    if(morse_flipper_audio_output_is_pwm(app)) {
        morse_flipper_audio_pwm_prepare(
            &app->audio_pwm,
            MORSE_FLIPPER_AUDIO_PWM_CARRIER_HZ,
            MORSE_FLIPPER_AUDIO_PWM_SAMPLE_RATE_HZ,
            (uint32_t)(morse_flipper_active_tone_hz(app) + 0.5f),
            morse_flipper_p2_volume_pct(app),
            MORSE_FLIPPER_AUDIO_PWM_FADE_MS,
            MORSE_FLIPPER_AUDIO_PWM_FADE_MS);
        morse_flipper_audio_pwm_start(&app->audio_pwm);
    }

    morse_flipper_update_sidetone(app);
}

static bool morse_flipper_session_running_view(const MorseFlipperApp* app) {
    if(app == NULL || app->screen != MorseFlipperScreenSession) return false;
    return !morse_flipper_session_idle_view(app);
}

#include "morse_flipper_session.h"
#include "morse_flipper_session.c"
#include "morse_flipper_input.c"

#include "morse_flipper_runtime.c"
#include "morse_flipper_rf_live.c"
#include "morse_flipper_help.c"

static char morse_flipper_ham_upper(char ch)
{
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static void morse_flipper_ham_gpio_apply(MorseFlipperApp* app)
{
    const GpioPin* key_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP15];
    const GpioPin* ptt_pin = morse_flipper_gpio_pins[MorseFlipperGpioPinP16];

    if(app == NULL) return;

    furi_hal_gpio_init(key_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(ptt_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(key_pin, false);
    furi_hal_gpio_write(ptt_pin, false);
    app->ham_key_level = false;
    app->ptt_level = false;
    app->ptt_tail_until = 0U;
}

void morse_flipper_ham_gpio_release(MorseFlipperApp* app)
{
    if(app == NULL) return;

    furi_hal_gpio_write(morse_flipper_gpio_pins[MorseFlipperGpioPinP15], false);
    furi_hal_gpio_write(morse_flipper_gpio_pins[MorseFlipperGpioPinP16], false);
    app->ham_key_level = false;
    app->ptt_level = false;
    app->ptt_tail_until = 0U;
    morse_flipper_gpio_apply(app);
}

void morse_flipper_ham_stop_macro(MorseFlipperApp* app)
{
    if(app == NULL) return;

    app->ham_macro_active = false;
    app->ham_macro_mark = false;
    app->ham_macro_char_idx = 0U;
    app->ham_macro_mark_idx = 0U;
    app->ham_macro_dir = MORSE_FLIPPER_HAM_KEYER_UNASSIGNED;
    app->ham_macro_next_at = 0U;
    app->ham_macro_text[0] = '\0';
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_HAM_MACRO, false);
}

static void morse_flipper_ham_start_macro(
    MorseFlipperApp* app,
    const char* text,
    uint32_t now_ms)
{
    if(app == NULL || text == NULL || text[0] == '\0') return;

    morse_flipper_ham_stop_macro(app);
    strncpy(app->ham_macro_text, text, sizeof(app->ham_macro_text) - 1U);
    app->ham_macro_text[sizeof(app->ham_macro_text) - 1U] = '\0';
    app->ham_macro_active = true;
    app->ham_macro_next_at = now_ms;
    morse_flipper_view_dirty(app);
}

static void morse_flipper_tick_ham_macro(MorseFlipperApp* app, uint32_t now_ms)
{
    const char* morse;
    char ch;
    uint32_t dit_ms;

    if(app == NULL || !app->ham_macro_active) return;

    if(app->screen != MorseFlipperScreenHamRun) {
        morse_flipper_ham_stop_macro(app);
        return;
    }

    if(now_ms < app->ham_macro_next_at) return;

    dit_ms = morse_flipper_current_dit_ms(app);
    ch = morse_flipper_ham_upper(app->ham_macro_text[app->ham_macro_char_idx]);

    if(ch == '\0') {
        morse_flipper_ham_stop_macro(app);
        morse_flipper_view_dirty(app);
        return;
    }

    if(!app->ham_macro_mark && ch == ' ') {
        app->ham_macro_char_idx++;
        app->ham_macro_mark_idx = 0U;
        app->ham_macro_next_at = now_ms + (dit_ms * 7U);
        return;
    }

    morse = morse_trainer_char_morse(ch);
    if(morse == NULL || morse[0] == '\0') {
        app->ham_macro_char_idx++;
        app->ham_macro_mark_idx = 0U;
        app->ham_macro_next_at = now_ms + (dit_ms * 3U);
        return;
    }

    if(!app->ham_macro_mark) {
        app->ham_macro_mark = true;
        morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_HAM_MACRO, true);
        app->ham_macro_next_at =
            now_ms + (morse[app->ham_macro_mark_idx] == '-' ? (dit_ms * 3U) : dit_ms);
        morse_flipper_view_dirty(app);
        return;
    }

    app->ham_macro_mark = false;
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_HAM_MACRO, false);
    if(morse[app->ham_macro_mark_idx + 1U] != '\0') {
        app->ham_macro_mark_idx++;
        app->ham_macro_next_at = now_ms + dit_ms;
    } else {
        app->ham_macro_char_idx++;
        app->ham_macro_mark_idx = 0U;
        app->ham_macro_next_at = now_ms + (dit_ms * 3U);
    }
    morse_flipper_view_dirty(app);
}

static void morse_flipper_ham_log_now(char* date_key, size_t date_sz, char* stamp, size_t stamp_sz)
{
    DateTime dt = {0};

    furi_hal_rtc_get_datetime(&dt);
    if(date_key != NULL && date_sz != 0U) {
        snprintf(
            date_key,
            date_sz,
            "%04u-%02u-%02u",
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day);
    }
    if(stamp != NULL && stamp_sz != 0U) {
        snprintf(
            stamp,
            stamp_sz,
            "%04u-%02u-%02u %02u:%02u",
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day,
            (unsigned)dt.hour,
            (unsigned)dt.minute);
    }
}

static void morse_flipper_ham_log_append_text(
    MorseFlipperApp* app,
    const char* text,
    uint32_t now_ms)
{
    char date_key[MORSE_FLIPPER_HAM_KEYER_DATE_LEN + 1U];
    char stamp[MORSE_FLIPPER_HAM_KEYER_STAMP_LEN + 1U];

    if(app == NULL || text == NULL || text[0] == '\0') return;
    if(app->screen != MorseFlipperScreenHamRun || !app->ham_keyer.logging_enabled) return;

    morse_flipper_ham_log_now(date_key, sizeof(date_key), stamp, sizeof(stamp));
    morse_flipper_ham_keyer_append_activity(&app->ham_keyer, text, now_ms, date_key, stamp);
}

static void morse_flipper_ham_log_append_marker(
    MorseFlipperApp* app,
    const char* marker,
    uint32_t now_ms)
{
    char date_key[MORSE_FLIPPER_HAM_KEYER_DATE_LEN + 1U];
    char stamp[MORSE_FLIPPER_HAM_KEYER_STAMP_LEN + 1U];

    if(app == NULL || marker == NULL || marker[0] == '\0') return;
    if(app->screen != MorseFlipperScreenHamRun || !app->ham_keyer.logging_enabled) return;

    morse_flipper_ham_log_now(date_key, sizeof(date_key), stamp, sizeof(stamp));
    morse_flipper_ham_keyer_append_marker(&app->ham_keyer, marker, now_ms, date_key, stamp);
}

void morse_flipper_ham_log_flush(MorseFlipperApp* app)
{
    Storage* storage;
    File* file;
    char path[96];
    char header[MORSE_FLIPPER_HAM_KEYER_STAMP_LEN + 3U];
    bool ok;

    if(app == NULL || app->ham_keyer.pending_len == 0U) return;

    if(!app->ham_keyer.logging_enabled) {
        morse_flipper_ham_keyer_clear_pending(&app->ham_keyer);
        return;
    }

    snprintf(
        path,
        sizeof(path),
        "%s%s.txt",
        MORSE_FLIPPER_HAM_KEYER_LOG_PREFIX,
        app->ham_keyer.current_log_date[0] ? app->ham_keyer.current_log_date : "unknown");
    snprintf(header, sizeof(header), "%s\n", app->ham_keyer.pending_stamp);

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    storage_common_mkdir(storage, MORSE_FLIPPER_HAM_DIR);
    ok = storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) {
        storage_file_write(file, header, strlen(header));
        storage_file_write(file, app->ham_keyer.pending, app->ham_keyer.pending_len);
        if(app->ham_keyer.pending[app->ham_keyer.pending_len - 1U] != '\n')
            storage_file_write(file, "\n", 1U);
        storage_file_write(file, "\n", 1U);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    morse_flipper_ham_keyer_clear_pending(&app->ham_keyer);
}

static void morse_flipper_ham_log_flush_if_idle(MorseFlipperApp* app, uint32_t now_ms)
{
    if(app == NULL || app->screen != MorseFlipperScreenHamRun) return;
    if(morse_flipper_any_active_notes(app) || app->ham_macro_active) return;
    if(morse_flipper_ham_keyer_should_flush(&app->ham_keyer, now_ms))
        morse_flipper_ham_log_flush(app);
}

void morse_flipper_enter_screen(
    MorseFlipperApp* app,
    uint8_t screen,
    uint8_t scene,
    uint32_t now_ms) {
    uint8_t old_screen;
    uint8_t old_scene;
    bool audio_wait;

    if(app->screen == screen && app->scene == scene) return;
    old_screen = app->screen;
    old_scene = app->scene;
    audio_wait = morse_flipper_audio_wait_transition(app, old_scene, scene);

    if(app->screen == MorseFlipperScreenSession && screen != MorseFlipperScreenSession &&
       screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenSessionEnd && screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenStraight && screen != MorseFlipperScreenStraight) {
        morse_flipper_reset_straight_state(app, now_ms);
    }

    if((app->screen == MorseFlipperScreenRf || app->screen == MorseFlipperScreenRfRx) &&
       screen != MorseFlipperScreenRf && screen != MorseFlipperScreenRfRx) {
        app->rf_live_active = false;
        app->rf_carrier_present = false;
        app->rf_monitor_tone = false;
        morse_flipper_radio_sync_live(
            &app->radio,
            morse_flipper_rf_frequency_hz(&app->rf),
            false,
            false,
            MorseFlipperRadioProfileOokData);
        morse_flipper_radio_set_tx_level(&app->radio, false);
    }

    if(app->scene == MorseFlipperSceneRun && scene != MorseFlipperSceneRun) {
        app->run_dit_ms = 0U;
    }

    if(app->screen == MorseFlipperScreenHamRun && screen != MorseFlipperScreenHamRun) {
        morse_flipper_ham_log_flush(app);
        morse_flipper_ham_stop_macro(app);
        morse_flipper_ham_gpio_release(app);
        app->run_dit_ms = 0U;
    }

    morse_flipper_clear_button_keying(app, now_ms);

    if(screen == MorseFlipperScreenSession && app->screen != MorseFlipperScreenSession &&
       app->screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(screen == MorseFlipperScreenSessionEnd && app->screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_drop_live_keying_for_playback(app, now_ms);
        morse_flipper_update_sidetone(app);
    }

    if(screen == MorseFlipperScreenStraight && app->screen != MorseFlipperScreenStraight) {
        morse_flipper_reset_straight_state(app, now_ms);
    }

    if(scene == MorseFlipperSceneRun && app->scene != MorseFlipperSceneRun) {
        app->preview_ticks = 0U;
        app->run_dit_ms = morse_flipper_current_dit_ms(app);
        morse_flipper_reset_run_state(app);
    }

    if(screen == MorseFlipperScreenHamRun && app->screen != MorseFlipperScreenHamRun) {
        app->preview_ticks = 0U;
        app->run_dit_ms = morse_flipper_current_dit_ms(app);
        morse_flipper_reset_run_state(app);
        morse_flipper_ham_gpio_apply(app);
    }

    if(screen == MorseFlipperScreenRf && app->screen != MorseFlipperScreenRf) {
        app->rf_live_active = true;
        app->rf_rssi_valid = false;
        app->rf_rssi_dbm = 0;
        app->rf_rssi_peak_dbm = 0;
        app->rf_rssi_sum_dbm = 0;
        app->rf_rssi_samples = 0U;
        app->rf_rssi_next_at = 0U;
        app->rf_rx_edges_window = 0U;
        app->rf_rx_activity = 0U;
        app->rf_rssi_peak_decay_at = 0U;
        app->rf_carrier_present = false;
        app->rf_monitor_tone = false;
        app->rf_rx_text[0] = '\0';
        app->rf_tx_tail_until = 0U;
        morse_flipper_rf_reset_live(&app->rf);
        morse_flipper_cw_decoder_init(&app->rf_decoder, morse_flipper_current_dit_ms(app));
        morse_flipper_reset_run_state(app);
    }

    if(screen == MorseFlipperScreenRfRx && app->screen != MorseFlipperScreenRfRx) {
        app->rf_live_active = true;
        app->rf_rssi_valid = false;
        app->rf_rssi_dbm = 0;
        app->rf_rssi_peak_dbm = 0;
        app->rf_rssi_sum_dbm = 0;
        app->rf_rssi_samples = 0U;
        app->rf_rssi_next_at = 0U;
        app->rf_rx_edges_window = 0U;
        app->rf_rx_activity = 0U;
        app->rf_rssi_peak_decay_at = 0U;
        app->rf_carrier_present = false;
        app->rf_monitor_tone = false;
        app->rf_rx_text[0] = '\0';
        morse_flipper_rf_reset_live(&app->rf);
    }

    if(screen == MorseFlipperScreenRfFreq && app->screen != MorseFlipperScreenRfFreq) {
        morse_flipper_rf_reset_edit(app);
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
    app->scene = scene;
    if(morse_flipper_gpio_probe_screen(app)) {
        morse_flipper_gpio_probe_prepare(app, now_ms);
    } else {
        morse_flipper_gpio_probe_reset(app);
    }
    if(old_screen == MorseFlipperScreenStraight || screen == MorseFlipperScreenStraight) {
        morse_flipper_refresh_keyer(app, now_ms);
    }
    if(screen == MorseFlipperScreenHome) {
        morse_flipper_poll(app);
    }
    if(audio_wait) {
        app->audio_wait_active = true;
        morse_flipper_view_dirty(app);
        furi_delay_ms(MORSE_FLIPPER_AUDIO_WAIT_DRAW_MS);
    }
    if(old_scene != scene || morse_flipper_scene_supports_audio_pwm(scene) ||
       morse_flipper_scene_supports_audio_pwm(old_scene))
        morse_flipper_sync_audio_output(app);
    if(audio_wait) app->audio_wait_active = false;
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

static bool morse_flipper_training_playback_active(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenStraight) return app->straight_playback_active;
    return app->screen == MorseFlipperScreenSession && app->trainer_playback_active;
}

static bool morse_flipper_straight_answer_down(const MorseFlipperApp* app) {
    if(app == NULL) return false;

    if(app->input_source == MorseFlipperInputSourceButtons) return false;
    if(morse_flipper_gpio_probe_blocks_start(app)) return false;
    if(app->input_source == MorseFlipperInputSourcePaddle)
        return morse_flipper_gpio_probe_use_straight(app) ? !furi_hal_gpio_read(morse_flipper_dit_pin) :
                                                            (morse_flipper_logical_dit_down(app) ||
                                                             morse_flipper_logical_dah_down(app));
    return morse_flipper_straight_down();
}

static void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->straight_playback_active = false;
    app->straight_playback_mark = false;
    app->straight_started = false;
    app->straight_wait_answer = false;
    app->straight_done = false;
    app->straight_key_down = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = 0U;
    app->straight_wait_started_at = 0U;
    app->straight_last_input_at = 0U;
    app->straight_mark_started_at = 0U;
    app->straight_trainer.active = false;
    app->straight_trainer.answer[0] = '\0';
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
   morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_update_sidetone(app);
    morse_flipper_clear_button_keying(app, now_ms);
}

static void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!app->straight_started) morse_flipper_reset_straight_session(app);
    morse_flipper_reset_straight_state(app, now_ms);
    morse_flipper_straight_trainer_start( &app->straight_trainer, MORSE_FLIPPER_STRAIGHT_CHARSET, morse_flipper_current_straight_dit_ms(app));
    app->straight_started = true;
    app->straight_playback_active = true;
    app->straight_playback_mark = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = now_ms;
    morse_flipper_view_dirty(app);
}

static uint8_t morse_flipper_straight_hist_idx(char ch)
{
    if(ch >= 'A' && ch <= 'Z') return (uint8_t)(ch - 'A');
    if(ch >= '0' && ch <= '9') return (uint8_t)(26U + (ch - '0'));
    return 0xFFU;
}

static uint16_t morse_flipper_straight_attempt_sum(const MorseFlipperApp* app)
{
    uint16_t sum = 0U;

    if(app == NULL) return 0U;
    if(morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] == '\0') return 0U;
    if(!morse_flipper_straight_trainer_symbol_count_match(&app->straight_trainer)) return 0U;
    sum += morse_flipper_straight_trainer_worst_space_score(&app->straight_trainer);
    sum += morse_flipper_straight_trainer_worst_dit_score(&app->straight_trainer);
    sum += morse_flipper_straight_trainer_worst_dah_score(&app->straight_trainer);
    return sum;
}

static void morse_flipper_straight_refresh_worst(MorseFlipperApp* app)
{
    uint8_t pick[5] = {0xFFU,0xFFU,0xFFU,0xFFU,0xFFU};
    size_t at = 0U;

    if(app == NULL) return;

    for(uint8_t j = 0U; j < 5U; j++) {
        uint8_t best = 0xFFU;
        uint16_t best_avg = 0xFFFFU;

        for(uint8_t i = 0U; i < 36U; i++) {
            bool used = false;
            uint16_t avg;

            if(app->straight_hist_cnt[i] < 3U) continue;
            for(uint8_t k = 0U; k < j; k++) if(pick[k] == i) used = true;
            if(used) continue;

            avg = (uint16_t)(app->straight_hist_sum[i] / app->straight_hist_cnt[i]);
            if(avg < best_avg) {
                best_avg = avg;
                best = i;
            }
        }

        pick[j] = best;
    }

    memset(app->straight_worst_line, 0, sizeof(app->straight_worst_line));
    memcpy(app->straight_worst_line, "Wo:", 3U);
    at = 3U;
    for(uint8_t j = 0U; j < 5U; j++) {
        char ch;

        if(pick[j] == 0xFFU) break;
        ch = pick[j] < 26U ? (char)('A' + pick[j]) : (char)('0' + (pick[j] - 26U));
        if(at + 3U >= sizeof(app->straight_worst_line)) break;
        app->straight_worst_line[at++] = ' ';
        app->straight_worst_line[at++] = ch;
    }

    if(at == 3U) snprintf(app->straight_worst_line, sizeof(app->straight_worst_line), "Wo: -");
}

static void morse_flipper_reset_straight_session(MorseFlipperApp* app)
{
    if(app == NULL) return;
    app->straight_session_total = 0U;
    app->straight_session_good = 0U;
    memset(app->straight_hist_cnt, 0, sizeof(app->straight_hist_cnt));
    memset(app->straight_hist_sum, 0, sizeof(app->straight_hist_sum));
    snprintf(app->straight_worst_line, sizeof(app->straight_worst_line), "Wo: -");
}

static void morse_flipper_note_straight_session(MorseFlipperApp* app)
{
    uint8_t idx;

    if(app == NULL) return;

    app->straight_session_total++;
    if(strcmp(
           morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
           morse_flipper_straight_trainer_answer(&app->straight_trainer)) == 0)
        app->straight_session_good++;

    idx = morse_flipper_straight_hist_idx(morse_flipper_straight_trainer_target_char(&app->straight_trainer));
    if(idx != 0xFFU) {
        if(app->straight_hist_cnt[idx] != 0xFFU) app->straight_hist_cnt[idx]++;
        app->straight_hist_sum[idx] += morse_flipper_straight_attempt_sum(app);
    }

    morse_flipper_straight_refresh_worst(app);
}

static void morse_flipper_finish_straight_round(MorseFlipperApp* app, uint32_t now_ms)
{
    const char* answer;
    uint16_t err_ms;
    uint8_t drift_pct;
    bool hard_fail;

    if(app == NULL) return;

    app->straight_wait_answer = false;
    app->straight_done = true;
    app->straight_trainer.active = false;
    app->straight_next_at = now_ms + ((uint32_t)app->straight_next_delay_s * 1000U);
    morse_flipper_note_straight_session(app);
    answer = morse_flipper_straight_trainer_answer(&app->straight_trainer);
    hard_fail = answer[0] == '\0' ||
                !morse_flipper_straight_trainer_symbol_count_match(&app->straight_trainer);
    if(hard_fail) {
        err_ms = 0xFFFFU;
        drift_pct = 100U;
    } else {
        err_ms = morse_flipper_straight_trainer_average_mark_error_ms(&app->straight_trainer);
        drift_pct = morse_flipper_straight_trainer_average_drift_percent(&app->straight_trainer);
    }
    morse_trainer_note_straight_attempt(
        &app->straight_stats,
        morse_flipper_straight_trainer_target_char(&app->straight_trainer),
        err_ms,
        drift_pct,
        morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
        answer);
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
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
        return;
    }

    if(morse[app->trainer_mark_idx] == '\0') {
        app->trainer_playback_active = false;
        app->trainer_next_at = 0U;
        morse_flipper_update_sidetone(app);
        return;
    }

    app->trainer_playback_mark = true;
    app->trainer_next_at =
        now_ms + ((morse[app->trainer_mark_idx] == '-') ? (dit_ms * 3U) : dit_ms);
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms) {
    const char* morse;
    uint32_t dit_ms;
    size_t len;

    if(app == NULL) return;

    if(app->straight_playback_active && now_ms >= app->straight_next_at) {
        morse = morse_flipper_straight_trainer_target_morse(&app->straight_trainer);
        dit_ms = morse_flipper_current_straight_dit_ms(app);
        len = strlen(morse);

        if(!app->straight_playback_mark) {
            if(app->straight_mark_idx >= len) {
                app->straight_playback_active = false;
                app->straight_wait_answer = true;
                app->straight_wait_started_at = now_ms;
                app->straight_next_at = 0U;
                morse_flipper_update_sidetone(app);
                morse_flipper_view_dirty(app);
                return;
            }

            app->straight_playback_mark = true;
            app->straight_next_at =
                now_ms + (morse[app->straight_mark_idx] == '-' ? (dit_ms * 3U) : dit_ms);
            morse_flipper_update_sidetone(app);
            morse_flipper_view_dirty(app);
            return;
        }

        app->straight_playback_mark = false;
        app->straight_mark_idx++;
        if(app->straight_mark_idx >= len) {
            app->straight_playback_active = false;
            app->straight_wait_answer = true;
            app->straight_wait_started_at = now_ms;
            app->straight_next_at = 0U;
        } else {
            app->straight_next_at = now_ms + dit_ms;
        }
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
    }

    if(app->straight_wait_answer &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] == '\0') {
        uint32_t timeout_ms = (uint32_t)app->straight_answer_timeout_s * 1000U;

        if(app->straight_wait_started_at != 0U && now_ms - app->straight_wait_started_at >= timeout_ms) {
            morse_flipper_finish_straight_round(app, now_ms);
            return;
        }
    }

    if(app->straight_wait_answer &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
        uint32_t settle = morse_flipper_straight_answer_settle_ms(app);

        if(settle != 0U && !app->straight_key_down &&
           now_ms - app->straight_last_input_at >= settle) {
            morse_flipper_finish_straight_round(app, now_ms);
            return;
        }
    }

    if(app->straight_done && app->straight_next_at != 0U && now_ms >= app->straight_next_at) {
        morse_flipper_start_straight_round(app, now_ms);
    }
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

static void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenSession) {
        morse_flipper_leave_session(app, now_ms);
        return;
    }

    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_scene_back(app);
}

void morse_flipper_apply_trainer_charset_choice(MorseFlipperApp* app) {
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

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    app->keyer_mode = morse_keyer_next_ui_mode(app->keyer_mode);
    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_view_dirty(app);
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

#include "morse_flipper_live_view.c"
