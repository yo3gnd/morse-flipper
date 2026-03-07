static const MorseFlipperTone* morse_flipper_current_tone(const MorseFlipperApp* app)
{
    if(app == NULL) return &morse_flipper_tones[MORSE_FLIPPER_DEFAULT_TONE_IDX];

    if(app->vail_tone_active && app->vail_tone_idx < COUNT_OF(morse_flipper_tones))
        return &morse_flipper_tones[app->vail_tone_idx];

    if(app->tone_idx >= COUNT_OF(morse_flipper_tones))
        return &morse_flipper_tones[MORSE_FLIPPER_DEFAULT_TONE_IDX];

    return &morse_flipper_tones[app->tone_idx];
}

static const char* morse_flipper_current_tone_name(const MorseFlipperApp* app)
{
    if(app != NULL && app->tone_idx == MORSE_FLIPPER_TONE_OFF_IDX) return "Off";
    return morse_flipper_current_tone(app)->name;
}

static const MorseFlipperTone* morse_flipper_current_audible_tone(const MorseFlipperApp* app)
{
    const MorseFlipperTone* tone = morse_flipper_current_tone(app);

    if(tone->hz > 0.0f) return tone;
    return &morse_flipper_tones[MORSE_FLIPPER_DEFAULT_TONE_IDX];
}

static float morse_flipper_active_tone_hz(const MorseFlipperApp* app)
{
    float hz = morse_flipper_current_audible_tone(app)->hz;

    if(app != NULL && app->vail_tone_active && app->vail_tone_idx < COUNT_OF(morse_flipper_tones))
        hz = morse_flipper_current_audible_tone(app)->hz;

    return hz;
}

static bool morse_flipper_local_buzzer_enabled(const MorseFlipperApp* app)
{
    if(app == NULL) return false;
    return app->tone_idx != MORSE_FLIPPER_TONE_OFF_IDX;
}

static bool morse_flipper_use_pwm_buzzer(const MorseFlipperApp* app)
{
    return morse_flipper_audio_output_is_pwm(app) && app->audio_pwm.running;
}

static bool morse_flipper_any_active_notes(const MorseFlipperApp* app)
{
    if(app == NULL) return false;
    return app->note_sources[0] != 0U || app->note_sources[1] != 0U || app->note_sources[2] != 0U;
}

static void morse_flipper_tone_stop(MorseFlipperApp* app)
{
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    app->tone_on = false;
    app->speaker_owned = false;
    app->speaker_busy = false;
    app->speaker_hz = 0.0f;
}

static void morse_flipper_tone_start(MorseFlipperApp* app)
{
    float hz;

    if(app->tone_on) return;

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

static void morse_flipper_update_sidetone(MorseFlipperApp* app)
{
    bool use_pwm;
    bool want_tx_tone = morse_flipper_any_active_notes(app) || (app->preview_ticks > 0U);
    bool want_aux_tone = app->trainer_playback_mark || app->straight_playback_mark ||
                         app->session_result_tone || app->rf_monitor_tone;
    bool want_speaker;

    use_pwm = morse_flipper_use_pwm_buzzer(app);
    want_speaker = !use_pwm && (want_aux_tone ||
                   (want_tx_tone && morse_flipper_local_buzzer_enabled(app)));

    if(use_pwm) {
        if(app->speaker_owned || app->tone_on) {
            morse_flipper_tone_stop(app);
        }

        morse_flipper_audio_pwm_set_gate(&app->audio_pwm, want_tx_tone || want_aux_tone);
        app->tone_on =
            want_tx_tone || want_aux_tone || morse_flipper_audio_pwm_sound_active(&app->audio_pwm);
        app->speaker_busy = false;
        morse_flipper_sync_ptt(app, furi_get_tick());
        return;
    }

    if(want_speaker) {
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

    morse_flipper_sync_ptt(app, furi_get_tick());
}

static uint32_t morse_flipper_note_source_for_paddle(uint8_t paddle)
{
    return (paddle == MorseKeyerPaddleDit) ? MORSE_SOURCE_KEYER_DIT : MORSE_SOURCE_KEYER_DAH;
}

static uint8_t morse_flipper_note_for_paddle(uint8_t paddle)
{
    return (paddle == MorseKeyerPaddleDit) ? 1U : 2U;
}

static void morse_flipper_set_note_source( MorseFlipperApp* app, uint8_t note, uint32_t source_mask, bool active)
{
    uint32_t now_ms;

    if(note >= COUNT_OF(app->note_sources)) return;

    uint32_t before = app->note_sources[note];
    uint32_t after = active ? (before | source_mask) : (before & ~source_mask);

    if(before == after) return;

    app->note_sources[note] = after;
    morse_flipper_update_sidetone(app);
    if(app->screen == MorseFlipperScreenRf && app->rf_live_active) {
        now_ms = furi_get_tick();
        app->rf_tx_tail_until =
            now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_TX_TAIL_DITS);

        if(morse_flipper_any_active_notes(app) && !app->radio.tx_on) {
            morse_flipper_radio_sync_live(
                &app->radio,
                morse_flipper_rf_frequency_hz(&app->rf),
                true,
                true,
                MorseFlipperRadioProfileOokData);
        }
        morse_flipper_radio_set_tx_level(&app->radio, morse_flipper_any_active_notes(app));
    }

    if(before == 0U && after != 0U) {
        morse_flipper_send_transport_note(app, note, true);
    } else if(before != 0U && after == 0U) {
        morse_flipper_send_transport_note(app, note, false);
    }
}

static void morse_flipper_release_all_notes(MorseFlipperApp* app)
{
    uint32_t note_sources[COUNT_OF(app->note_sources)];

    for(size_t note = 0; note < COUNT_OF(app->note_sources); note++) {
        note_sources[note] = app->note_sources[note];
        app->note_sources[note] = 0U;
    }

    morse_flipper_update_sidetone(app);

    for(size_t note = 0; note < COUNT_OF(app->note_sources); note++) {
        if(note_sources[note] != 0U) morse_flipper_send_transport_note(app, (uint8_t)note, false);
    }

    if(app->screen == MorseFlipperScreenRf && app->rf_live_active) {
        uint32_t now_ms = furi_get_tick();
        app->rf_tx_tail_until =
            now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_TX_TAIL_DITS);
        morse_flipper_radio_set_tx_level(&app->radio, false);
    }
}

