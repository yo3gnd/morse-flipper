static void morse_flipper_gpio_init(MorseFlipperApp* app) {
    morse_flipper_gpio_apply(app);
}

static void morse_flipper_gpio_deinit(void) {
    morse_flipper_gpio_reset_candidates();
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

static void morse_flipper_decoder_drain_into( MorseFlipperCwDecoder* decoder, char* out, size_t out_sz) {
    if(decoder == NULL || out == NULL || out_sz < 2U) return;

    if(morse_flipper_cw_decoder_output(decoder)[0] != '\0') {
        morse_flipper_append_text(out, out_sz, morse_flipper_cw_decoder_output(decoder));
        morse_flipper_cw_decoder_clear_output(decoder);
    }
}

static void morse_flipper_drain_tx_decoder(MorseFlipperApp* app) {
    const char* out;

    if(app == NULL) return;
    out = morse_flipper_cw_decoder_output(&app->tx_decoder);
    if(out[0] == '\0') return;

    morse_flipper_append_text(app->rf_tx_text, sizeof(app->rf_tx_text), out);
    if(app->screen == MorseFlipperScreenHamRun) {
        morse_flipper_ham_log_append_text(app, out, furi_get_tick());
    }
    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenRf ||
       app->screen == MorseFlipperScreenHamRun) {
        morse_flipper_run_history_append(&app->run_history, out);
        morse_flipper_view_dirty(app);
    }
    morse_flipper_cw_decoder_clear_output(&app->tx_decoder);
}

static char morse_flipper_tx_symbol(const MorseFlipperApp* app) {
    if(app->note_sources[1] != 0U) return '.';
    if(app->note_sources[2] != 0U) return '-';
    if(app->note_sources[0] != 0U) return 'K';
    return '?';
}

static bool morse_flipper_tx_decoder_allowed(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->trainer_playback_active || app->straight_playback_active) return false;
    if(app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app)) return false;
    return true;
}

static void morse_flipper_feed_tx_edge(MorseFlipperApp* app, bool level, uint32_t now_ms) {
    uint32_t dt;

    if(app == NULL) return;
    if(level == app->rf_tx_level) return;

    if(app->rf_tx_edge_at != 0U) {
        dt = now_ms - app->rf_tx_edge_at;
        if(dt != 0U && morse_flipper_tx_decoder_allowed(app)) {
            if(app->rf_tx_level) {
                morse_flipper_cw_decoder_feed_mark(&app->tx_decoder, (uint16_t)dt);
            } else {
                morse_flipper_cw_decoder_feed_space(&app->tx_decoder, (uint16_t)dt);
            }
            morse_flipper_drain_tx_decoder(app);
        }
    }

    app->rf_tx_level = level;
    app->rf_tx_edge_at = now_ms;
    app->rf_tx_gap_flushed = level;
    morse_flipper_rf_handle_tx(&app->rf, level, morse_flipper_tx_symbol(app));
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

static void morse_flipper_feed_straight_mark(MorseFlipperApp* app, uint16_t mark_ms, uint32_t now_ms) {
    char elem;
    uint16_t gap_ms = 0U;

    if(app == NULL || mark_ms == 0U) return;

    if(app->straight_mark_started_at != 0U && app->straight_last_input_at != 0U &&
       app->straight_mark_started_at > app->straight_last_input_at)
        gap_ms = (uint16_t)(app->straight_mark_started_at - app->straight_last_input_at);

    elem = mark_ms >= (morse_flipper_current_straight_dit_ms(app) * 2U) ? '-' : '.';
    morse_flipper_straight_trainer_feed(&app->straight_trainer, elem, mark_ms, gap_ms);
    app->straight_last_input_at = now_ms;
}

static uint32_t morse_flipper_straight_answer_settle_ms(const MorseFlipperApp* app)
{
    uint32_t dit_ms;
    uint8_t target_symbols;
    uint8_t answer_symbols;

    if(app == NULL) return 0U;

    dit_ms = morse_flipper_current_straight_dit_ms(app);
    target_symbols = morse_flipper_straight_trainer_target_symbol_count(&app->straight_trainer);
    answer_symbols = morse_flipper_straight_trainer_answer_symbol_count(&app->straight_trainer);

    if(answer_symbols == 0U) return 0U;
    if(app->input_source == MorseFlipperInputSourceButtons) {
        if(answer_symbols >= target_symbols) return dit_ms * 3U;
        return dit_ms * 4U;
    }
    if(answer_symbols >= target_symbols) return dit_ms * 2U;
    return dit_ms * 3U;
}

static bool morse_flipper_live_straight_active( MorseFlipperApp* app, bool raw_down, uint32_t now_ms)
{
    if(app == NULL) return raw_down;
    return morse_flipper_straight_filter_update( &app->straight_filter, raw_down, now_ms, MORSE_FLIPPER_STRAIGHT_RELEASE_DEBOUNCE_MS);
}


static void morse_flipper_sync_gpio_inputs(MorseFlipperApp* app, uint32_t now_ms) {
    bool straight_active = false;
    bool dit_active = false;
    bool dah_active = false;

    if(app->input_source == MorseFlipperInputSourceStraight) {
        straight_active = morse_flipper_live_straight_active(app, morse_flipper_straight_down(), now_ms);
    } else if(app->input_source == MorseFlipperInputSourcePaddle) {
        if(morse_flipper_gpio_probe_use_straight(app)) {
            straight_active = morse_flipper_live_straight_active( app, !furi_hal_gpio_read(morse_flipper_dit_pin), now_ms);
        } else if(!morse_flipper_gpio_probe_blocks_start(app)) {
            morse_flipper_straight_filter_reset(&app->straight_filter);
            dit_active = morse_flipper_logical_dit_down(app);
            dah_active = morse_flipper_logical_dah_down(app);
        }
    } else {
        morse_flipper_straight_filter_reset(&app->straight_filter);
    }

    if(morse_flipper_training_playback_active(app) || app->screen == MorseFlipperScreenSessionEnd ||
       app->screen == MorseFlipperScreenRfRx ||
       (app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app))) {
        morse_flipper_straight_filter_reset(&app->straight_filter);
        straight_active = false;
        dit_active = false;
        dah_active = false;
    }

    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, straight_active);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_GPIO_DIT, dit_active, now_ms);
    morse_flipper_set_paddle_source( app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_GPIO_DAH, dah_active, now_ms);
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
    uint8_t straight_idx = morse_flipper_gpio_straight_idx(app);
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
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(straight_idx));
    }
    if((app->input_mask & (1U << 4)) && app->gpio_dah_idx != straight_idx) {
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(app->gpio_dah_idx));
    }
    if((app->input_mask & (1U << 5)) && app->gpio_dit_idx != straight_idx &&
       app->gpio_dit_idx != app->gpio_dah_idx) {
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(app->gpio_dit_idx));
    }

    if(n == 4U) {
        snprintf(buf, buf_sz, "raw -");
    }

    return buf;
}

