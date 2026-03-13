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

#include "cw.h"
#include "keyer.h"
#include "morse_flipper_audio_pwm.h"
#include "morse_flipper_cw_decoder.h"
#include "morse_flipper_cw_token.h"
#include "morse_flipper_gpio.h"
#include "morse_flipper_gpio_probe.h"
#include "morse_flipper_ham_keyer.h"
#include "morse_flipper_paths.h"
#include "morse_flipper_radio.h"
#include "morse_flipper_run_history.h"
#include "morse_flipper_rf.h"
#include "morse_flipper_straight_filter.h"
#include "morse_flipper_straight_trainer.h"
#include "morse_flipper_tx_groups.h"
#include "pc_keys.h"
#include "trainer.h"
#include "trainer_files.h"
#include "usb/morse_usb_midi.h"

#define MORSE_FLIPPER_VOLUME                        0.7f
#define MORSE_FLIPPER_POLL_MS                       5
#define MORSE_FLIPPER_PREVIEW_TICKS                 8
#define MORSE_FLIPPER_CONFIG_PATH                   APP_DATA_PATH("config.bin")
#define MORSE_FLIPPER_RF_CONFIG_PATH                APP_DATA_PATH("rf.bin")
#define MORSE_FLIPPER_TXG_CONFIG_PATH               APP_DATA_PATH("tx_groups.bin")
#define MORSE_FLIPPER_CONFIG_VERSION                11
#define MORSE_FLIPPER_DEFAULT_DIT_MS                100U
#define MORSE_FLIPPER_SESSION_SETTLE_MS             1000U
#define MORSE_FLIPPER_SESSION_RESULT_MS             160U
#define MORSE_FLIPPER_TXG_RESULT_DELAY_MS           500U
#define MORSE_FLIPPER_STRAIGHT_SETTLE_MS            700U
#define MORSE_FLIPPER_STRAIGHT_RELEASE_DEBOUNCE_MS  15U
#define MORSE_FLIPPER_AUDIO_WAIT_DRAW_MS            30U
#define MORSE_FLIPPER_RF_TX_TAIL_DITS               2U
#define MORSE_FLIPPER_RF_RSSI_WINDOW_MS             160U
#define MORSE_FLIPPER_RF_RSSI_PEAK_DECAY_MS         240U
#define MORSE_FLIPPER_RF_LIVE_DECODERS              0U
#define _SHOW_ANSWER_WHILE_TRAINING_LCWO            1U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S     6U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S         3U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S         10U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S     3U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S     15U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S 3U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_DEFAULT_S    6U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S        3U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S        10U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S           3U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S           15U
#define MORSE_FLIPPER_STRAIGHT_NEXT_DEFAULT_S       3U
#define MORSE_FLIPPER_STRAIGHT_CHARSET              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define MORSE_FLIPPER_RF_FREQ_DIGITS                6U
#define MORSE_FLIPPER_TONE_OFF_IDX                  0xFFU
#define MORSE_FLIPPER_DEFAULT_TONE_IDX              20U

#define MORSE_SOURCE_STRAIGHT_GPIO (1UL << 0)
#define MORSE_SOURCE_STRAIGHT_BTN  (1UL << 1)
#define MORSE_SOURCE_KEYER_DIT     (1UL << 2)
#define MORSE_SOURCE_KEYER_DAH     (1UL << 3)
#define MORSE_SOURCE_HAM_MACRO     (1UL << 4)

#define MORSE_PADDLE_SOURCE_GPIO_DIT (1UL << 0)
#define MORSE_PADDLE_SOURCE_GPIO_DAH (1UL << 1)
#define MORSE_PADDLE_SOURCE_BTN_OK   (1UL << 2)
#define MORSE_PADDLE_SOURCE_BTN_BACK (1UL << 3)

extern const GpioPin* const morse_flipper_gpio_pins[MORSE_FLIPPER_GPIO_PIN_COUNT];
extern const GpioPin* morse_flipper_straight_pin;
extern const GpioPin* morse_flipper_dit_pin;
extern const GpioPin* morse_flipper_dah_pin;
extern const GpioPin* morse_flipper_ground_pin;
extern const GpioPin* morse_flipper_ptt_pin;

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
    MorseFlipperTxgDifficultyEasy = 0,
    MorseFlipperTxgDifficultyMedium,
    MorseFlipperTxgDifficultyCompetition,
    MorseFlipperTxgDifficultyCount,
} MorseFlipperTxgDifficulty;

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
    MorseFlipperScreenTxGroups = 18,
    MorseFlipperScreenTxGroupsResult = 19,
    MorseFlipperScreenTxGroupsFinal = 20,
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
    MorseFlipperSceneTxGroups,
    MorseFlipperSceneTxGroupsResult,
    MorseFlipperSceneTxGroupsFinal,
    MorseFlipperSceneTxGroupsCfg,
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

