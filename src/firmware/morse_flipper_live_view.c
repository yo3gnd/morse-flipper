#include "fonts/morse_flipper_terminus24.h"
#include "morse_flipper_app_i.h"
#include "morse_flipper_cw_token.h"
#include "morse_flipper_run_layout.h"

static void morse_flipper_draw_left_exit_hint(Canvas* canvas) {
    canvas_draw_box(canvas, 124, 32, 1, 1);
    canvas_draw_box(canvas, 125, 31, 1, 3);
    canvas_draw_box(canvas, 126, 30, 1, 5);
    canvas_draw_box(canvas, 127, 29, 1, 7);
}

static void morse_flipper_draw_straight_prompt(Canvas* canvas, int32_t cx, int32_t cy, uint8_t ch);

static void morse_flipper_draw_tx_history_divider(Canvas* canvas, bool left_hint) {
    if(canvas == NULL) return;

    if(!left_hint) {
        canvas_draw_line(canvas, 0, 34, 119, 34);
        return;
    }

    canvas_draw_line(canvas, 0, 34, 119, 34);
    canvas_draw_box(canvas, 124, 34, 1, 1);
    canvas_draw_box(canvas, 125, 33, 1, 3);
    canvas_draw_box(canvas, 126, 32, 1, 5);
    canvas_draw_box(canvas, 127, 31, 1, 7);
}

static void morse_flipper_txg_score_line(const MorseFlipperApp* app, char* out, size_t out_sz) {
    unsigned pct;

    if(out == NULL || out_sz == 0U) return;
    pct = app != NULL && app->txg_session_total != 0U ?
              ((unsigned)app->txg_session_good * 100U) / app->txg_session_total :
              0U;
    snprintf(
        out,
        out_sz,
        "%u/%u  %u%%",
        app ? (unsigned)app->txg_session_good : 0U,
        app ? (unsigned)app->txg_session_total : 0U,
        pct);
}

static void morse_flipper_txg_score_pct(const MorseFlipperApp* app, char* out, size_t out_sz) {
    unsigned pct;

    if(out == NULL || out_sz == 0U) return;
    pct = app != NULL && app->txg_session_total != 0U ?
              ((unsigned)app->txg_session_good * 100U) / app->txg_session_total :
              0U;
    if(pct > 100U) pct = 100U;
    snprintf(out, out_sz, "%u%%", pct);
}

static void morse_flipper_draw_txg_big_slots(Canvas* canvas, int32_t cy, const char* text) {
    const int32_t gap = 3;
    const int32_t cell = (int32_t)MORSE_FLIPPER_TERMINUS24_WIDTH;
    const int32_t total = (cell * 5) + (gap * 4);
    int32_t cx = ((128 - total) / 2) + (cell / 2);

    if(canvas == NULL || text == NULL) return;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_TX_GROUP_LEN; i++) {
        if(text[i] == '\0') break;
        morse_flipper_draw_straight_prompt(
            canvas,
            cx + ((cell + gap) * (int32_t)i),
            cy,
            text[i]);
    }
}

static void morse_flipper_draw_tx_groups_practice(Canvas* canvas, MorseFlipperApp* app) {
    char score[8];
    uint8_t x;

    if(canvas == NULL || app == NULL) return;

    morse_flipper_draw_txg_big_slots(canvas, 18, app->tx_group.target);
    morse_flipper_draw_tx_history_divider(canvas, morse_flipper_live_left_hint(app));
    morse_flipper_draw_txg_big_slots(canvas, 49, app->tx_group.answer);

    canvas_set_font(canvas, FontSecondary);
    morse_flipper_txg_score_pct(app, score, sizeof(score));
    x = (uint8_t)(126U - canvas_string_width(canvas, score));
    canvas_draw_str(canvas, x, 64, score);
}

