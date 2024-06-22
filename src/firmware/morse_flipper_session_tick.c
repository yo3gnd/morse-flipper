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
    app->session_wait_draw_s = 0xFFU;
}

static void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->trainer_playback_active = false;
    app->trainer_playback_mark = false;
    app->session_log_pending = false;

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    morse_flipper_reset_session_runtime(app);
    morse_trainer_reset_session(&app->trainer);
    app->rf_tx_text[0] = '\0';
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));

    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_update_sidetone(app);
}

static bool morse_flipper_session_repeat_active(const MorseFlipperApp* app) {
    return app && app->screen == MorseFlipperScreenSession && app->session_started &&
           !app->trainer_playback_active && app->session_next_group_at == 0U &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat;
}

static bool morse_flipper_session_idle_view(const MorseFlipperApp* app) {
    MorseTrainerPhase ph;

    if(!app || app->screen != MorseFlipperScreenSession) return false;

    ph = morse_trainer_phase(&app->trainer);
    return !app->session_started || (!app->trainer_playback_active && !app->session_round_pending &&
           !app->session_result_hold && !app->session_result_tone &&
           app->session_next_group_at == 0U && !morse_trainer_session_active(&app->trainer) &&
           ph != MorseTrainerPhaseRepeat);
}

static bool morse_flipper_session_live_keying(const MorseFlipperApp* app) {
    return app &&
           (app->paddle_sources[MorseKeyerPaddleDit] != 0U ||
            app->paddle_sources[MorseKeyerPaddleDah] != 0U ||
            app->note_sources[0] != 0U || app->note_sources[1] != 0U ||
            app->note_sources[2] != 0U);
}

static bool morse_flipper_session_wait_key_down(const MorseFlipperApp* app) {
    if(app == NULL) return false;

    if(app->input_source == MorseFlipperInputSourceButtons) {
        if(morse_flipper_straight_like_mode(app)) return app->ok_down;
        return app->ok_down || app->back_down;
    }

    if(app->input_source == MorseFlipperInputSourceStraight) return morse_flipper_straight_down();

    return morse_flipper_logical_dit_down(app) || morse_flipper_logical_dah_down(app);
}

static void morse_flipper_queue_session_feedback(MorseFlipperApp* app, uint32_t now_ms) {
    bool missed;
    const NotificationSequence* seq;

    if(app == NULL || !app->session_round_pending) return;

    app->session_round_pending = false;
    app->session_result_hold = true;
    app->session_result_good = !morse_trainer_last_failed(&app->trainer);
    missed = morse_trainer_answer(&app->trainer)[0] == '\0';
    app->session_result_tone = !app->session_result_good;
    app->session_result_until = now_ms + MORSE_FLIPPER_SESSION_RESULT_MS;
    app->session_next_group_at =
        morse_trainer_session_active(&app->trainer) ?
            (now_ms + ((uint32_t)app->trainer_group_pause_s * 1000U)) :
            0U;
    app->session_wait_draw_s = 0xFFU;
    seq = &morse_flipper_led_good_twice;
    if(!app->session_result_good) {
        if(missed) seq = &morse_flipper_led_miss_twice;
        else seq = &morse_flipper_led_bad_twice;
    }
    if(app->notifications) notification_message(app->notifications, seq);
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
    app->rf_tx_text[0] = '\0';
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    if(app->screen == MorseFlipperScreenSession) {
        app->session_round_pending = true;
        app->session_result_hold = false;
        app->session_result_tone = false;
        app->session_result_good = false;
        app->session_last_input_at = now_ms;
        app->session_result_until = 0U;
        app->session_next_group_at = 0U;
        app->session_wait_draw_s = 0xFFU;
    }
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

static void morse_flipper_tick_session(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t dt;
    uint32_t left_ms;
    uint8_t left_s;

    if(app == NULL || app->screen != MorseFlipperScreenSession || !app->session_started) return;

    if(app->session_result_tone && now_ms >= app->session_result_until) app->session_result_tone = false;

    if(app->session_result_hold && app->session_next_group_at > now_ms) {
        if(morse_flipper_session_wait_key_down(app)) {
            if(app->session_next_group_at - now_ms > 1000U) {
                app->session_next_group_at = now_ms + 1000U;
                app->session_wait_draw_s = 0xFFU;
                morse_flipper_view_dirty(app);
            }
        }

        left_ms = app->session_next_group_at - now_ms;
        left_s = (uint8_t)((left_ms + 999U) / 1000U);
        if(left_s == 0U) left_s = 1U;
        if(left_s != app->session_wait_draw_s) {
            app->session_wait_draw_s = left_s;
            morse_flipper_view_dirty(app);
        }
    }

    if(app->session_next_group_at != 0U && now_ms >= app->session_next_group_at) {
        app->session_next_group_at = 0U;
        app->session_wait_draw_s = 0xFFU;
        if(morse_trainer_next_session_group(&app->trainer)) {
            morse_flipper_begin_group_playback(app, now_ms);
        } else {
            morse_flipper_view_dirty(app);
        }
        return;
    }

    if(!app->session_round_pending || app->trainer_playback_active) return;

    if(morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone) {
        morse_flipper_queue_session_feedback(app, now_ms);
        return;
    }

    if(!morse_flipper_session_repeat_active(app) || morse_trainer_answer(&app->trainer)[0] == '\0')
        return;

    if(morse_flipper_session_live_keying(app)) {
        app->session_last_input_at = now_ms;
        return;
    }

    dt = morse_flipper_current_dit_ms(app) * 8U;
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
