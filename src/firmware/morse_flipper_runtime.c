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

static void morse_flipper_dec_drain( MorseFlipperCwDecoder* decoder, char* out, size_t out_sz) {
    if(decoder == NULL || out == NULL || out_sz < 2U) return;

    if(morse_flipper_cw_decoder_output(decoder)[0] != '\0') {
        morse_flipper_append_text(out, out_sz, morse_flipper_cw_decoder_output(decoder));
        morse_flipper_cw_decoder_clear_output(decoder);
    }
}

static char morse_flipper_tx_symbol(const MorseFlipperApp* app) {
    if(app->note_src[1] != 0U) return '.';
    if(app->note_src[2] != 0U) return '-';
    if(app->note_src[0] != 0U) return 'K';
    return '?';
}

static bool morse_flipper_tx_decoder_allowed(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->trainer_playback_active || app->straight_playback_active) return false;
    if(app->screen == MorseFlipperScreenSession && !morse_flipper_session_repeat_active(app)) return false;
    if(app->screen == MorseFlipperScreenRf && !MORSE_FLIPPER_RF_LIVE_DECODERS) return false;
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
            morse_flipper_dec_drain(&app->tx_decoder, app->rf_tx_text, sizeof(app->rf_tx_text));
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
            morse_flipper_dec_drain(&app->gpio_decoder, app->gpio_text, sizeof(app->gpio_text));
        }
    }

    app->gpio_level = level;
    app->gpio_edge_at = now_ms;
    app->gpio_gap_flushed = level;
}

static void morse_flipper_feed_sk_mark(MorseFlipperApp* app, uint16_t mark_ms) {
    char elem;

    if(app == NULL || mark_ms == 0U) return;

    elem = mark_ms >= (morse_flipper_current_dit_ms(app) * 2U) ? '-' : '.';
    morse_flipper_straight_trainer_feed(&app->straight_trainer, elem, mark_ms);
    app->straight_last_input_at = furi_get_tick();

    if(strlen(morse_flipper_straight_trainer_answer(&app->straight_trainer)) >=
       strlen(morse_flipper_straight_trainer_target_morse(&app->straight_trainer))) {
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
    }
}


static void morse_flipper_drain_keyer_events(MorseFlipperApp* app) {
    MorseKeyerEvent event;

    while(morse_keyer_pop_event(&app->keyer, &event)) {
        if((app->screen == MorseFlipperScreenTrainer ||
            app->screen == MorseFlipperScreenSession) &&
           event.type == MorseKeyerEventPress &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat) {
            morse_trainer_feed_element( &app->trainer, event.paddle == MorseKeyerPaddleDit ? '.' : '-');
        }

        if(app->screen == MorseFlipperScreenSession &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat) {
            app->session_last_input_at = furi_get_tick();
        }

        morse_flipper_set_note_source(
            app,
            morse_flipper_note_for_paddle(event.paddle),
            morse_flipper_note_source_for_paddle(event.paddle),
            event.type == MorseKeyerEventPress);
    }
}