static void morse_flipper_draw_txg_label(Canvas* canvas, int32_t x, int32_t y, const char* s, bool bad) {
    uint16_t w;

    if(!bad) {
        canvas_draw_str(canvas, x, y, s);
        return;
    }

    w = canvas_string_width(canvas, s);
    canvas_draw_box(canvas, x - 1, y - 7, w + 2, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, s);
    canvas_set_color(canvas, ColorBlack);
}

static void morse_flipper_draw_txg_metric(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    const char* label,
    const char* value,
    bool bad) {
    morse_flipper_draw_txg_label(canvas, x, y, label, bad);
    canvas_draw_str(canvas, x + 28, y, value);
}

static uint8_t morse_flipper_txg_countdown_s(const MorseFlipperApp* app) {
    uint32_t now;
    uint32_t left;

    if(app == NULL || app->txg_result_until == 0U) return 0U;
    now = furi_get_tick();
    if(now >= app->txg_result_until) return 0U;
    left = app->txg_result_until - now;
    return (uint8_t)((left + 999U) / 1000U);
}

static void morse_flipper_draw_tx_groups_result(Canvas* canvas, MorseFlipperApp* app) {
    char a[12];
    char b[12];
    char c[4];
    const MorseFlipperTxGroupResult* r;

    if(canvas == NULL || app == NULL) return;
    r = &app->tx_group.result;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, r->passed ? "OK" : "Fail");
    canvas_set_font(canvas, FontKeyboard);

    snprintf(a, sizeof(a), "%u/5", (unsigned)r->correct);
    morse_flipper_draw_txg_metric(canvas, 1, 20, "Corr", a, !r->correct_pass);
    snprintf(a, sizeof(a), "%u%%", (unsigned)r->speed_pct);
    morse_flipper_draw_txg_metric(canvas, 1, 29, "Time", a, !r->speed_pass);
    snprintf(a, sizeof(a), "%u%%", (unsigned)r->letter_gap_pct);
    morse_flipper_draw_txg_metric(canvas, 1, 38, "LGap", a, !r->letter_gap_pass);

    if(app->tx_group.sk) {
        snprintf(a, sizeof(a), "%u.%02u", (unsigned)(r->ratio_x100 / 100U), (unsigned)(r->ratio_x100 % 100U));
        morse_flipper_draw_txg_metric(canvas, 64, 20, "Rtio", a, !r->ratio_pass);
        snprintf(a, sizeof(a), "%u%%", (unsigned)r->accuracy_pct);
        morse_flipper_draw_txg_metric(canvas, 64, 29, "Acc", a, !r->accuracy_pass);
        snprintf(a, sizeof(a), "%u%%", (unsigned)r->dit_gap_pct);
        morse_flipper_draw_txg_metric(canvas, 64, 38, "DGap", a, !r->dit_gap_pass);
        snprintf(a, sizeof(a), "%u%%", (unsigned)r->variance_pct);
        morse_flipper_draw_txg_metric(canvas, 64, 47, "Var", a, !r->variance_pass);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 56, (r->fault && r->fault[0]) ? r->fault : "");
    snprintf(c, sizeof(c), "%u", (unsigned)morse_flipper_txg_countdown_s(app));
    canvas_draw_str(canvas, 2, 64, c);
    morse_flipper_txg_score_line(app, b, sizeof(b));
    canvas_draw_str(canvas, 126 - canvas_string_width(canvas, b), 64, b);
    if(app->input_source == MorseFlipperInputSourceButtons && !app->txg_sk)
        morse_flipper_draw_left_exit_hint(canvas);
}

static uint16_t morse_flipper_txg_avg_u16(uint32_t sum, uint16_t n) {
    if(n == 0U) return 0U;
    return (uint16_t)((sum + (n / 2U)) / n);
}