const NotificationSequence morse_flipper_led_good_twice = {
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    &message_delay_50,
    &message_green_255,
    &message_delay_50,
    &message_green_0,
    NULL,
};

const NotificationSequence morse_flipper_led_bad_twice = {
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

const NotificationSequence morse_flipper_led_miss_twice = {
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

const NotificationSequence morse_flipper_led_timeout_twice = {
    &message_red_255,
    &morse_flipper_msg_green_96,
    &message_delay_50,
    &message_red_0,
    &message_green_0,
    &message_delay_50,
    &message_red_255,
    &morse_flipper_msg_green_96,
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
    {"G2", 98.00f, 43U},   {"A2", 110.00f, 45U},  {"B2", 123.47f, 47U},  {"C3", 130.81f, 48U},
    {"D3", 146.83f, 50U},  {"E3", 164.81f, 52U},  {"F3", 174.61f, 53U},  {"G3", 196.00f, 55U},
    {"A3", 220.00f, 57U},  {"B3", 246.94f, 59U},  {"C4", 261.63f, 60U},  {"D4", 293.66f, 62U},
    {"E4", 329.63f, 64U},  {"F4", 349.23f, 65U},  {"G4", 392.00f, 67U},  {"A4", 440.00f, 69U},
    {"B4", 493.88f, 71U},  {"C5", 523.25f, 72U},  {"D5", 587.33f, 74U},  {"E5", 659.25f, 76U},
    {"F5", 698.46f, 77U},  {"G5", 783.99f, 79U},  {"A5", 880.00f, 81U},  {"B5", 987.77f, 83U},
    {"C6", 1046.50f, 84U}, {"D6", 1174.66f, 86U}, {"E6", 1318.51f, 88U}, {"F6", 1396.91f, 89U},
    {"G6", 1567.98f, 91U}, {"A6", 1760.00f, 93U}, {"B6", 1975.53f, 95U},
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
    uint32_t txg_wait_started_at;
    uint32_t txg_last_input_at;
    uint32_t txg_result_open_at;
    uint32_t txg_result_until;
    uint8_t txg_result_draw_s;
    uint16_t run_dit_ms;
    uint16_t straight_dit_ms;
    uint16_t straight_session_total;
    uint16_t straight_session_good;
    uint16_t txg_session_total;
    uint16_t txg_session_good;
    uint16_t txg_session_sk;
    uint32_t txg_sum_speed;
    uint32_t txg_sum_lgap;
    uint32_t txg_sum_ratio;
    uint32_t txg_sum_accuracy;
    uint32_t txg_sum_dgap;
    uint32_t txg_sum_variance;
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
    uint8_t straight_hist_cnt[36];
    uint16_t straight_hist_sum[36];
    char straight_worst_line[24];
    bool straight_playback_active;
    bool straight_playback_mark;
    bool straight_started;
    bool straight_wait_answer;
    bool straight_done;
    bool straight_key_down;
    bool txg_started;
    bool txg_wait_answer;
    bool txg_done;
    bool txg_sk;
    bool txg_start_holdoff;
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
    uint8_t straight_next_draw_s;
    uint8_t txg_repeated_timeouts;
    uint8_t txg_difficulty;
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
    MorseFlipperTxGroup tx_group;
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

void morse_flipper_set_paddle_source(
    MorseFlipperApp* app,
    uint8_t paddle,
    uint32_t source_mask,
    bool active,
    uint32_t now_ms);
void morse_flipper_set_note_source(
    MorseFlipperApp* app,
    uint8_t note,
    uint32_t source_mask,
    bool active);
void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_poll(MorseFlipperApp* app);
void morse_flipper_release_all_notes(MorseFlipperApp* app);
void morse_flipper_load_config(MorseFlipperApp* app);
void morse_flipper_save_config(const MorseFlipperApp* app);
void morse_flipper_load_rf_config(MorseFlipperApp* app);
void morse_flipper_save_rf_config(const MorseFlipperApp* app);
void morse_flipper_load_txg_config(MorseFlipperApp* app);
void morse_flipper_save_txg_config(const MorseFlipperApp* app);
uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app);
void morse_flipper_set_run_wpm(MorseFlipperApp* app, uint8_t wpm);
uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app);
void morse_flipper_clamp_trainer_settings(MorseFlipperApp* app);
void morse_flipper_clamp_straight_settings(MorseFlipperApp* app);
void morse_flipper_set_pc_mode(MorseFlipperApp* app, uint8_t mode);
void morse_flipper_handle_midi_rx(MorseFlipperApp* app);
void morse_flipper_update_sidetone(MorseFlipperApp* app);
void morse_flipper_midi_rx_ready(void* context);
void morse_flipper_handle_active_keying_event(MorseFlipperApp* app, const InputEvent* event);
const char* morse_flipper_pc_state_name(const MorseFlipperApp* app);
uint8_t morse_flipper_nearest_tone_idx_for_midi(uint8_t midi_note);
bool morse_flipper_transport_connected(const MorseFlipperApp* app);
void morse_flipper_release_mouse_buttons(void);
void morse_flipper_send_transport_note(MorseFlipperApp* app, uint8_t note, bool active);
void morse_flipper_resync_transport_notes(MorseFlipperApp* app);
void morse_flipper_toggle_source(MorseFlipperApp* app);
bool morse_flipper_training_playback_active(const MorseFlipperApp* app);
bool morse_flipper_straight_answer_down(const MorseFlipperApp* app);
void morse_flipper_cycle_mode(MorseFlipperApp* app);
void morse_flipper_tone_nudge(MorseFlipperApp* app, int dir);
bool morse_flipper_local_buzzer_enabled(const MorseFlipperApp* app);
bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app);
bool morse_flipper_use_pwm_buzzer(const MorseFlipperApp* app);
bool morse_flipper_any_active_notes(const MorseFlipperApp* app);
uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app);
void morse_flipper_sync_audio_output(MorseFlipperApp* app);
const char* morse_flipper_current_tone_name(const MorseFlipperApp* app);
float morse_flipper_active_tone_hz(const MorseFlipperApp* app);
void morse_flipper_toggle_handedness(MorseFlipperApp* app);
void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_help_open(MorseFlipperApp* app);
void morse_flipper_about_open(MorseFlipperApp* app);
void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir);
void morse_flipper_apply_trainer_charset_choice(MorseFlipperApp* app);
void morse_flipper_drop_live_keying_for_playback(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_begin_group_playback(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_start_session(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_reset_session_runtime(MorseFlipperApp* app);
void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_session_repeat_active(const MorseFlipperApp* app);
bool morse_flipper_session_idle_view(const MorseFlipperApp* app);
void morse_flipper_tick_session(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_leave_session(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_session_wait_key_down(const MorseFlipperApp* app);
bool morse_flipper_straight_down(void);
bool morse_flipper_logical_dit_down(const MorseFlipperApp* app);
bool morse_flipper_logical_dah_down(const MorseFlipperApp* app);
void morse_flipper_gpio_probe_reset(MorseFlipperApp* app);
void morse_flipper_gpio_probe_prepare(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_gpio_probe_tick(MorseFlipperApp* app, uint32_t now_ms);
uint8_t morse_flipper_gpio_probe_sample_raw(MorseFlipperApp* app);
bool morse_flipper_gpio_probe_screen(const MorseFlipperApp* app);
bool morse_flipper_gpio_probe_keep_state(uint8_t screen);
bool morse_flipper_gpio_probe_notice_active(const MorseFlipperApp* app);
bool morse_flipper_gpio_probe_blocks_start(const MorseFlipperApp* app);
bool morse_flipper_gpio_probe_use_straight(const MorseFlipperApp* app);
void morse_flipper_feed_straight_mark(MorseFlipperApp* app, uint16_t mark_ms, uint32_t now_ms);
void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_reset_straight_session(MorseFlipperApp* app);
void morse_flipper_note_straight_session(MorseFlipperApp* app);
bool morse_flipper_straight_countdown_active(const MorseFlipperApp* app);
void morse_flipper_start_straight_countdown(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_finish_straight_round(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_reset_tx_groups_state(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_start_tx_groups_round(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_leave_tx_groups(MorseFlipperApp* app, uint32_t now_ms);
const char* morse_flipper_txg_difficulty_name(uint8_t difficulty);
uint8_t morse_flipper_txg_range_low(uint8_t difficulty);
uint8_t morse_flipper_txg_range_high(uint8_t difficulty);
void morse_flipper_tick_ham_macro(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_ham_start_macro(MorseFlipperApp* app, const char* text, uint32_t now_ms);
void morse_flipper_ham_stop_macro(MorseFlipperApp* app);
void morse_flipper_ham_gpio_apply(MorseFlipperApp* app);
void morse_flipper_ham_gpio_release(MorseFlipperApp* app);
void morse_flipper_ham_log_append_text(MorseFlipperApp* app, const char* text, uint32_t now_ms);
void morse_flipper_ham_log_append_marker(MorseFlipperApp* app, const char* marker, uint32_t now_ms);
void morse_flipper_ham_log_flush(MorseFlipperApp* app);
void morse_flipper_ham_log_flush_if_idle(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms);
void morse_flipper_draw(Canvas* canvas, void* ctx);
void morse_flipper_view_dirty(MorseFlipperApp* app);
void morse_flipper_scene_open(MorseFlipperApp* app, uint32_t scene);
void morse_flipper_scene_back(MorseFlipperApp* app);
void morse_flipper_live_draw(Canvas* canvas, void* model);
static uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app);
void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms);
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
const GpioPin* morse_flipper_gpio_pin_ptr(uint8_t pin_idx);
void morse_flipper_gpio_bind_from_app(const MorseFlipperApp* app);
void morse_flipper_gpio_reset_candidates(void);
void morse_flipper_gpio_apply(MorseFlipperApp* app);
void morse_flipper_gpio_alert(MorseFlipperApp* app, MorseFlipperGpioRule rule);
bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    uint8_t ptt,
    MorseFlipperGpioRule* out_rule);
void morse_flipper_sync_ptt(MorseFlipperApp* app, uint32_t now_ms);
const char* morse_flipper_status_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_run_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_run_mode_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_run_input_name(const MorseFlipperApp* app);
const char* morse_flipper_run_keyer_name(const MorseFlipperApp* app);
const char* morse_flipper_run_usb_name(const MorseFlipperApp* app);
void morse_flipper_rf_reset_edit(MorseFlipperApp* app);
const char* morse_flipper_rf_khz_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
void morse_flipper_reset_run_state(MorseFlipperApp* app);
const char* morse_flipper_trace_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz);
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
bool morse_flipper_input_chunk_a(MorseFlipperApp* app, InputEvent* event);
bool morse_flipper_input_chunk_b(MorseFlipperApp* app, InputEvent* event, uint32_t now_ms);
const char* morse_flipper_input_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
uint32_t morse_flipper_straight_answer_settle_ms(const MorseFlipperApp* app);
void morse_flipper_draw_session_rows(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_session_bottom(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_session_end(Canvas* canvas, const MorseFlipperApp* app);

MorseFlipperApp* morse_flipper_boot(void);
ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app);
void morse_flipper_shutdown(MorseFlipperApp* app);

static uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app) {
    if(app == NULL) return MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace ||
       app->screen == MorseFlipperScreenHamRun || app->screen == MorseFlipperScreenStraight ||
       app->screen == MorseFlipperScreenTxGroups ||
       app->screen == MorseFlipperScreenTxGroupsResult ||
       app->screen == MorseFlipperScreenTxGroupsFinal)
        return MorseFlipperBacklightHold;

    if(app->screen == MorseFlipperScreenRf || app->screen == MorseFlipperScreenRfRx)
        return app->rf_live_active ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenSession || app->screen == MorseFlipperScreenSessionEnd)
        return app->session_started ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    return MorseFlipperBacklightAuto;
}

void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms) {
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

static void morse_flipper_feedback_do(MorseFlipperApp* app, const NotificationSequence* seq) {
    if(app == NULL) return;
    if(app->notifications && seq) notification_message(app->notifications, seq);
    app->session_result_tone = true;
    app->session_result_until = furi_get_tick() + MORSE_FLIPPER_SESSION_RESULT_MS;
    morse_flipper_update_sidetone(app);
}

void morse_flipper_feedback_pass(MorseFlipperApp* app) {
    if(app == NULL) return;
    if(app->notifications) notification_message(app->notifications, &morse_flipper_led_good_twice);
    app->session_result_tone = false;
    app->session_result_until = 0U;
    morse_flipper_update_sidetone(app);
}

void morse_flipper_feedback_fail(MorseFlipperApp* app) {
    morse_flipper_feedback_do(app, &morse_flipper_led_bad_twice);
}

void morse_flipper_feedback_timeout(MorseFlipperApp* app) {
    morse_flipper_feedback_do(app, &morse_flipper_led_timeout_twice);
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

uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app) {
    uint16_t dit;
    uint8_t wpm;

    if(app == NULL) return 0U;
    dit = app->straight_dit_ms ? app->straight_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    wpm = (uint8_t)((1200U + (dit / 2U)) / dit);
    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    return wpm;
}

void morse_flipper_set_straight_wpm(MorseFlipperApp* app, uint8_t wpm) {
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

static bool morse_flipper_audio_wait_transition(
    const MorseFlipperApp* app,
    uint8_t old_scene,
    uint8_t new_scene) {
    if(app == NULL || app->audio_path != MorseFlipperAudioPathGpioP2Hd || old_scene == new_scene)
        return false;
    return morse_flipper_scene_supports_audio_pwm(old_scene) ||
           morse_flipper_scene_supports_audio_pwm(new_scene);
}

uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app) {
    uint8_t pct = app ? app->p2_volume_pct : 100U;

    if(pct < 10U) pct = 10U;
    if(pct > 100U) pct = 100U;
    return pct;
}

bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenHamRun) return false;
    return app->audio_path == MorseFlipperAudioPathGpioP2Hd &&
           morse_flipper_scene_supports_audio_pwm(app->scene);
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

uint16_t morse_flipper_current_straight_dit_ms(const MorseFlipperApp* app) {
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

bool morse_flipper_straight_like_mode(const MorseFlipperApp* app) {
    uint8_t mode = morse_flipper_current_keyer_mode(app);

    return mode == MorseKeyerModePassthrough || mode == MorseKeyerModeStraight;
}

bool morse_flipper_session_running_view(const MorseFlipperApp* app);

typedef struct {
    bool live;
    bool btn;
    bool btn_str;
    bool btn_pad;
    bool back_key;
    bool back_exit;
    bool left_hint;
} MorseFlipperInputGate;

MorseFlipperInputGate morse_flipper_input_gate(const MorseFlipperApp* app) {
    MorseFlipperInputGate g = {0};

    if(app == NULL) return g;

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace) {
        g.live = true;
    } else if(app->screen == MorseFlipperScreenSession) {
        g.live = morse_flipper_session_repeat_active(app) ||
                 morse_flipper_session_running_view(app);
    } else if(app->screen == MorseFlipperScreenRf || app->screen == MorseFlipperScreenRfRx) {
        g.live = app->rf_live_active;
    } else if(app->screen == MorseFlipperScreenStraight) {
        g.live = app->straight_wait_answer;
    } else if(app->screen == MorseFlipperScreenTxGroups) {
        g.live = app->txg_wait_answer;
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

    if(app->screen == MorseFlipperScreenTxGroups && app->txg_sk) {
        g.btn_str = true;
        g.back_exit = true;
        return g;
    }

    if(morse_flipper_straight_like_mode(app))
        g.btn_str = true;
    else
        g.btn_pad = true;

    g.back_key = g.btn_pad;
    g.back_exit = !g.back_key;
    g.left_hint = g.back_key;
    return g;
}

bool morse_flipper_live_back_is_key(const MorseFlipperApp* app) {
    return morse_flipper_input_gate(app).back_key;
}

bool morse_flipper_live_left_hint(const MorseFlipperApp* app) {
    if(app != NULL && app->screen == MorseFlipperScreenHamRun) return true;
    return morse_flipper_input_gate(app).left_hint;
}

const char* morse_flipper_status_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
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

    snprintf(
        buf,
        buf_sz,
        "src %s straight",
        morse_flipper_gpio_name(morse_flipper_gpio_straight_idx(app)));
    return buf;
}

const char* morse_flipper_hand_name(const MorseFlipperApp* app) {
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

const char* morse_flipper_run_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
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

const char* morse_flipper_run_mode_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    const char* input;
    const char* keyer;

    if(app == NULL || buf == NULL || buf_sz == 0U) return "---";

    input = morse_flipper_run_input_name(app);
    keyer = morse_flipper_run_keyer_name(app);
    if(morse_flipper_straight_like_mode(app) || strcmp(keyer, "---") == 0) {
        snprintf(buf, buf_sz, "%u wpm  %s", (unsigned)morse_flipper_current_wpm(app), input);
    } else {
        snprintf(
            buf, buf_sz, "%u wpm  %s  %s", (unsigned)morse_flipper_current_wpm(app), input, keyer);
    }

    return buf;
}

const char* morse_flipper_run_input_name(const MorseFlipperApp* app) {
    if(app == NULL) return "---";
    if(app->input_source == MorseFlipperInputSourceButtons) return "buttons";
    if(app->input_source == MorseFlipperInputSourcePaddle) return "paddles";
    return "straight";
}

const char* morse_flipper_run_keyer_name(const MorseFlipperApp* app) {
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

const char* morse_flipper_run_usb_name(const MorseFlipperApp* app) {
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

const char* morse_flipper_trace_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(app->input_source == MorseFlipperInputSourceButtons) {
        snprintf(
            buf,
            buf_sz,
            "%s",
            morse_flipper_live_back_is_key(app) ? "key live hold L" : "OK str Bk back");
        return buf;
    }

    snprintf(buf, buf_sz, "gpio live  Bk back");
    return buf;
}

const char* morse_flipper_source_short_name(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
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

bool morse_flipper_session_left_exit_active(const MorseFlipperApp* app) {
    return morse_flipper_live_left_hint(app);
}

void morse_flipper_reset_run_state(MorseFlipperApp* app) {
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

void morse_flipper_sync_audio_output(MorseFlipperApp* app) {
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

bool morse_flipper_session_running_view(const MorseFlipperApp* app) {
    if(app == NULL || app->screen != MorseFlipperScreenSession) return false;
    return !morse_flipper_session_idle_view(app);
}

#include "morse_flipper_session.h"
#include "morse_flipper_help.c"

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

    if(screen == MorseFlipperScreenTxGroups && app->screen != MorseFlipperScreenTxGroups &&
       app->screen != MorseFlipperScreenTxGroupsResult &&
       app->screen != MorseFlipperScreenTxGroupsFinal) {
        morse_flipper_reset_tx_groups_state(app, now_ms);
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
    } else if(!morse_flipper_gpio_probe_keep_state(screen)) {
        morse_flipper_gpio_probe_reset(app);
    }
    if(old_screen == MorseFlipperScreenStraight || screen == MorseFlipperScreenStraight ||
       old_screen == MorseFlipperScreenTxGroups || screen == MorseFlipperScreenTxGroups) {
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

void morse_flipper_toggle_source(MorseFlipperApp* app) {
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

bool morse_flipper_training_playback_active(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenStraight) return app->straight_playback_active;
    return app->screen == MorseFlipperScreenSession && app->trainer_playback_active;
}

bool morse_flipper_straight_answer_down(const MorseFlipperApp* app) {
    if(app == NULL) return false;

    if(app->input_source == MorseFlipperInputSourceButtons) return false;
    if(morse_flipper_gpio_probe_blocks_start(app)) return false;
    if(app->input_source == MorseFlipperInputSourcePaddle)
        return morse_flipper_gpio_probe_use_straight(app) ?
                   !furi_hal_gpio_read(morse_flipper_dit_pin) :
                   (morse_flipper_logical_dit_down(app) || morse_flipper_logical_dah_down(app));
    return morse_flipper_straight_down();
}

void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms) {
    const char* group;
    uint16_t cw_code;
    uint32_t dit_ms;
    uint8_t marks;

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

    cw_code = cw(group[app->trainer_char_idx]);
    marks = cw_symbol_count(cw_code);
    dit_ms = morse_flipper_current_dit_ms(app);

    if(marks == 0U) {
        app->trainer_char_idx++;
        app->trainer_mark_idx = 0U;
        app->trainer_next_at = now_ms + morse_flipper_trainer_char_gap_ms(app);
        morse_flipper_view_dirty(app);
        return;
    }

    if(app->trainer_playback_mark) {
        app->trainer_playback_mark = false;
        if(app->trainer_mark_idx + 1U < marks) {
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

    if(app->trainer_mark_idx >= marks) {
        app->trainer_playback_active = false;
        app->trainer_next_at = 0U;
        morse_flipper_update_sidetone(app);
        return;
    }

    app->trainer_playback_mark = true;
    app->trainer_next_at = now_ms + (dit_ms * cw_symbol_units(cw_code, app->trainer_mark_idx));
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir) {
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

void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms) {
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

void morse_flipper_cycle_mode(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    app->keyer_mode = morse_keyer_next_ui_mode(app->keyer_mode);
    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_view_dirty(app);
}

void morse_flipper_toggle_handedness(MorseFlipperApp* app) {
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
