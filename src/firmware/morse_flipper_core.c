#include "morse_flipper_app_i.h"
#include "cw.h"

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

uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app) {
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
