#include "morse_flipper_prompt_font.h"

static void morse_flipper_draw_left_exit_hint(Canvas* canvas) {
    canvas_draw_box(canvas, 124, 32, 1, 1);
    canvas_draw_box(canvas, 125, 31, 1, 3);
    canvas_draw_box(canvas, 126, 30, 1, 5);
    canvas_draw_box(canvas, 127, 29, 1, 7);
}

static char morse_flipper_live_upper_char(char ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static void morse_flipper_gpio_probe_pair_text( const MorseFlipperApp* app, uint8_t pin_idx, char* out, size_t out_sz) {
    if(app == NULL || out == NULL || out_sz == 0U) return;
    snprintf(
        out,
        out_sz,
        "%s - %s",
        morse_flipper_gpio_name(app->gpio_ground_idx),
        morse_flipper_gpio_name(pin_idx));
}

static void morse_flipper_draw_gpio_probe_overlay(Canvas* canvas, const MorseFlipperApp* app) {
    char pair_a[24];
    char pair_b[32];

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontPrimary);
    if(morse_flipper_gpio_probe_blocks_start(app)) {
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Short circuit");
        canvas_set_font(canvas, FontSecondary);
        if(app->gpio_probe_state == MorseFlipperGpioProbeGroundToBoth) {
            morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
            morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_b, sizeof(pair_b));
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, pair_a);
            canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, pair_b);
        } else {
            morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_a, sizeof(pair_a));
            canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Do not press the paddles");
        return;
    }

    morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
    snprintf(pair_b, sizeof(pair_b), "%s shorted", pair_a);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, pair_b);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Mono jack in stereo plug?");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Switching to SK mode");
}

static void morse_flipper_draw_startup_gpio_probe(Canvas* canvas, const MorseFlipperApp* app) {
    char pair_a[24];
    char pair_b[24];

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Short circuit");
    canvas_set_font(canvas, FontSecondary);

    if(app->boot_probe == MorseFlipperGpioProbeGroundToBoth) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_b, sizeof(pair_b));
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, pair_a);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, pair_b);
    } else if(app->boot_probe == MorseFlipperGpioProbeGroundToDah) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
    } else if(app->boot_probe == MorseFlipperGpioProbeGroundToDit) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_a, sizeof(pair_a));
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
    }

    if(!morse_flipper_probe_sk(app->boot_probe)) {
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Press back to continue");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "Mono jack in stereo plug?");
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Press OK to switch to SK mode");
}

static uint8_t morse_flipper_straight_units_from_ms(uint16_t ms, uint16_t dit_ms)
{
    uint32_t u;

    if(dit_ms == 0U) return 1U;
    u = (ms + (dit_ms / 2U)) / dit_ms;
    if(u == 0U) u = 1U;
    if(u > 9U) u = 9U;
    return (uint8_t)u;
}

static uint8_t morse_flipper_straight_strip_units(
    const char* code,
    const uint16_t* marks_ms,
    const uint16_t* spaces_ms,
    uint16_t dit_ms,
    bool use_ms)
{
    uint8_t total = 0U;
    size_t i;

    if(code == NULL || code[0] == '\0') return 0U;

    for(i = 0U; code[i] != '\0'; i++) {
        uint8_t u;

        if(use_ms && marks_ms != NULL && marks_ms[i] != 0U) {
            u = morse_flipper_straight_units_from_ms(marks_ms[i], dit_ms);
        } else {
            u = code[i] == '-' ? 3U : 1U;
        }
        total = (uint8_t)(total + u);

        if(code[i + 1U] == '\0') continue;

        if(use_ms && spaces_ms != NULL && spaces_ms[i] != 0U) {
            u = morse_flipper_straight_units_from_ms(spaces_ms[i], dit_ms);
        } else {
            u = 1U;
        }
        total = (uint8_t)(total + u);
    }

    return total;
}