static void morse_flipper_draw_tx_groups_final(Canvas* canvas, MorseFlipperApp* app) {
    char v[24];
    uint16_t avg;
    uint8_t y = 20U;

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Final score");
    canvas_set_font(canvas, FontKeyboard);

    snprintf(v, sizeof(v), "%u/%u", (unsigned)app->txg_session_good, (unsigned)app->txg_session_total);
    morse_flipper_draw_txg_metric(canvas, 1, y, "Pass", v, false);
    y += 9U;
    snprintf(v, sizeof(v), "%u%%", (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_speed, app->txg_session_total));
    morse_flipper_draw_txg_metric(canvas, 1, y, "Time", v, false);
    y += 9U;
    snprintf(v, sizeof(v), "%u%%", (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_lgap, app->txg_session_total));
    morse_flipper_draw_txg_metric(canvas, 1, y, "LGap", v, false);

    if(app->txg_session_sk != 0U) {
        avg = morse_flipper_txg_avg_u16(app->txg_sum_ratio, app->txg_session_sk);
        snprintf(v, sizeof(v), "%u.%02u", (unsigned)(avg / 100U), (unsigned)(avg % 100U));
        morse_flipper_draw_txg_metric(canvas, 64, 20, "Rtio", v, false);
        snprintf(v, sizeof(v), "%u%%", (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_accuracy, app->txg_session_sk));
        morse_flipper_draw_txg_metric(canvas, 64, 29, "Acc", v, false);
        snprintf(v, sizeof(v), "%u%%", (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_dgap, app->txg_session_sk));
        morse_flipper_draw_txg_metric(canvas, 64, 38, "DGap", v, false);
        snprintf(v, sizeof(v), "%u%%", (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_variance, app->txg_session_sk));
        morse_flipper_draw_txg_metric(canvas, 64, 47, "Var", v, false);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 64, "Back exit");
    morse_flipper_txg_score_line(app, v, sizeof(v));
    canvas_draw_str(canvas, 126 - canvas_string_width(canvas, v), 64, v);
    if(app->input_source == MorseFlipperInputSourceButtons && !app->txg_sk)
        morse_flipper_draw_left_exit_hint(canvas);
}

static void morse_flipper_rf_edit_text(char* out, size_t out_sz, uint32_t khz) {
    if(out == NULL || out_sz == 0U) return;
    snprintf(out, out_sz, "%06lu", (unsigned long)(khz % 1000000U));
}

static void morse_flipper_draw_rf_freq_digit(Canvas* canvas, int32_t x, int32_t y, size_t w, size_t h, bool focused, char digit) {
    char text[2] = {digit, '\0'};

    if(canvas == NULL) return;

    canvas_set_color(canvas, ColorBlack);
    if(focused) {
        canvas_draw_triangle(canvas, x + (int32_t)(w / 2U), y - 2, 5, 3, CanvasDirectionBottomToTop);
        canvas_draw_triangle(canvas, x + (int32_t)(w / 2U), y + (int32_t)h + 2, 5, 3, CanvasDirectionTopToBottom);
        canvas_draw_rbox(canvas, x, y, w, h, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 1);
    }

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + (int32_t)(w / 2U),
        y + (int32_t)(h / 2U),
        AlignCenter,
        AlignCenter,
        text);
    canvas_set_color(canvas, ColorBlack);
}

static void morse_flipper_draw_rf_freq_picker(Canvas* canvas, const MorseFlipperApp* app) {
    char digits[MORSE_FLIPPER_RF_FREQ_DIGITS + 1U];
    const int32_t cell_w = 18;
    const int32_t cell_h = 20;
    const int32_t gap = 1;
    const int32_t total_w =
        (cell_w * (int32_t)MORSE_FLIPPER_RF_FREQ_DIGITS) +
        (gap * ((int32_t)MORSE_FLIPPER_RF_FREQ_DIGITS - 1));
    const int32_t x0 = (128 - total_w) / 2;
    const int32_t y0 = 11;
    uint8_t i;

    if(canvas == NULL || app == NULL) return;

    morse_flipper_rf_edit_text(digits, sizeof(digits), app->rf_edit_khz);
    for(i = 0U; i < MORSE_FLIPPER_RF_FREQ_DIGITS; i++) {
        morse_flipper_draw_rf_freq_digit(
            canvas,
            x0 + ((cell_w + gap) * (int32_t)i),
            y0,
            (size_t)cell_w,
            (size_t)cell_h,
            i == (app->rf_freq_focus % MORSE_FLIPPER_RF_FREQ_DIGITS),
            digits[i]);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        64,
        52,
        AlignCenter,
        AlignCenter,
        morse_flipper_rf_tx_allowed_khz(app->rf_edit_khz) ? "TX allowed" : "TX restricted");
}

