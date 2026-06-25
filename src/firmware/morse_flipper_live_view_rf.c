#include "morse_flipper_app_i.h"

static void morse_flipper_rf_edit_text(char* out, size_t out_sz, uint32_t khz) {
    if(out == NULL || out_sz == 0U) return;
    snprintf(out, out_sz, "%06lu", (unsigned long)(khz % 1000000U));
}

static void morse_flipper_draw_rf_freq_digit(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t w,
    size_t h,
    bool focused,
    char digit) {
    char text[2] = {digit, '\0'};

    if(canvas == NULL) return;

    canvas_set_color(canvas, ColorBlack);
    if(focused) {
        canvas_draw_triangle(
            canvas, x + (int32_t)(w / 2U), y - 2, 5, 3, CanvasDirectionBottomToTop);
        canvas_draw_triangle(
            canvas, x + (int32_t)(w / 2U), y + (int32_t)h + 2, 5, 3, CanvasDirectionTopToBottom);
        canvas_draw_rbox(canvas, x, y, w, h, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 1);
    }

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas, x + (int32_t)(w / 2U), y + (int32_t)(h / 2U), AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
}

void morse_flipper_draw_rf_freq_picker(Canvas* canvas, const MorseFlipperApp* app) {
    char digits[MORSE_FLIPPER_RF_FREQ_DIGITS + 1U];
    const int32_t cell_w = 18;
    const int32_t cell_h = 20;
    const int32_t gap = 1;
    const int32_t total_w = (cell_w * (int32_t)MORSE_FLIPPER_RF_FREQ_DIGITS) +
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

void morse_flipper_draw_rf_rx_screen(Canvas* canvas, MorseFlipperApp* app) {
    char line[32];

    if(canvas == NULL || app == NULL) return;

    morse_flipper_draw_tx_history_divider(canvas, false);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 44, morse_flipper_rf_khz_line(app, line, sizeof(line)));
    morse_flipper_draw_rf_rssi_bar(canvas, app);
    canvas_draw_str(canvas, 3, 64, morse_flipper_rf_rssi_line(app, line, sizeof(line)));
    canvas_draw_str(canvas, 125 - canvas_string_width(canvas, "Bk exit"), 64, "Bk exit");
}
