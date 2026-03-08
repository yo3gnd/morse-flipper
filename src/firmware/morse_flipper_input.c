static bool morse_flipper_about_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenAbout) return false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        app->about_ok_count++;
        if(app->about_ok_count >= 3U) {
            app->about_ok_count = 0U;
            morse_flipper_scene_open(app, MorseFlipperSceneTrace);
        }
        return true;
    }

    if(event->key == InputKeyBack && (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app->about_ok_count = 0U;
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_startup_probe_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenStartupProbe) return false;

    if(!morse_flipper_gpio_probe_forces_straight(app->startup_gpio_probe_state)) {
        return false;
    }

    if(event->key == InputKeyOk &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app->input_source = MorseFlipperInputSourceStraight;
        app->startup_gpio_probe_state = MorseFlipperGpioProbeOk;
        morse_flipper_save_config(app);
        morse_flipper_refresh_keyer(app, furi_get_tick());
        morse_flipper_poll(app);
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, MorseFlipperSceneMenuMain);
        return true;
    }

    return false;
}

static uint8_t morse_flipper_ham_dir_from_key(InputKey key)
{
    switch(key) {
    case InputKeyUp:
        return MorseFlipperHamKeyerDirUp;
    case InputKeyDown:
        return MorseFlipperHamKeyerDirDown;
    case InputKeyLeft:
        return MorseFlipperHamKeyerDirLeft;
    case InputKeyRight:
        return MorseFlipperHamKeyerDirRight;
    default:
        return MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS;
    }
}

static bool morse_flipper_ham_shell_input(MorseFlipperApp* app, const InputEvent* event)
{
    uint8_t dir;

    if(app->screen != MorseFlipperScreenHamStartRefusal &&
       app->screen != MorseFlipperScreenHamAssign &&
       app->screen != MorseFlipperScreenHamAssignments &&
       app->screen != MorseFlipperScreenHamRun)
        return false;

    if(app->screen == MorseFlipperScreenHamRun) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_live_screen(app, furi_get_tick());
            return true;
        }

        if(event->key == InputKeyBack && event->type == InputTypeShort) {
            if(app->ham_macro_active) {
                morse_flipper_ham_stop_macro(app);
                morse_flipper_update_sidetone(app);
                morse_flipper_view_dirty(app);
                return true;
            }

            app->ham_keyer.break_in_enabled = !app->ham_keyer.break_in_enabled;
            if(!app->ham_keyer.break_in_enabled) {
                app->ptt_tail_until = 0U;
            }
            if(app->ham_keyer.break_in_enabled) {
                morse_flipper_run_history_append(&app->run_history, "\n[BKON]\n");
                morse_flipper_ham_log_append_marker(app, "[BKON]", furi_get_tick());
            } else {
                morse_flipper_run_history_append(&app->run_history, "\n[BKOFF]\n");
                morse_flipper_ham_log_append_marker(app, "[BKOFF]", furi_get_tick());
            }
            morse_flipper_sync_audio_output(app);
            morse_flipper_view_dirty(app);
            return true;
        }

        if(event->key == InputKeyUp && event->type == InputTypeLong) {
            morse_flipper_set_run_wpm(app, (uint8_t)(morse_flipper_current_wpm(app) + 1U));
            return true;
        }

        if(event->key == InputKeyDown && event->type == InputTypeLong) {
            uint8_t wpm = morse_flipper_current_wpm(app);
            morse_flipper_set_run_wpm(app, wpm > 0U ? (uint8_t)(wpm - 1U) : 0U);
            return true;
        }

        if(event->type == InputTypeShort) {
            dir = morse_flipper_ham_dir_from_key(event->key);
            if(dir < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) {
                const char* text = morse_flipper_ham_keyer_assignment_text(&app->ham_keyer, dir);

                if(text[0] != '\0') {
                    morse_flipper_ham_start_macro(app, text, furi_get_tick());
                    app->ham_macro_dir = dir;
                } else {
                    snprintf(
                        app->ham_notice,
                        sizeof(app->ham_notice),
                        "No %s",
                        morse_flipper_ham_keyer_dir_label(dir));
                    app->ham_notice_until = furi_get_tick() + 700U;
                    morse_flipper_view_dirty(app);
                }
                return true;
            }
        }

        return true;
    }

    if(app->screen == MorseFlipperScreenHamAssign && event->type == InputTypeShort) {
        dir = morse_flipper_ham_dir_from_key(event->key);
        if(dir < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) {
            morse_flipper_ham_keyer_assign(&app->ham_keyer, dir, app->ham_selected_message);
            morse_flipper_save_config(app);
            morse_flipper_scene_back(app);
            return true;
        }
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return true;
}