static void morse_flipper_draw_sk_strip(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    const char* code,
    const uint16_t* marks_ms,
    const uint16_t* spaces_ms,
    uint16_t dit_ms,
    uint8_t ref_units,
    bool use_ms)
{
    uint8_t unit_px;
    uint8_t low_y = y + 6U;
    uint8_t hi_y = y + 1U;
    uint8_t pos;
    uint8_t draw_units;
    size_t i;

    if(canvas == NULL || code == NULL || ref_units == 0U) return;
    draw_units = morse_flipper_straight_strip_units(code, marks_ms, spaces_ms, dit_ms, use_ms);
    if(draw_units < ref_units) draw_units = ref_units;
    unit_px = w / (uint8_t)(draw_units + 2U);
    if(unit_px == 0U) unit_px = 1U;
    pos = x;

    canvas_draw_line(canvas, pos, low_y, pos + unit_px, low_y);
    pos = (uint8_t)(pos + unit_px);

    for(i = 0U; code[i] != '\0'; i++) {
        uint8_t u = 1U;
        uint8_t px;

        if(use_ms) {
            if(marks_ms != NULL && marks_ms[i] != 0U) {
                u = morse_flipper_straight_units_from_ms(marks_ms[i], dit_ms);
            } else {
                u = code[i] == '-' ? 3U : 1U;
            }
        } else {
            u = code[i] == '-' ? 3U : 1U;
        }

        px = (uint8_t)(u * unit_px);
        canvas_draw_line(canvas, pos, low_y, pos, hi_y);
        canvas_draw_line(canvas, pos, hi_y, pos + px, hi_y);
        pos = (uint8_t)(pos + px);
        canvas_draw_line(canvas, pos, hi_y, pos, low_y);

        if(code[i + 1U] == '\0') break;

        if(use_ms) {
            if(spaces_ms != NULL && spaces_ms[i] != 0U) {
                u = morse_flipper_straight_units_from_ms(spaces_ms[i], dit_ms);
            } else {
                u = 1U;
            }
        } else {
            u = 1U;
        }

        px = (uint8_t)(u * unit_px);
        canvas_draw_line(canvas, pos, low_y, pos + px, low_y);
        pos = (uint8_t)(pos + px);
    }

    canvas_draw_line(canvas, pos, low_y, pos + unit_px, low_y);
}

static void morse_flipper_draw_sk_met(Canvas* canvas, const MorseFlipperApp* app)
{
    char s_txt[8];
    char di_txt[8];
    char da_txt[8];
    char ra_txt[8];
    char cnt[24];
    unsigned pct = 0U;
    bool answer_empty;
    bool count_fail;
    uint8_t x;

    if(canvas == NULL || app == NULL) return;

    answer_empty = morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] == '\0';
    count_fail = app->sk_done &&
                 (answer_empty ||
                  !morse_flipper_straight_trainer_symbol_count_match(&app->straight_trainer));

    if(app->sk_done && !count_fail) {
        snprintf(
            s_txt,
            sizeof(s_txt),
            "%s",
            morse_flipper_straight_trainer_worst_space_score(&app->straight_trainer) >= 90U ?
                "OK" :
                "");
        if(s_txt[0] == '\0')
            snprintf(s_txt, sizeof(s_txt), "%u", (unsigned)morse_flipper_straight_trainer_worst_space_score(&app->straight_trainer));
        snprintf(
            di_txt,
            sizeof(di_txt),
            "%s",
            morse_flipper_straight_trainer_worst_dit_score(&app->straight_trainer) >= 90U ?
                "OK" :
                "");
        if(di_txt[0] == '\0')
            snprintf(di_txt, sizeof(di_txt), "%u", (unsigned)morse_flipper_straight_trainer_worst_dit_score(&app->straight_trainer));
        snprintf(
            da_txt,
            sizeof(da_txt),
            "%s",
            morse_flipper_straight_trainer_worst_dah_score(&app->straight_trainer) >= 90U ?
                "OK" :
                "");
        if(da_txt[0] == '\0')
            snprintf(da_txt, sizeof(da_txt), "%u", (unsigned)morse_flipper_straight_trainer_worst_dah_score(&app->straight_trainer));
        snprintf(
            ra_txt,
            sizeof(ra_txt),
            "%u.%02u",
            (unsigned)(morse_flipper_straight_trainer_ratio_x100(&app->straight_trainer) / 100U),
            (unsigned)(morse_flipper_straight_trainer_ratio_x100(&app->straight_trainer) % 100U));
    }

    if(app->straight_session_total != 0U)
        pct = ((unsigned)app->straight_session_good * 100U) / app->straight_session_total;
    snprintf(cnt, sizeof(cnt), "%u/%u %u%%",
        (unsigned)app->straight_session_good,
        (unsigned)app->straight_session_total,
        pct);

    if(app->sk_done && count_fail) {
        canvas_set_font(canvas, FontPrimary);
        x = (uint8_t)((128U - canvas_string_width(canvas, "FAIL")) / 2U);
        canvas_draw_str(canvas, x, 46, "FAIL");
    } else if(app->sk_done) {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 46, "S");
        canvas_draw_str(canvas, 10, 46, s_txt);
        canvas_draw_str(canvas, 28, 46, "di");
        canvas_draw_str(canvas, 42, 46, di_txt);
        canvas_draw_str(canvas, 62, 46, "da");
        canvas_draw_str(canvas, 76, 46, da_txt);
        canvas_draw_str(canvas, 96, 46, "r");
        canvas_draw_str(canvas, 102, 46, ra_txt);
    }

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 56, app->straight_worst_line[0] ? app->straight_worst_line : "Wo: -");
    x = (uint8_t)(126U - canvas_string_width(canvas, cnt));
    canvas_draw_str(canvas, x, 64, cnt);
}