static uint8_t morse_flipper_live_upper_char(uint8_t ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static void morse_flipper_draw_straight_prompt(Canvas* canvas, int32_t cx, int32_t cy, uint8_t ch) {
    const MorseFlipperTerminus24Glyph* glyph;
    int32_t x0;
    int32_t y0;
    size_t row;
    size_t x_max;
    size_t y_max;

    if(canvas == NULL) return;

    glyph = morse_flipper_terminus24_glyph(morse_flipper_live_upper_char(ch));
    x0 = cx - (int32_t)(MORSE_FLIPPER_TERMINUS24_WIDTH / 2U);
    y0 = cy - (int32_t)(MORSE_FLIPPER_TERMINUS24_HEIGHT / 2U);
    x_max = canvas_width(canvas);
    y_max = canvas_height(canvas);

    for(row = 0U; row < MORSE_FLIPPER_TERMINUS24_HEIGHT; row++) {
        uint16_t bits = glyph->rows[row];
        uint8_t col;

        for(col = 0U; col < MORSE_FLIPPER_TERMINUS24_WIDTH; col++) {
            if((bits & (uint16_t)(1U << (11U - col))) != 0U) {
                int32_t px = x0 + (int32_t)col;
                int32_t py = y0 + (int32_t)row;

                if(px >= 0 && py >= 0 && (size_t)px < x_max && (size_t)py < y_max) {
                    canvas_draw_dot(canvas, px, py);
                }
            }
        }
    }
}

static uint16_t morse_flipper_canvas_glyph_width(uint8_t ch, void* ctx) {
    Canvas* canvas = ctx;
    if(canvas == NULL) return 0U;
    return (uint16_t)canvas_glyph_width(canvas, ch);
}

static void morse_flipper_draw_run_text(Canvas* canvas, int32_t x, int32_t y, const char* text) {
    static const uint8_t aa_glyph[] = {
        0x02U,
        0x02U,
        0x12U,
        0x32U,
        0x7EU,
        0x30U,
        0x10U,
    };

    if(canvas == NULL || text == NULL) return;

    while(*text != '\0') {
        uint8_t ch = (uint8_t)*text++;

        if(ch == MORSE_FLIPPER_CW_TOKEN_AA) {
            for(size_t row = 0U; row < sizeof(aa_glyph) / sizeof(aa_glyph[0]); row++) {
                for(size_t col = 0U; col < 8U; col++) {
                    if((aa_glyph[row] & (uint8_t)(1U << (7U - col))) != 0U) {
                        canvas_draw_dot(canvas, x + (int32_t)col, y - 7 + (int32_t)row);
                    }
                }
            }
            x += 8;
            continue;
        }

        if(morse_flipper_cw_token_is_private(ch)) {
            const char* label = morse_flipper_cw_token_label(ch);
            uint16_t w = (uint16_t)canvas_string_width(canvas, label);

            canvas_draw_str(canvas, x, y, label);
            if(w == 0U) w = 1U;
            canvas_draw_line(canvas, x, y - 8, x + (int32_t)w - 1, y - 8);
            x += w;
            continue;
        }

        char s[2] = {(char)ch, '\0'};
        canvas_draw_str(canvas, x, y, s);
        x += (int32_t)canvas_glyph_width(canvas, ch);
    }
}

static void morse_flipper_gpio_probe_pair_text(const MorseFlipperApp* app, uint8_t pin_idx, char* out, size_t out_sz) {
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

    if(app->startup_gpio_probe_state == MorseFlipperGpioProbeGroundToBoth) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_b, sizeof(pair_b));
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, pair_a);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, pair_b);
    } else if(app->startup_gpio_probe_state == MorseFlipperGpioProbeGroundToDah) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dah_idx, pair_a, sizeof(pair_a));
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
    } else if(app->startup_gpio_probe_state == MorseFlipperGpioProbeGroundToDit) {
        morse_flipper_gpio_probe_pair_text(app, app->gpio_dit_idx, pair_a, sizeof(pair_a));
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, pair_a);
    }

    if(!morse_flipper_gpio_probe_forces_straight(app->startup_gpio_probe_state)) {
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Press back to continue");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "Mono jack in stereo plug?");
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Press OK to switch to SK mode");
}