static bool morse_flipper_input_chunk_a(MorseFlipperApp* app, InputEvent* event)
{
    if(morse_flipper_about_input(app, event)) return true;
    if(morse_flipper_startup_probe_input(app, event)) return true;
    if(morse_flipper_ham_shell_input(app, event)) return true;
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
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_straight_input( MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms)
{
    if(app->screen != MorseFlipperScreenStraight) return false;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(morse_flipper_gpio_probe_blocks_start(app)) {
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
                morse_flipper_feed_straight_mark(app, dt, now_ms);
                morse_flipper_view_dirty(app);
            }
        }
        return true;
    }

    if(!app->straight_playback_active && !app->straight_wait_answer && !app->straight_done &&
       event->key == InputKeyOk &&
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

    if((morse_flipper_gpio_probe_notice_active(app) || morse_flipper_gpio_probe_blocks_start(app)) &&
       event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_session(app, now_ms);
        return true;
    }

    if(morse_flipper_gpio_probe_notice_active(app) || morse_flipper_gpio_probe_blocks_start(app)) {
        return true;
    }

    if(morse_flipper_session_repeat_active(app)) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(g.back_exit && event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyBack && g.back_key) return true;
        if(event->key == InputKeyOk && g.btn) return true;
        if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) return true;

        return false;
    }

    if(morse_flipper_session_running_view(app)) {
        if(event->key == InputKeyLeft && event->type == InputTypeLong) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(g.back_exit && event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_leave_session(app, now_ms);
            return true;
        }

        if(event->key == InputKeyBack && g.back_key) return true;
        if(event->key == InputKeyOk && g.btn) return true;
        if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) return true;
    }

    if(event->key == InputKeyOk && event->type == InputTypeShort &&
       !morse_trainer_session_active(&app->trainer) && !app->trainer_playback_active) {
        morse_flipper_start_session(app, now_ms);
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_session(app, now_ms);
        return true;
    }

    return false;
}

static void morse_flipper_leave_session_end(MorseFlipperApp* app, uint32_t now_ms)
{
    if(app == NULL) return;
    morse_flipper_reset_session_state(app, now_ms);
    if(scene_manager_search_and_switch_to_previous_scene( app->scene_manager, MorseFlipperSceneMenuTraining))
        return;
    scene_manager_search_and_switch_to_another_scene(app->scene_manager, MorseFlipperSceneMenuTraining);
}

static bool morse_flipper_session_end_input( MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms)
{
    if(app->screen != MorseFlipperScreenSessionEnd) return false;

    if((event->key == InputKeyOk || event->key == InputKeyBack) &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_leave_session_end(app, now_ms);
        return true;
    }

    return false;
}

static bool morse_flipper_rf_freq_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenRfFreq) return false;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_rf_commit_edit(app);
        morse_flipper_scene_back(app);
        return true;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    if(event->key == InputKeyLeft) {
        morse_flipper_rf_bump_focus(app, -1);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyRight) {
        morse_flipper_rf_bump_focus(app, 1);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyUp) {
        morse_flipper_rf_bump_digit(app, 1);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyDown) {
        morse_flipper_rf_bump_digit(app, -1);
        morse_flipper_view_dirty(app);
        return true;
    }

    return true;
}

static bool morse_flipper_rf_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenRf) return false;
    morse_flipper_handle_active_keying_event(app, event);
    return true;
}

static bool morse_flipper_rf_rx_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenRfRx) return false;

    if((event->key == InputKeyLeft || event->key == InputKeyRight) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        int dir = event->key == InputKeyLeft ? -1 : 1;

        app->rf_monitor_threshold_dbm =
            morse_flipper_rf_clamp_dbm((int8_t)(app->rf_monitor_threshold_dbm + dir));
        app->rf_monitor_tone =
            (app->rf_carrier_present && app->rf_rssi_valid) ||
            (app->rf_rssi_valid && (app->rf_rssi_dbm >= app->rf_monitor_threshold_dbm));
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return true;
}