static void morse_flipper_set_paddle_source( MorseFlipperApp* app, uint8_t paddle, uint32_t source_mask, bool active, uint32_t now_ms) {
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

static void morse_flipper_key_evt( MorseFlipperApp* app, const InputEvent* event) {
    uint32_t now_ms = furi_get_tick();
    MorseFlipperInputGate g = morse_flipper_input_gate(app);
    bool btn_src = g.btn;
    bool btn_str = g.btn_str;
    bool btn_pad = g.btn_pad;

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

    if(g.back_exit && event->key == InputKeyBack &&
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

static void morse_flipper_sync_gpio_inputs(MorseFlipperApp* app, uint32_t now_ms) {
    bool straight_active = false;
    bool dit_active = false;
    bool dah_active = false;

    if(app->in_src == MorseFlipperInputSourceStraight) {
        straight_active = morse_flipper_straight_down();
    } else if(app->in_src == MorseFlipperInputSourcePaddle) {
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
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(app->gpio_straight_idx));
    }
    if((app->input_mask & (1U << 4)) && app->gpio_dah_idx != app->gpio_straight_idx) {
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(app->gpio_dah_idx));
    }
    if((app->input_mask & (1U << 5)) && app->gpio_dit_idx != app->gpio_straight_idx &&
       app->gpio_dit_idx != app->gpio_dah_idx) {
        n += snprintf(buf + n, buf_sz - n, "%s ", morse_flipper_gpio_name(app->gpio_dit_idx));
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
    app->prev_n = MORSE_FLIPPER_PREVIEW_TICKS;

    if(app->tone_on && app->sp_owned && furi_hal_speaker_is_mine()) {
        furi_hal_speaker_start(morse_flipper_current_tone(app)->hz, MORSE_FLIPPER_VOLUME);
    }

    morse_flipper_save_config(app);
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_poll(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();
    bool old_tone = app->tone_on;
    bool old_busy = app->sp_busy;
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
       morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone) {
        if(morse_trainer_session_completed(&app->trainer)) {
            morse_trainer_append_session_log(&app->trainer);
            morse_trainer_load_session_lines(&app->session_lines);
            app->session_line_idx =
                app->session_lines.count == 0U ? 0U : (app->session_lines.count - 1U);
        }
        app->session_log_pending = false;
    }

    if(app->screen == MorseFlipperScreenSession && app->session_started &&
       morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat) {
        morse_trainer_tick( &app->trainer, MORSE_FLIPPER_POLL_MS, (uint32_t)app->trainer_to_s * 1000U);
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
            morse_flipper_dec_drain(&app->gpio_decoder, app->gpio_text, sizeof(app->gpio_text));
            app->gpio_gap_flushed = true;
        }
    }

    raw_straight = morse_flipper_straight_answer_down(app);
    if(app->screen == MorseFlipperScreenStraight && !raw_straight &&
       app->straight_mark_started_at != 0U && app->sk_down) {
        app->sk_down = false;
        morse_flipper_feed_sk_mark(app, (uint16_t)(now_ms - app->straight_mark_started_at));
        app->straight_mark_started_at = 0U;
    } else if(app->screen == MorseFlipperScreenStraight && raw_straight &&
              !app->sk_down && app->sk_wait) {
        app->sk_down = true;
        app->straight_mark_started_at = now_ms;
    }

    tx_now = morse_flipper_any_active_notes(app);
    morse_flipper_feed_tx_edge(app, tx_now, now_ms);
    if(!app->rf_tx_level && !app->rf_tx_gap_flushed && app->rf_tx_edge_at != 0U) {
        uint32_t gap = now_ms - app->rf_tx_edge_at;
        if(gap >= (morse_flipper_current_dit_ms(app) * 5U) / 2U) {
            if(morse_flipper_tx_decoder_allowed(app)) {
                morse_flipper_cw_decoder_feed_space(&app->tx_decoder, (uint16_t)gap);
                morse_flipper_dec_drain(&app->tx_decoder, app->rf_tx_text, sizeof(app->rf_tx_text));
            }
            app->rf_tx_gap_flushed = true;
        }
    }

    morse_flipper_tick_live_rf(app, now_ms);
#if MORSE_FLIPPER_RF_LIVE_DECODERS
    morse_flipper_radio_drain_rx(&app->radio);
#endif
    morse_flipper_update_sidetone(app);
    morse_flipper_sync_backlight(app, now_ms);

    if(old_tone != app->tone_on || old_busy != app->sp_busy || old_mask != app->input_mask ||
       old_transport != app->transport_connected) {
        morse_flipper_view_dirty(app);
    }
}


static bool morse_flipper_live_input(InputEvent* event, void* ctx) {
    MorseFlipperApp* app = ctx;
    uint32_t now_ms = furi_get_tick();

    if(morse_flipper_input_chunk_a(app, event)) return true;
    if(morse_flipper_input_chunk_b(app, event, now_ms)) return true;
    return false;
}

static bool morse_flipper_custom_evt(void* context, uint32_t event) {
    MorseFlipperApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool morse_flipper_back_evt(void* context) {
    MorseFlipperApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void morse_flipper_tick_callback(void* context) {
    MorseFlipperApp* app = context;

    morse_flipper_poll(app);
    morse_flipper_tick_trainer_playback(app, furi_get_tick());

    if(app->prev_n > 0U) {
        app->prev_n--;
        morse_flipper_update_sidetone(app);
    }

    scene_manager_handle_tick_event(app->scene_manager);
}
