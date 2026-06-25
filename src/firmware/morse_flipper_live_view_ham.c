#include "morse_flipper_app_i.h"

void morse_flipper_draw_ham_start_refusal(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Please connect");
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "your paddle or SK");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Bk back");
}

void morse_flipper_draw_ham_assign(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Press Up, Down,");
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "Left or Right");
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "to assign this message");
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Bk cancel");
}

void morse_flipper_draw_ham_assignments(Canvas* canvas, MorseFlipperApp* app) {
    char line[64];

    if(canvas == NULL || app == NULL) return;

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
}

void morse_flipper_draw_ham_run(Canvas* canvas, MorseFlipperApp* app) {
    char browse_line[32];
    uint32_t now_ms;

    if(canvas == NULL || app == NULL) return;

    now_ms = furi_get_tick();

    snprintf(
        browse_line,
        sizeof(browse_line),
        "Back: Bkin %s hold L exit",
        app->ham_keyer.break_in_enabled ? "on" : "off");
    morse_flipper_draw_tx_history_screen_custom(canvas, app, "P16 PTT  P15 Key", browse_line);
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
}