static void morse_flipper_poll(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();
    bool old_tone = app->tone_on;
    bool old_busy = app->speaker_busy;
    uint8_t old_mask = app->input_mask;
    bool old_transport = app->transport_connected;
    uint8_t old_session_flash = app->session_end_flash_phase;
    uint32_t pwm_tone_hz;
    bool raw_straight;
    bool tx_now;

    if(app->pc_mode == MorseFlipperPcModeMidi && app->midi_rx_pending) {
        app->midi_rx_pending = false;
        morse_flipper_handle_midi_rx(app);
    }

    if(morse_flipper_use_pwm_buzzer(app)) {
        pwm_tone_hz = (uint32_t)(morse_flipper_active_tone_hz(app) + 0.5f);
        if(app->audio_pwm.tone_hz != pwm_tone_hz)
            morse_flipper_audio_pwm_set_tone_hz(&app->audio_pwm, pwm_tone_hz);
    }

    if(app->screen == MorseFlipperScreenSession && app->session_started &&
       morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat) {
        morse_trainer_tick( &app->trainer, MORSE_FLIPPER_POLL_MS, (uint32_t)app->trainer_answer_timeout_s * 1000U);
    }

    morse_flipper_tick_session(app, now_ms);
    morse_flipper_tick_straight(app, now_ms);
    morse_flipper_tick_ham_macro(app, now_ms);
    morse_flipper_gpio_probe_tick(app, now_ms);

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
    if(app->screen == MorseFlipperScreenSession && !app->session_started && !app->trainer_playback_active &&
       app->input_source != MorseFlipperInputSourceButtons && morse_flipper_session_wait_key_down(app)) {
        morse_flipper_start_session(app, now_ms);
        raw_straight = false;
    }
    if(app->screen == MorseFlipperScreenStraight && !app->straight_started && !app->straight_playback_active &&
       !app->straight_wait_answer && !app->straight_done && raw_straight) {
        morse_flipper_start_straight_round(app, now_ms);
        raw_straight = false;
    }
    if(app->screen == MorseFlipperScreenStraight && !raw_straight &&
       app->straight_mark_started_at != 0U && app->straight_key_down) {
        app->straight_key_down = false;
        morse_flipper_feed_straight_mark(app, (uint16_t)(now_ms - app->straight_mark_started_at), now_ms);
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
            if(morse_flipper_tx_decoder_allowed(app)) {
                morse_flipper_cw_decoder_feed_space(&app->tx_decoder, (uint16_t)gap);
                morse_flipper_drain_tx_decoder(app);
            }
            app->rf_tx_gap_flushed = true;
        }
    }
    morse_flipper_ham_log_flush_if_idle(app, now_ms);

    morse_flipper_tick_live_rf(app, now_ms);
#if MORSE_FLIPPER_RF_LIVE_DECODERS
    morse_flipper_radio_drain_rx(&app->radio);
#endif
    if(app->screen == MorseFlipperScreenSessionEnd && morse_flipper_session_end_flash(app))
        app->session_end_flash_phase = (uint8_t)((now_ms / 250U) & 1U);
    else
        app->session_end_flash_phase = 0U;
    morse_flipper_update_sidetone(app);
    morse_flipper_sync_ptt(app, now_ms);
    morse_flipper_sync_backlight(app, now_ms);

    if(old_tone != app->tone_on || old_busy != app->speaker_busy || old_mask != app->input_mask ||
       old_transport != app->transport_connected || old_session_flash != app->session_end_flash_phase) {
        morse_flipper_view_dirty(app);
    }
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
