/*
 * Purpose: Draw Listening progress summary, totals, and bounded history pages.
 * Owns: progress screen layout only; persistence and history loading live in progress.c.
 * Depends on: morse_flipper_app_i.h and progress helpers.
 * Tests: firmware build; rendering is hardware-only.
 */

#include "morse_flipper_app_i.h"

static const char* const morse_flipper_progress_months[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
};

static void morse_flipper_draw_progress_bar(Canvas* canvas, uint8_t done, uint8_t total) {
    uint8_t x = 42U;
    uint8_t y = 54U;
    uint8_t w = 44U;
    uint8_t fill;
    char label[12];

    if(total == 0U) total = 1U;
    if(done > total) done = total;
    fill = (uint8_t)(((uint16_t)done * (w - 2U)) / total);

    canvas_draw_rframe(canvas, x, y, w, 8, 2);
    if(fill != 0U) canvas_draw_rbox(canvas, x + 1U, y + 1U, fill, 6, 1);
    snprintf(label, sizeof(label), "%u/%u", (unsigned)done, (unsigned)total);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignCenter, label);
}

static void morse_flipper_draw_progress_stats(Canvas* canvas, MorseFlipperApp* app) {
    MorseFlipperProgress* progress = app->view_progress;
    char line[40];
    char lesson_label[16];
    char weak[12];
    uint8_t lesson = morse_trainer_lesson(&app->trainer);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "Listening stats");
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "Streak: %u days", (unsigned)progress->streak_days);
    canvas_draw_str(canvas, 4, 21, line);

    if(morse_flipper_effective_trainer_custom_set_idx(app) != 0U) {
        canvas_draw_str(canvas, 4, 33, "Custom chars");
        canvas_draw_str(canvas, 4, 45, "Stats: lesson mode only");
        elements_button_left(canvas, "Lessons");
        elements_button_right(canvas, "More");
        return;
    }

    morse_trainer_lesson_label(lesson, lesson_label, sizeof(lesson_label));
    {
        char lesson_line[64];
        snprintf(lesson_line, sizeof(lesson_line), "Lesson: %s", lesson_label);
        canvas_draw_str(canvas, 4, 32, lesson_line);
    }
    morse_flipper_progress_top_weak(
        progress, morse_trainer_charset(&app->trainer), weak, sizeof(weak));
    snprintf(line, sizeof(line), "Needs improv: %s", weak);
    canvas_draw_str(canvas, 4, 43, line);

    morse_flipper_draw_progress_bar(canvas, progress->lesson_attempts[lesson], 40U);
    elements_button_left(canvas, "Lessons");
    elements_button_right(canvas, "More");
}

static void morse_flipper_draw_progress_totals(Canvas* canvas, MorseFlipperApp* app) {
    MorseFlipperProgress* progress = app->view_progress;
    char line[32];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "Listening totals");
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "Total attempts: %u", (unsigned)progress->total_attempts);
    canvas_draw_str(canvas, 4, 24, line);
    snprintf(line, sizeof(line), "Daily record: %u", (unsigned)progress->daily_record);
    canvas_draw_str(canvas, 4, 36, line);
    snprintf(
        line,
        sizeof(line),
        "Best score: %u%%",
        (unsigned)morse_flipper_progress_best_score(progress));
    canvas_draw_str(canvas, 4, 48, line);
    elements_button_left(canvas, "Stats");
    elements_button_right(canvas, "Lessons");
}

static void morse_flipper_progress_stars_text(uint8_t percent, char* out, size_t out_sz) {
    uint8_t stars = morse_flipper_progress_stars(percent);
    uint8_t i;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';
    for(i = 0U; i < stars && i + 1U < out_sz; i++) {
        out[i] = '*';
        out[i + 1U] = '\0';
    }
}

static void morse_flipper_draw_progress_history_row(
    Canvas* canvas,
    uint8_t y,
    const MorseFlipperProgressHistoryRow* row) {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    char date[12];
    char time[8];
    char score[8];
    char stars[4];
    char lesson[2] = {row->lesson, '\0'};

    if(!morse_flipper_progress_history_row_date(row, &year, &month, &day)) return;
    if(month < 1U || month > 12U) return;

    snprintf(
        date,
        sizeof(date),
        "%02u %s %02u",
        (unsigned)day,
        morse_flipper_progress_months[month - 1U],
        (unsigned)(year % 100U));
    snprintf(time, sizeof(time), "%02u:%02u", (unsigned)row->hour, (unsigned)row->minute);
    snprintf(score, sizeof(score), "%u%%", (unsigned)row->percent);
    morse_flipper_progress_stars_text(row->percent, stars, sizeof(stars));

    canvas_draw_str(canvas, 2, y, lesson);
    canvas_draw_str(canvas, 12, y, date);
    canvas_draw_str(canvas, 64, y, time);
    canvas_draw_str(canvas, 96, y, score);
    canvas_draw_str(canvas, 116, y, stars);
}

static void morse_flipper_draw_progress_history(Canvas* canvas, MorseFlipperApp* app) {
    uint8_t i;
    uint8_t visible = 0U;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 10, "Recent lessons");
    canvas_set_font(canvas, FontSecondary);

    if(app->progress_row_offset < app->progress_row_count)
        visible = (uint8_t)(app->progress_row_count - app->progress_row_offset);
    if(visible > MORSE_FLIPPER_PROGRESS_HISTORY_ROWS)
        visible = MORSE_FLIPPER_PROGRESS_HISTORY_ROWS;

    if(visible == 0U) {
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "No lesson history yet");
    } else {
        for(i = 0U; i < visible; i++) {
            morse_flipper_draw_progress_history_row(
                canvas,
                (uint8_t)(18U + (i * 8U)),
                &app->progress_rows[app->progress_row_offset + i]);
        }
    }

    elements_button_left(canvas, "More");
    elements_button_right(canvas, "Stats");
}

void morse_flipper_draw_progress(Canvas* canvas, MorseFlipperApp* app) {
    if(canvas == NULL || app == NULL) return;

    if(app->view_progress == NULL) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Progress unavailable");
        return;
    }

    if(app->progress_page == MorseFlipperProgressPageTotals) {
        morse_flipper_draw_progress_totals(canvas, app);
    } else if(app->progress_page == MorseFlipperProgressPageHistory) {
        morse_flipper_draw_progress_history(canvas, app);
    } else {
        morse_flipper_draw_progress_stats(canvas, app);
    }
}