static uint8_t morse_flipper_straight_units_from_ms(uint16_t ms, uint16_t dit_ms) {
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
    bool use_ms) {
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

static void morse_flipper_draw_straight_strip(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    const char* code,
    const uint16_t* marks_ms,
    const uint16_t* spaces_ms,
    uint16_t dit_ms,
    uint8_t ref_units,
    bool use_ms) {
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

static void morse_flipper_draw_straight_countdown(Canvas* canvas, const MorseFlipperApp* app) {
    char wait_txt[12];
    uint32_t now;
    uint32_t left_ms;

    if(canvas == NULL || app == NULL || app->straight_next_at == 0U) return;
    now = furi_get_tick();
    if(now >= app->straight_next_at) return;

    left_ms = app->straight_next_at - now;
    snprintf(wait_txt, sizeof(wait_txt), "%u", (unsigned)((left_ms + 999U) / 1000U));
    canvas_draw_str(canvas, 2, 64, wait_txt);
}

static void morse_flipper_draw_straight_metrics(Canvas* canvas, const MorseFlipperApp* app) {
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
    count_fail = app->straight_done &&
                 (answer_empty ||
                  !morse_flipper_straight_trainer_symbol_count_match(&app->straight_trainer));

    if(app->straight_done && !count_fail) {
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

    if(app->straight_done && count_fail) {
        canvas_set_font(canvas, FontPrimary);
        x = (uint8_t)((128U - canvas_string_width(canvas, "FAIL")) / 2U);
        canvas_draw_str(canvas, x, 46, "FAIL");
    } else if(app->straight_done) {
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
    if(app->straight_done) morse_flipper_draw_straight_countdown(canvas, app);
    x = (uint8_t)(126U - canvas_string_width(canvas, cnt));
    canvas_draw_str(canvas, x, 64, cnt);
}

static void morse_flipper_draw_tx_history_screen_custom(
    Canvas* canvas,
    MorseFlipperApp* app,
    const char* second_line,
    const char* hint_override) {
    MorseFlipperRunHistory preview_history;
    MorseFlipperRunLayout layout;
    char mode_line[32];
    char hint_line[32];
    const char* footer;
    char preview;
    bool preview_extendable;
    bool left_hint;
    static const uint8_t row_y[MORSE_FLIPPER_RUN_HISTORY_ROWS] = {10U, 20U, 30U};

    if(canvas == NULL || app == NULL) return;

    preview_history = app->run_history;
    preview = morse_flipper_live_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
    preview_extendable = morse_flipper_cw_decoder_preview_extendable(&app->tx_decoder);
    if(preview != 0 && preview != ' ' && preview != '|') {
        char preview_text[2] = {preview, '\0'};
        morse_flipper_run_history_append(&preview_history, preview_text);
    }

    left_hint = morse_flipper_live_left_hint(app);

    canvas_set_font(canvas, FontSecondary);
    morse_flipper_run_layout_build(
        morse_flipper_run_history_text(&preview_history),
        preview != 0 && preview != ' ' && preview != '|' && preview_extendable,
        126U,
        morse_flipper_canvas_glyph_width,
        canvas,
        &layout);
    morse_flipper_draw_run_text(canvas, 1, row_y[0], layout.rows[0]);
    morse_flipper_draw_run_text(canvas, 1, row_y[1], layout.rows[1]);
    morse_flipper_draw_run_text(canvas, 1, row_y[2], layout.rows[2]);
    if(layout.underline_valid && layout.underline_row < MORSE_FLIPPER_RUN_HISTORY_ROWS) {
        uint16_t underline_x = (uint16_t)(1U + layout.underline_x);
        uint8_t underline_w = layout.underline_w == 0U ? 1U : layout.underline_w;
        uint8_t underline_y = (uint8_t)(row_y[layout.underline_row] + 2U);
        canvas_draw_line(
            canvas,
            (int32_t)underline_x,
            underline_y,
            (int32_t)(underline_x + underline_w - 1U),
            underline_y);
    }
    morse_flipper_draw_tx_history_divider(canvas, left_hint);
    canvas_draw_str(canvas, 3, 44, morse_flipper_run_mode_line(app, mode_line, sizeof(mode_line)));
    canvas_draw_str(canvas, 3, 54, second_line ? second_line : "");
    footer = hint_override ? hint_override : morse_flipper_run_hint(app, hint_line, sizeof(hint_line));
    if(canvas_string_width(canvas, footer) > 124) canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 3, 64, footer);
}

static void morse_flipper_draw_tx_history_screen(
    Canvas* canvas,
    MorseFlipperApp* app,
    const char* second_line) {
    morse_flipper_draw_tx_history_screen_custom(canvas, app, second_line, NULL);
}

static uint8_t morse_flipper_rf_rssi_bar_px(int8_t dbm, uint8_t width) {
    const int16_t lo = -115;
    const int16_t hi = -50;
    int16_t clamped = dbm;

    if(clamped <= lo) return 0U;
    if(clamped >= hi) return width;
    return (uint8_t)(((clamped - lo) * width) / (hi - lo));
}

static void morse_flipper_draw_rf_rssi_bar(Canvas* canvas, const MorseFlipperApp* app) {
    const int32_t x = 3;
    const int32_t y = 51;
    const uint8_t w = 122;
    const uint8_t h = 5;
    const uint8_t inner_w = w - 2U;
    uint8_t fill_w = 0U;
    uint8_t peak_w = 0U;
    uint8_t thr_w = 0U;
    int32_t peak_x;
    int32_t thr_x;

    if(canvas == NULL || app == NULL) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);

    thr_w = morse_flipper_rf_rssi_bar_px(app->rf_monitor_threshold_dbm, inner_w);
    thr_x = x + 1 + thr_w;
    if(thr_x > x + (int32_t)inner_w) thr_x = x + (int32_t)inner_w;
    canvas_draw_box(canvas, thr_x - 2, y - 4, 5, 1);
    canvas_draw_box(canvas, thr_x - 1, y - 3, 3, 1);
    canvas_draw_box(canvas, thr_x, y - 2, 1, 1);

    if(!app->rf_rssi_valid) return;

    fill_w = morse_flipper_rf_rssi_bar_px(app->rf_rssi_dbm, inner_w);
    peak_w = morse_flipper_rf_rssi_bar_px(app->rf_rssi_peak_dbm, inner_w);

    if(fill_w != 0U) canvas_draw_box(canvas, x + 1, y + 1, fill_w, h - 2U);

    peak_x = x + 1 + peak_w;
    if(peak_x > x + (int32_t)inner_w) peak_x = x + (int32_t)inner_w;
    canvas_set_color(canvas, peak_w <= fill_w ? ColorWhite : ColorBlack);
    canvas_draw_box(canvas, peak_x, y + 1, 1, h - 2U);
    canvas_set_color(canvas, ColorBlack);
}

static void morse_flipper_draw_rf_rx_screen(Canvas* canvas, MorseFlipperApp* app) {
    char line[32];

    if(canvas == NULL || app == NULL) return;

    morse_flipper_draw_tx_history_divider(canvas, false);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 44, morse_flipper_rf_khz_line(app, line, sizeof(line)));
    morse_flipper_draw_rf_rssi_bar(canvas, app);
    canvas_draw_str(canvas, 3, 64, morse_flipper_rf_rssi_line(app, line, sizeof(line)));
    canvas_draw_str(canvas, 125 - canvas_string_width(canvas, "Bk exit"), 64, "Bk exit");
}

void morse_flipper_draw(Canvas* canvas, void* ctx) {
    MorseFlipperApp* app = ctx;
    char tone_line[32];
    char input_line[32];
    char trace_line1[32];
    char trace_line2[32];
    char trace_line3[32];
    char trace_line4[32];
    char trainer_line[32];
    char trainer_line2[32];
    char trainer_line3[32];
    char browse_line[32];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->audio_wait_active) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Please wait");
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

    if(app->screen == MorseFlipperScreenHamStartRefusal) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Please connect");
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "your paddle or SK");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenHamAssign) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Press Up, Down,");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "Left or Right");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "to assign this message");
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Bk cancel");
        return;
    }

    if(app->screen == MorseFlipperScreenHamAssignments) {
        char line[64];

        canvas_set_font(canvas, FontSecondary);
        for(uint8_t i = 0U; i < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS; i++) {
            const char* text = morse_flipper_ham_keyer_assignment_text(&app->ham_keyer, i);

            snprintf(
                line,
                sizeof(line),
                "%s: %.42s",
                morse_flipper_ham_keyer_dir_label(i),
                text[0] ? text : "-");
            canvas_draw_str(canvas, 3, (int32_t)(12U + (i * 12U)), line);
        }
        canvas_draw_str(canvas, 3, 64, "Bk back");
        return;
    }

    if(app->screen == MorseFlipperScreenHamRun) {
        uint32_t now_ms = furi_get_tick();

        snprintf(
            browse_line,
            sizeof(browse_line),
            "Back: Bkin %s hold L exit",
            app->ham_keyer.break_in_enabled ? "on" : "off");
        morse_flipper_draw_tx_history_screen_custom(
            canvas,
            app,
            "P16 PTT  P15 Key",
            browse_line);
        if(app->ham_macro_active && app->ham_macro_dir < MORSE_FLIPPER_HAM_KEYER_ASSIGNMENTS) {
            snprintf(
                browse_line,
                sizeof(browse_line),
                "Send %s",
                morse_flipper_ham_keyer_dir_label(app->ham_macro_dir));
            canvas_draw_str(canvas, 92, 54, browse_line);
        } else if(app->ham_notice[0] != '\0' && now_ms < app->ham_notice_until) {
            canvas_draw_str(canvas, 98, 54, app->ham_notice);
        }
        return;
    }

    if(app->screen == MorseFlipperScreenRun) {
        morse_flipper_draw_tx_history_screen(canvas, app, morse_flipper_run_usb_name(app));
        return;
    }

    if(app->screen == MorseFlipperScreenRfFreq) {
        morse_flipper_draw_rf_freq_picker(canvas, app);
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
        if(morse_flipper_gpio_probe_notice_active(app) || morse_flipper_gpio_probe_blocks_start(app)) {
            morse_flipper_draw_gpio_probe_overlay(canvas, app);
            return;
        }

        if(!app->straight_started || morse_flipper_straight_countdown_active(app)) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Straight trainer");
            canvas_set_font(canvas, FontSecondary);
            if(morse_flipper_straight_countdown_active(app)) {
                canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Starting");
                morse_flipper_draw_straight_countdown(canvas, app);
            } else if(app->input_source == MorseFlipperInputSourceButtons) {
                canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Press OK to start");
            } else {
                canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
                canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Press your key to start");
            }
            return;
        }

        morse_flipper_draw_straight_prompt(canvas, 19, 18, morse_flipper_straight_trainer_target_char(&app->straight_trainer));

        if(app->straight_done &&
           morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
            morse_flipper_draw_straight_strip(
                canvas,
                39,
                6,
                85,
                morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
                app->straight_trainer.target_marks_ms,
                NULL,
                morse_flipper_current_straight_dit_ms(app),
                morse_flipper_straight_trainer_ref_units_max(&app->straight_trainer),
                false);
            morse_flipper_draw_straight_strip(
                canvas,
                39,
                20,
                85,
                morse_flipper_straight_trainer_answer(&app->straight_trainer),
                app->straight_trainer.answer_marks_ms,
                app->straight_trainer.answer_spaces_ms,
                morse_flipper_current_straight_dit_ms(app),
                morse_flipper_straight_trainer_ref_units_max(&app->straight_trainer),
                true);
        }

        morse_flipper_draw_straight_metrics(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenSession) {
        if(morse_flipper_gpio_probe_notice_active(app) || morse_flipper_gpio_probe_blocks_start(app)) {
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

    if(app->screen == MorseFlipperScreenTxGroups ||
       app->screen == MorseFlipperScreenTxGroupsResult ||
       app->screen == MorseFlipperScreenTxGroupsFinal) {
        canvas_set_font(canvas, FontSecondary);
        if(app->screen == MorseFlipperScreenTxGroups && !app->txg_started) {
            if(morse_flipper_gpio_probe_notice_active(app) || morse_flipper_gpio_probe_blocks_start(app)) {
                morse_flipper_draw_gpio_probe_overlay(canvas, app);
                return;
            }
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "TX Groups of 5");
            canvas_set_font(canvas, FontSecondary);
            if(app->input_source == MorseFlipperInputSourceButtons) {
                canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Press OK to start");
            } else {
                canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
                canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Press your key to start");
            }
        } else if(app->screen == MorseFlipperScreenTxGroups) {
            morse_flipper_draw_tx_groups_practice(canvas, app);
        } else if(app->screen == MorseFlipperScreenTxGroupsResult) {
            morse_flipper_draw_tx_groups_result(canvas, app);
        } else {
            morse_flipper_draw_tx_groups_final(canvas, app);
        }
        return;
    }

    if(app->screen == MorseFlipperScreenRf) {
        morse_flipper_draw_tx_history_screen(canvas, app, morse_flipper_rf_khz_line(app, browse_line, sizeof(browse_line)));
        return;
    }

    if(app->screen == MorseFlipperScreenRfRx) {
        morse_flipper_draw_rf_rx_screen(canvas, app);
        return;
    }

    if(app->screen == MorseFlipperScreenTrace) {
        char tx_trace[48];
        char gpio_trace[48];

        morse_flipper_cw_token_expand_text(
            tx_trace,
            sizeof(tx_trace),
            app->rf_tx_text[0] ? app->rf_tx_text : "-");
        morse_flipper_cw_token_expand_text(
            gpio_trace,
            sizeof(gpio_trace),
            app->gpio_text[0] ? app->gpio_text : "-");
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
            tx_trace,
            gpio_trace);

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
    canvas_draw_str(canvas, 8, 14, "Keying");

    snprintf(
        tone_line,
        sizeof(tone_line),
        "< %s > %s",
        morse_flipper_current_tone_name(app),
        morse_keyer_mode_name(morse_flipper_current_keyer_mode(app)));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 8, 30, morse_flipper_pc_state_name(app));
    canvas_draw_str(canvas, 8, 42, tone_line);
    canvas_draw_str(canvas, 8, 52, morse_flipper_status_line(app, browse_line, sizeof(browse_line)));
    canvas_draw_str(canvas, 8, 62, "L/R tone U src D/hD");
}
