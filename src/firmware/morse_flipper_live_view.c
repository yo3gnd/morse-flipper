static void morse_flipper_draw_left_exit_hint(Canvas* canvas) {
    canvas_draw_box(canvas, 124, 32, 1, 1);
    canvas_draw_box(canvas, 125, 31, 1, 3);
    canvas_draw_box(canvas, 126, 30, 1, 5);
    canvas_draw_box(canvas, 127, 29, 1, 7);
}

static void morse_flipper_draw(Canvas* canvas, void* ctx) {
    MorseFlipperApp* app = ctx;
    char tone_line[32];
    char input_line[32];
    char trace_line1[32];
    char trace_line2[32];
    char trace_line3[32];
    char trace_line4[32];
    char run_line[32];
    char pc_line[32];
    char pc_pad_line[32];
    char pc_str_line[32];
    char trainer_line[32];
    char trainer_line2[32];
    char trainer_line3[32];
    char browse_line[32];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->screen == MorseFlipperScreenRun) {
        snprintf(
            run_line,
            sizeof(run_line),
            "%s %s %s",
            morse_flipper_current_tone(app)->name,
            morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)),
            morse_flipper_hand_name(app));

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 14, "Free Practice");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 8, 28, run_line);
        if(app->input_source == MorseFlipperInputSourceButtons &&
           morse_flipper_button_paddle_keying_active(app)) {
            canvas_draw_str(canvas, 8, 40, morse_flipper_button_map_line(app));
        } else {
            canvas_draw_str(
                canvas,
                8,
                40,
                morse_flipper_free_practice_hint(app, browse_line, sizeof(browse_line)));
        }
        canvas_draw_str(canvas, 8, 52, morse_flipper_input_line(app, input_line, sizeof(input_line)));
        canvas_draw_str(canvas, 8, 62, morse_flipper_status_line(app, browse_line, sizeof(browse_line)));
        if(morse_flipper_live_left_hint(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    if(app->screen == MorseFlipperScreenPc) {
        snprintf(pc_line, sizeof(pc_line), "< %s >", morse_flipper_pc_mode_name(app->pc_mode));
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 18, 14, "PC Mode");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 20, 30, pc_line);
        canvas_draw_str(canvas, 8, 44, morse_flipper_pc_state_name(app));
        if(app->pc_mode == MorseFlipperPcModeKeyboard) {
            snprintf(
                pc_pad_line,
                sizeof(pc_pad_line),
                "pad %s",
                morse_pc_paddle_preset_name(app->pc_paddle_preset));
            snprintf(
                pc_str_line,
                sizeof(pc_str_line),
                "str %s hold R keys",
                morse_pc_straight_preset_name(app->pc_straight_preset));
            canvas_draw_str(canvas, 8, 54, pc_pad_line);
            canvas_draw_str(canvas, 8, 63, pc_str_line);
        } else {
            snprintf(
                tone_line,
                sizeof(tone_line),
                "wpm %u  %s",
                morse_flipper_current_wpm(app),
                morse_flipper_hand_name(app));
            canvas_draw_str(canvas, 8, 54, tone_line);
            canvas_draw_str(canvas, 8, 63, morse_flipper_pc_hint(app, browse_line, sizeof(browse_line)));
        }
        if(morse_flipper_live_left_hint(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    if(app->screen == MorseFlipperScreenPcKeys) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 6, 14, "USB Key Presets");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            pc_pad_line,
            sizeof(pc_pad_line),
            "%c Paddle <%s>",
            app->pc_keys_row == 0U ? '>' : ' ',
            morse_pc_paddle_preset_name(app->pc_paddle_preset));
        snprintf(
            pc_str_line,
            sizeof(pc_str_line),
            "%c Straight <%s>",
            app->pc_keys_row == 1U ? '>' : ' ',
            morse_pc_straight_preset_name(app->pc_straight_preset));
        canvas_draw_str(canvas, 2, 30, pc_pad_line);
        canvas_draw_str(canvas, 2, 44, pc_str_line);
        canvas_draw_str(canvas, 2, 58, "U/D row L/R pick");
        canvas_draw_str(canvas, 2, 64, "OK/Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenTrainer) {
        canvas_set_font(canvas, FontSecondary);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 12, "Koch Setup");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "%c lesson %u/%u",
            app->trainer_row == 0U ? '>' : ' ',
            (unsigned)morse_trainer_lesson(&app->trainer),
            (unsigned)morse_trainer_lesson_count());
        snprintf(
            trainer_line2,
            sizeof(trainer_line2),
            "%c size %u",
            app->trainer_row == 1U ? '>' : ' ',
            (unsigned)morse_trainer_group_size(&app->trainer));
        snprintf(
            trainer_line3,
            sizeof(trainer_line3),
            "%c groups %u",
            app->trainer_row == 2U ? '>' : ' ',
            (unsigned)morse_trainer_session_groups(&app->trainer));
        snprintf(
            tone_line,
            sizeof(tone_line),
            "%c chars %s",
            app->trainer_row == 3U ? '>' : ' ',
            app->trainer.custom_set_idx == 0U ? "lesson" : app->trainer.custom_name);
        canvas_draw_str(canvas, 8, 24, trainer_line);
        canvas_draw_str(canvas, 8, 34, trainer_line2);
        canvas_draw_str(canvas, 8, 44, trainer_line3);
        canvas_draw_str(canvas, 8, 54, tone_line);
        canvas_draw_str(canvas, 8, 64, "OK sess  Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenStraight) {
        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "1ch %c %s",
            morse_flipper_straight_trainer_target_char(&app->straight_trainer),
            morse_flipper_straight_trainer_target_morse(&app->straight_trainer));
        snprintf(
            trainer_line2,
            sizeof(trainer_line2),
            "ans %s",
            morse_flipper_straight_trainer_answer(&app->straight_trainer));
        snprintf(
            trainer_line3,
            sizeof(trainer_line3),
            "err %u drift %u%%",
            (unsigned)morse_flipper_straight_trainer_average_mark_error_ms(&app->straight_trainer),
            (unsigned)morse_flipper_straight_trainer_average_drift_percent(&app->straight_trainer));
        snprintf(
            tone_line,
            sizeof(tone_line),
            "best %c/%u  worst %c/%u",
            app->straight_stats.have_best ? app->straight_stats.best_char : '-',
            (unsigned)app->straight_stats.best_error_ms,
            app->straight_stats.have_worst ? app->straight_stats.worst_char : '-',
            (unsigned)app->straight_stats.worst_error_ms);
        canvas_draw_str(canvas, 2, 10, trainer_line);
        canvas_draw_str(canvas, 2, 22, trainer_line2);
        canvas_draw_str(canvas, 2, 34, trainer_line3);
        canvas_draw_str(
            canvas,
            2,
            46,
            morse_flipper_straight_trainer_error_bars(&app->straight_trainer)[0] != '\0' ?
                morse_flipper_straight_trainer_error_bars(&app->straight_trainer) :
                morse_flipper_straight_trainer_timing_view(&app->straight_trainer));
        canvas_draw_str(canvas, 2, 58, tone_line);
        if(app->straight_playback_active) {
            canvas_draw_str(canvas, 2, 64, "playing  Bk back");
        } else if(app->straight_wait_answer) {
            canvas_draw_str(canvas, 2, 64, morse_flipper_straight_wait_hint(app, browse_line, sizeof(browse_line)));
        } else {
            canvas_draw_str(canvas, 2, 64, "OK play  Bk back");
        }
        return;
    }

    if(app->screen == MorseFlipperScreenSession) {
        morse_flipper_draw_session_rows(canvas, app);
        morse_flipper_draw_session_bottom(canvas, app);
        if(morse_flipper_live_left_hint(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    if(app->screen == MorseFlipperScreenBrowse) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 8, 14, "Session Logs");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            browse_line,
            sizeof(browse_line),
            "%u/%u",
            (unsigned)(app->session_line_idx + 1U),
            (unsigned)app->session_lines.count);
        canvas_draw_str(canvas, 96, 14, browse_line);
        if(app->session_lines.count != 0U) {
            canvas_draw_str(canvas, 2, 32, app->session_lines.lines[app->session_line_idx]);
        } else {
            canvas_draw_str(canvas, 2, 32, "no saved sessions");
        }
        canvas_draw_str(canvas, 2, 62, "U/D pick  R/Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenRf) {
        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "rf %s %s",
            morse_flipper_rf_frequency_text(&app->rf),
            app->rf_live_active ? "live" : "idle");
        canvas_draw_str(canvas, 2, 10, trainer_line);
        if(app->rf_manual_active) {
            snprintf(
                trainer_line2,
                sizeof(trainer_line2),
                "khz %s [%u]",
                app->rf_edit_khz,
                (unsigned)app->rf_edit_digit);
            canvas_draw_str(canvas, 2, 22, trainer_line2);
            canvas_draw_str(canvas, 2, 34, "L/R move U/D digit");
            canvas_draw_str(canvas, 2, 46, "OK apply");
            canvas_draw_str(canvas, 2, 64, "Bk cancel");
            return;
        }

        if(app->rf_live_active) {
            snprintf(
                trainer_line2,
                sizeof(trainer_line2),
                "src %s mode %s",
                morse_flipper_source_short_name(app, browse_line, sizeof(browse_line)),
                morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));
            canvas_draw_str(canvas, 2, 22, trainer_line2);
            snprintf(
                trainer_line3,
                sizeof(trainer_line3),
                "rx %.28s",
                app->rf_rx_text[0] ? app->rf_rx_text : "-");
            canvas_draw_str(canvas, 2, 34, trainer_line3);
            snprintf(
                tone_line,
                sizeof(tone_line),
                "tx %.28s",
                app->rf_tx_text[0] ? app->rf_tx_text : "-");
            canvas_draw_str(canvas, 2, 46, tone_line);
            canvas_draw_str(
                canvas,
                2,
                58,
                morse_flipper_rf_last_error(&app->rf)[0] ? morse_flipper_rf_last_error(&app->rf) :
                                                           morse_flipper_rf_rx_log_line(&app->rf, morse_flipper_rf_rx_log_count(&app->rf) ?
                                                                                                 (morse_flipper_rf_rx_log_count(&app->rf) - 1U) :
                                                                                                 0U));
            canvas_draw_str(
                canvas,
                2,
                64,
                morse_flipper_live_back_is_key(app) ? "live key hold L" : "live key Bk back");
            if(morse_flipper_live_left_hint(app)) {
                morse_flipper_draw_left_exit_hint(canvas);
            }
            return;
        }

        canvas_draw_str(canvas, 2, 22, "L/R step  U band");
        canvas_draw_str(canvas, 2, 34, "OK edit  D live");
        canvas_draw_str(canvas, 2, 46, morse_flipper_rf_last_error(&app->rf)[0] ?
                                            morse_flipper_rf_last_error(&app->rf) :
                                            morse_flipper_rf_manual_khz_text(&app->rf));
        canvas_draw_str(canvas, 2, 58, app->gpio_text[0] ? app->gpio_text : "gpio -");
        canvas_draw_str(canvas, 2, 64, "Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        snprintf(
            trace_line1,
            sizeof(trace_line1),
            "tr %s %s %s",
            morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)),
            morse_flipper_source_short_name(app, browse_line, sizeof(browse_line)),
            morse_flipper_hand_name(app));
        snprintf(
            trace_line2,
            sizeof(trace_line2),
            "pd %u%u out %u%u rep %ld",
            app->keyer.paddle_down[MorseKeyerPaddleDit] ? 1U : 0U,
            app->keyer.paddle_down[MorseKeyerPaddleDah] ? 1U : 0U,
            app->keyer.output_active[MorseKeyerPaddleDit] ? 1U : 0U,
            app->keyer.output_active[MorseKeyerPaddleDah] ? 1U : 0U,
            (long)app->keyer.next_repeat_paddle);
        snprintf(
            trace_line3,
            sizeof(trace_line3),
            "set %u fifo %u pulse %u",
            app->keyer.set_queue_len,
            app->keyer.fifo_queue_len,
            app->keyer.pulse_active ? 1U : 0U);
        snprintf(
            trace_line4,
            sizeof(trace_line4),
            "tx %.12s gp %.11s",
            app->rf_tx_text[0] ? app->rf_tx_text : "-",
            app->gpio_text[0] ? app->gpio_text : "-");

        canvas_draw_str(canvas, 2, 10, trace_line1);
        canvas_draw_str(canvas, 2, 20, morse_flipper_input_line(app, input_line, sizeof(input_line)));
        canvas_draw_str(canvas, 2, 30, trace_line2);
        canvas_draw_str(canvas, 2, 40, trace_line3);
        canvas_draw_str(canvas, 2, 50, trace_line4);
        canvas_draw_str(canvas, 2, 60, morse_flipper_trace_hint(app, browse_line, sizeof(browse_line)));
        if(morse_flipper_live_left_hint(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 14, "Main settings");

    snprintf(
        tone_line,
        sizeof(tone_line),
        "< %s > %s",
        morse_flipper_current_tone(app)->name,
        morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, morse_flipper_pc_state_name(app));
    canvas_draw_str(canvas, 8, 42, tone_line);
    canvas_draw_str(canvas, 8, 52, morse_flipper_status_line(app, browse_line, sizeof(browse_line)));
    canvas_draw_str(canvas, 8, 62, "L/R tone U src D/hD");
}