static const char* morse_flipper_help_title(uint8_t idx) {
    switch(idx) {
    case MorseFlipperHelpFirstSteps:
        return "First steps";
    case MorseFlipperHelpInputKeys:
        return "Input & keys";
    case MorseFlipperHelpConnectingPaddle:
        return "Connecting the paddle";
    case MorseFlipperHelpLcwo:
        return "LCWO";
    case MorseFlipperHelpPrepping:
        return "Prepping";
    case MorseFlipperHelpContact:
        return "A complete Morse contact";
    case MorseFlipperHelpContesting:
        return "Contesting";
    case MorseFlipperHelpUsbLive:
        return "USB & live practice";
    default:
        return "Moving forward";
    }
}

static const char* morse_flipper_help_blurb(uint8_t idx) {
    switch(idx) {
    case MorseFlipperHelpFirstSteps:
        return "Short guide goes here later.";
    case MorseFlipperHelpInputKeys:
        return "Input notes go here later.";
    case MorseFlipperHelpConnectingPaddle:
        return "Paddle wiring notes go here later.";
    case MorseFlipperHelpLcwo:
        return "LCWO notes go here later.";
    case MorseFlipperHelpPrepping:
        return "Prepping notes go here later.";
    case MorseFlipperHelpContact:
        return "Contact notes go here later.";
    case MorseFlipperHelpContesting:
        return "Contest notes go here later.";
    case MorseFlipperHelpUsbLive:
        return "USB and live notes later.";
    default:
        return "More notes go here later.";
    }
}

