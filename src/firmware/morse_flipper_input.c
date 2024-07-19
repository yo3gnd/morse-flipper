static bool morse_flipper_help_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenHelp) return false;

    if(event->key == InputKeyLeft && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->help_topic = app->help_topic == 0U ? (MorseFlipperHelpCount - 1U) : app->help_topic - 1U;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyRight && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            app->help_topic = (app->help_topic + 1U) % MorseFlipperHelpCount;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyBack && (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_about_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenAbout) return false;

    if(event->key == InputKeyBack && (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_pc_keys_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenPcKeys) return false;

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

static bool morse_flipper_pc_input(MorseFlipperApp* app, InputEvent* event)
{
    if(app->screen != MorseFlipperScreenPc) return false;

    if(event->key == InputKeyLeft || event->key == InputKeyOk || event->key == InputKeyBack) {
        morse_flipper_key_evt(app, event);
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

static bool morse_flipper_input_chunk_a(MorseFlipperApp* app, InputEvent* event)
{
    if(morse_flipper_help_input(app, event)) return true;
    if(morse_flipper_about_input(app, event)) return true;
    if(morse_flipper_pc_keys_input(app, event)) return true;
    if(morse_flipper_pc_input(app, event)) return true;
    return false;
}

static bool morse_flipper_trainer_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenTrainer) return false;

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
        morse_flipper_cycle_train(app, -1);
        return true;
    }

    if(event->key == InputKeyRight &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        morse_flipper_cycle_train(app, 1);
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

static bool morse_flipper_straight_input( MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms)
{
    if(app->screen != MorseFlipperScreenStraight) return false;

    if(morse_flipper_live_back_exits(app) && event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(app->sk_wait && app->in_src == MorseFlipperInputSourceButtons &&
       event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            app->ok_down = true;
            app->sk_down = true;
            app->straight_mark_started_at = now_ms;
            morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, true);
            morse_flipper_view_dirty(app);
        } else if(event->type == InputTypeRelease) {
            uint16_t dt = 0U;

            app->ok_down = false;
            morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
            if(app->sk_down && app->straight_mark_started_at != 0U)
                dt = (uint16_t)(now_ms - app->straight_mark_started_at);
            app->sk_down = false;
            app->straight_mark_started_at = 0U;
            if(dt != 0U) {
                morse_flipper_feed_sk_mark(app, dt);
                morse_flipper_view_dirty(app);
            }
        }
        return true;
    }

    if(!app->straight_playback_active && !app->sk_wait && event->key == InputKeyOk &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_start_straight_round(app, now_ms);
        return true;
    }

    return false;
}

static bool morse_flipper_session_input( MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms)
{
    MorseFlipperInputGate g;

    if(app->screen != MorseFlipperScreenSession) return false;
    g = morse_flipper_input_gate(app);

    if(morse_flipper_session_repeat_active(app)) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) {
            morse_flipper_key_evt(app, event);
            return true;
        }

        if(event->key == InputKeyOk && g.btn) {
            morse_flipper_key_evt(app, event);
            return true;
        }

        if(event->key == InputKeyBack && g.back_key) {
            morse_flipper_key_evt(app, event);
            return true;
        }

        if(g.back_exit && event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        return false;
    }

    if(morse_flipper_session_running_view(app)) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) {
            morse_flipper_key_evt(app, event);
            return true;
        }

        if(event->key == InputKeyBack && g.back_key) {
            morse_flipper_key_evt(app, event);
            return true;
        }

        if(g.back_exit && event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyOk && g.btn) return true;
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

static bool morse_flipper_browse_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenBrowse) return false;

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

static bool morse_flipper_rf_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenRf) return false;

    if(app->rf_man) {
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
            app->rf_man = false;
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            app->rf_man = false;
            morse_flipper_view_dirty(app);
            return true;
        }

        return false;
    }

    if(app->rf_live) {
        morse_flipper_key_evt(app, event);
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
        app->rf_live = true;
        app->rf_tail_at = 0U;
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
        app->rf_man = true;
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

static bool morse_flipper_run_trace_home_input(MorseFlipperApp* app, InputEvent* event)
{
    if(app->screen == MorseFlipperScreenRun) {
        morse_flipper_key_evt(app, event);
        return true;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        morse_flipper_key_evt(app, event);
        return true;
    }

    if(app->screen != MorseFlipperScreenHome) return false;

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

    return false;
}

static bool morse_flipper_input_chunk_b( MorseFlipperApp* app, InputEvent* event, uint32_t now_ms)
{
    if(morse_flipper_trainer_input(app, event)) return true;
    if(morse_flipper_straight_input(app, event, now_ms)) return true;
    if(morse_flipper_session_input(app, event, now_ms)) return true;
    if(morse_flipper_browse_input(app, event)) return true;
    if(morse_flipper_rf_input(app, event)) return true;
    if(morse_flipper_run_trace_home_input(app, event)) return true;
    return false;
}