static bool morse_flipper_run_trace_home_input(MorseFlipperApp* app, InputEvent* event)
{
    if(app->screen == MorseFlipperScreenRun) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        morse_flipper_handle_active_keying_event(app, event);
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
    if(morse_flipper_session_end_input(app, event, now_ms)) return true;
    if(morse_flipper_rf_freq_input(app, event)) return true;
    if(morse_flipper_rf_rx_input(app, event)) return true;
    if(morse_flipper_rf_input(app, event)) return true;
    if(morse_flipper_run_trace_home_input(app, event)) return true;
    return false;
}

static bool morse_flipper_session_live_keying_input(MorseFlipperApp* app, const InputEvent* event)
{
    MorseFlipperInputGate g;

    if(app->screen != MorseFlipperScreenSession) return false;
    if(!morse_flipper_session_repeat_active(app) && !morse_flipper_session_running_view(app))
        return false;
    if(event->type != InputTypePress && event->type != InputTypeRelease) return false;

    g = morse_flipper_input_gate(app);

    if(event->key == InputKeyOk && g.btn) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(event->key == InputKeyBack && g.back_key) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    if(event->key == InputKeyLeft && morse_flipper_session_left_exit_active(app)) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }

    return false;
}

static void morse_flipper_handle_active_keying_event( MorseFlipperApp* app, const InputEvent* event)
{
    uint32_t now_ms = furi_get_tick();
    MorseFlipperInputGate g = morse_flipper_input_gate(app);
    bool btn_src = g.btn;
    bool btn_str = g.btn_str;
    bool btn_pad = g.btn_pad;

    if((app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenRf) &&
       event->key == InputKeyLeft &&
       event->type == InputTypeShort) {
        morse_flipper_reset_run_state(app);
        morse_flipper_view_dirty(app);
        return;
    }

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

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenRf) {
        if(app->screen == MorseFlipperScreenRun &&
           event->key == InputKeyUp &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            morse_flipper_set_run_wpm(app, (uint8_t)(morse_flipper_current_wpm(app) + 1U));
            return;
        }

        if(app->screen == MorseFlipperScreenRun &&
           event->key == InputKeyDown &&
           (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            uint8_t wpm = morse_flipper_current_wpm(app);
            morse_flipper_set_run_wpm(app, wpm > 0U ? (uint8_t)(wpm - 1U) : 0U);
            return;
        }

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
       (event->type == InputTypeShort || event->type == InputTypeLong))
        morse_flipper_scene_back(app);
}

static void morse_flipper_tone_nudge(MorseFlipperApp* app, int dir)
{
    if(app->audio_path == MorseFlipperAudioPathVibration) return;

    int idx = app->tone_idx < COUNT_OF(morse_flipper_tones) ? (int)app->tone_idx :
                                                            (int)MORSE_FLIPPER_DEFAULT_TONE_IDX;
    int current_idx = idx;

    idx += dir;

    if(idx < 0) idx = 0;
    if(idx >= (int)COUNT_OF(morse_flipper_tones)) idx = (int)COUNT_OF(morse_flipper_tones) - 1;
    if(idx == current_idx) return;

    app->tone_idx = (uint8_t)idx;
    app->preview_ticks = MORSE_FLIPPER_PREVIEW_TICKS;

    morse_flipper_save_config(app);
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

bool morse_flipper_live_input(InputEvent* event, void* ctx)
{
    MorseFlipperApp* app = ctx;
    uint32_t now_ms = furi_get_tick();

    if(morse_flipper_input_chunk_a(app, event)) return true;
    if(morse_flipper_session_live_keying_input(app, event)) return true;
    if(morse_flipper_input_chunk_b(app, event, now_ms)) return true;
    return false;
}

bool morse_flipper_custom_event_callback(void* context, uint32_t event)
{
    MorseFlipperApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool morse_flipper_back_event_callback(void* context)
{
    MorseFlipperApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}