static void morse_flipper_draw_help_page(Canvas* canvas, MorseFlipperApp* app) {
    char nbuf[20];
    uint8_t n = app->help_topic;

    if(n >= MorseFlipperHelpCount) n = 0U;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 12, morse_flipper_help_title(n));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 28, morse_flipper_help_blurb(n));
    canvas_draw_str(canvas, 4, 40, "Copy and polish later.");

    snprintf(nbuf, sizeof(nbuf), "%u/%u", (unsigned)(n + 1U), (unsigned)MorseFlipperHelpCount);
    canvas_draw_str(canvas, 104, 12, nbuf);
    canvas_draw_str(canvas, 4, 56, "< prev");
	canvas_draw_str(canvas, 88, 56, "next >");
    canvas_draw_str(canvas, 4, 64, "Bk back");
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
    char trainer_line[32];
    char trainer_line2[32];
    char trainer_line3[32];
    char browse_line[32];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->screen == MorseFlipperScreenHelp) {
        morse_flipper_draw_help_page(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenAbout) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 14, "About");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 30, "Nothing here yet.");
        canvas_draw_str(canvas, 4, 42, "That is the whole point.");
        canvas_draw_str(canvas, 4, 64, "Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenStartupProbe) {
        morse_flipper_draw_startup_gpio_probe(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenRun) {
        MorseFlipperRunHistory preview_history = app->run_history;
        char preview = morse_flipper_live_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));

        snprintf(
            run_line,
            sizeof(run_line),
            "%u wpm  %s",
            (unsigned)morse_flipper_current_wpm(app),
            morse_flipper_run_input_name(app));
        snprintf(
            browse_line,
            sizeof(browse_line),
            "%s  %s",
            morse_flipper_run_keyer_name(app),
            morse_flipper_run_usb_name(app));

        if(preview != 0 && preview != ' ' && preview != '|') {
            char preview_text[2] = {preview, '\0'};
            morse_flipper_run_history_append(&preview_history, preview_text);
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 1, 10, preview_history.line[0]);
        canvas_draw_str(canvas, 1, 20, preview_history.line[1]);
        canvas_draw_str(canvas, 1, 30, preview_history.line[2]);
        canvas_draw_line(canvas, 0, 34, 127, 34);
        canvas_draw_str(canvas, 3, 44, run_line);
        canvas_draw_str(canvas, 3, 54, browse_line);
        canvas_draw_str(canvas, 3, 64, morse_flipper_run_hint(app, input_line, sizeof(input_line)));
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
        if(morse_flipper_gpio_probe_blocks_start(app)) {
            morse_flipper_draw_gpio_probe_overlay(canvas, app);
            return;
        }

        if(!app->sk_started) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Straight Trainer");
            canvas_set_font(canvas, FontSecondary);
            if(app->in_src == MorseFlipperInputSourceButtons) {
                canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Press OK to start");
            } else {
                canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
                canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Press your key to start");
            }
            return;
        }

        snprintf(trainer_line, sizeof(trainer_line), "%c", morse_flipper_straight_trainer_target_char(&app->straight_trainer));
        canvas_set_custom_u8g2_font(canvas, morse_flipper_straight_prompt_font);
        canvas_draw_str_aligned(canvas, 19, 18, AlignCenter, AlignCenter, trainer_line);

        if(app->sk_done &&
           morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
            morse_flipper_draw_sk_strip(
                canvas,
                39,
                6,
                85,
                morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
                app->straight_trainer.target_marks_ms,
                NULL,
                morse_flipper_sk_dit(app),
                morse_flipper_straight_trainer_ref_units_max(&app->straight_trainer),
                false);
            morse_flipper_draw_sk_strip(
                canvas,
                39,
                20,
                85,
                morse_flipper_straight_trainer_answer(&app->straight_trainer),
                app->straight_trainer.answer_marks_ms,
                app->straight_trainer.answer_spaces_ms,
                morse_flipper_sk_dit(app),
                morse_flipper_straight_trainer_ref_units_max(&app->straight_trainer),
                true);
        }

        morse_flipper_draw_sk_met(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenSession) {
        if(morse_flipper_probe_note(app) || morse_flipper_gpio_probe_blocks_start(app)) {
            morse_flipper_draw_gpio_probe_overlay(canvas, app);
            return;
        }

        morse_flipper_draw_session_rows(canvas, app);
        morse_flipper_draw_session_bottom(canvas, app);
        if(morse_flipper_live_left_hint(app)) {
            morse_flipper_draw_left_exit_hint(canvas);
        }
        return;
    }

    if(app->screen == MorseFlipperScreenSessionEnd) {
        morse_flipper_draw_session_end(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenRf) {
        snprintf(
            trainer_line,
            sizeof(trainer_line),
            "rf %s %s",
            morse_flipper_rf_frequency_text(&app->rf),
            app->rf_live ? "live" : "idle");
        canvas_draw_str(canvas, 2, 10, trainer_line);
        if(app->rf_man) {
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

        if(app->rf_live) {
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
            canvas_draw_str( canvas, 2, 64, morse_flipper_live_back_is_key(app) ? "live key hold L" : "live key Bk back");
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
        morse_flipper_tone_name(app),
        morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, morse_flipper_pc_state_name(app));
    canvas_draw_str(canvas, 8, 42, tone_line);
    canvas_draw_str(canvas, 8, 52, morse_flipper_status_line(app, browse_line, sizeof(browse_line)));
    canvas_draw_str(canvas, 8, 62, "L/R tone U src D/hD");
}