static void morse_flipper_drain_keyer_events(MorseFlipperApp* app)
{
    MorseKeyerEvent event;

    while(morse_keyer_pop_event(&app->keyer, &event)) {
        if((app->screen == MorseFlipperScreenTrainer ||
            app->screen == MorseFlipperScreenSession) &&
           event.type == MorseKeyerEventPress &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat) {
            morse_trainer_feed_element( &app->trainer, event.paddle == MorseKeyerPaddleDit ? '.' : '-');
        }

        if(app->screen == MorseFlipperScreenSession &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat)
            app->session_last_input_at = furi_get_tick();

        morse_flipper_set_note_source(
            app,
            morse_flipper_note_for_paddle(event.paddle),
            morse_flipper_note_source_for_paddle(event.paddle),
            event.type == MorseKeyerEventPress);
    }
}

static void morse_flipper_set_paddle_source( MorseFlipperApp* app, uint8_t paddle, uint32_t source_mask, bool active, uint32_t now_ms)
{
    if(paddle >= MorseKeyerPaddleCount) return;

    uint32_t before = app->paddle_sources[paddle];
    uint32_t after = active ? (before | source_mask) : (before & ~source_mask);

    if(before == after) return;

    app->paddle_sources[paddle] = after;
    morse_keyer_paddle_event(&app->keyer, paddle, after != 0U, now_ms);
    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);
}

static void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms)
{
    morse_keyer_reset(&app->keyer);
    morse_flipper_drain_keyer_events(app);
    morse_keyer_set_mode(&app->keyer, morse_flipper_current_keyer_mode(app));
    morse_keyer_set_dit_duration(&app->keyer, morse_flipper_current_dit_ms(app));

    for(uint8_t paddle = 0; paddle < MorseKeyerPaddleCount; paddle++) {
        if(app->paddle_sources[paddle] != 0U) morse_keyer_paddle_event(&app->keyer, paddle, true, now_ms);
    }

    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);
}

static void morse_flipper_clear_button_paddles(MorseFlipperApp* app, uint32_t now_ms)
{
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
}

static void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms)
{
    uint8_t ok_paddle = morse_flipper_ok_button_paddle(app);
    uint8_t back_paddle = morse_flipper_back_button_paddle(app);

    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDit,
        MORSE_PADDLE_SOURCE_BTN_OK,
        app->ok_down && ok_paddle == MorseKeyerPaddleDit,
        now_ms);
    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDah,
        MORSE_PADDLE_SOURCE_BTN_OK,
        app->ok_down && ok_paddle == MorseKeyerPaddleDah,
        now_ms);
    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDit,
        MORSE_PADDLE_SOURCE_BTN_BACK,
        app->back_down && back_paddle == MorseKeyerPaddleDit,
        now_ms);
    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDah,
        MORSE_PADDLE_SOURCE_BTN_BACK,
        app->back_down && back_paddle == MorseKeyerPaddleDah,
        now_ms);
}

static void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now_ms)
{
    app->left_down = false;
    app->ok_down = false;
    app->back_down = false;
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_clear_button_paddles(app, now_ms);
}
