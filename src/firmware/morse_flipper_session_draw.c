static char morse_flipper_upper_char(char ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static uint8_t morse_flipper_session_slot_centers(uint8_t size, uint8_t* out) {
    static const uint8_t slot_pos[9][9] = {
        {64},
        {52, 76},
        {40, 64, 88},
        {31, 53, 75, 97},
        {24, 44, 64, 84, 104},
        {19, 37, 55, 73, 91, 109},
        {16, 32, 48, 64, 80, 96, 112},
        {13, 27, 41, 55, 73, 87, 101, 115},
        {11, 24, 37, 50, 64, 78, 91, 104, 117},
    };
    uint8_t i;

    if(size < 1U) size = 1U;
    if(size > 9U) size = 9U;
    if(out == NULL) return size;

    for(i = 0U; i < size; i++) {
        out[i] = slot_pos[size - 1U][i];
    }

    return size;
}

static void morse_flipper_session_answer_text(
    const MorseFlipperApp* app,
    char* out,
    size_t out_sz,
    uint8_t max_chars) {
    char preview = 0;
    size_t wi = 0U;
    size_t i;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';
    if(app == NULL || max_chars == 0U) return;

    for(i = 0U; app->rf_tx_text[i] != '\0' && wi + 1U < out_sz && wi < max_chars; i++) {
        char ch = morse_flipper_upper_char(app->rf_tx_text[i]);

        if(ch == ' ' || ch == '|') continue;
        out[wi++] = ch;
    }

    preview = morse_flipper_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
    if(preview != 0 && preview != ' ' && preview != '|' && wi + 1U < out_sz && wi < max_chars) {
        out[wi++] = preview;
    }

    out[wi] = '\0';
}

static uint8_t morse_flipper_session_prompt_count(const MorseFlipperApp* app) {
    uint8_t size;
    uint8_t shown;

    if(app == NULL) return 0U;

    size = morse_trainer_group_size(&app->trainer);
    if(size < 1U) size = 1U;
    if(size > 9U) size = 9U;

    if(morse_flipper_session_idle_view(app)) return 0U;
    if(!app->trainer_playback_active) return size;

    shown = app->trainer_char_idx;
    if(shown < size) shown++;
    if(shown > size) shown = size;
    return shown;
}

static uint8_t morse_flipper_session_answer_count(const char* answer) {
    uint8_t n = 0U;
    uint8_t i;

    if(answer == NULL) return 0U;

    for(i = 0U; answer[i] != '\0'; i++) {
        if(answer[i] == ' ' || answer[i] == '|') continue;
        n++;
    }

    return n;
}

static void morse_flipper_session_title(const MorseFlipperApp* app, char* out, size_t out_sz) {
    const char* chars;
    size_t i;
    size_t wi = 0U;
    size_t len;
    char lesson_label[32];

    if(out == NULL || out_sz < 2U) return;
    out[0] = '\0';
    if(app == NULL) return;

    if(app->trainer.custom_set_idx == 0U) {
        morse_trainer_lesson_label(
            morse_trainer_lesson(&app->trainer), lesson_label, sizeof(lesson_label));
        snprintf(out, out_sz, "Lesson %s", lesson_label);
        return;
    }

    chars = morse_trainer_charset(&app->trainer);
    len = strlen(chars);
    if(len > 12U) {
        snprintf(out, out_sz, "%s", app->trainer.custom_name[0] ? app->trainer.custom_name : chars);
        return;
    }

    for(i = 0U; chars[i] != '\0' && wi + 2U < out_sz; i++) {
        if(i != 0U && wi + 2U < out_sz) out[wi++] = ' ';
        out[wi++] = morse_flipper_upper_char(chars[i]);
    }
    out[wi] = '\0';
}

static void morse_flipper_draw_session_rows(Canvas* canvas, const MorseFlipperApp* app) {
    uint8_t size = morse_trainer_group_size(&app->trainer);
    uint8_t centers[9];
    char answers[MORSE_TRAINER_GROUP_CAP];
    const char* group = morse_trainer_last_group(&app->trainer);
    uint8_t top_y;
    uint8_t bot_y;
    uint8_t box_y;
    uint8_t box_h;
    size_t ans_len;
    uint8_t prompt_count;
    uint8_t answer_count;
    Font row_font;
    uint8_t i;
    bool idle = morse_flipper_session_idle_view(app);
    bool rep = morse_flipper_session_repeat_active(app);
    bool done;

    if(size < 1U) size = 1U;
    if(size > 9U) size = 9U;

    if(size <= 6U) {
        row_font = FontPrimary;
        top_y = 15U;
        bot_y = 29U;
        box_y = 5U;
        box_h = 11U;
    } else if(size <= 8U) {
        row_font = FontSecondary;
        top_y = 16U;
        bot_y = 28U;
        box_y = 9U;
        box_h = 8U;
    } else {
        row_font = FontKeyboard;
        top_y = 16U;
        bot_y = 27U;
        box_y = 8U;
        box_h = 9U;
    }
    morse_flipper_session_slot_centers(size, centers);
    morse_flipper_session_answer_text(app, answers, sizeof(answers), size);
    ans_len = strlen(answers);
    prompt_count = morse_flipper_session_prompt_count(app);
    answer_count = morse_flipper_session_answer_count(answers);
    done = !idle && app->session_started && !app->trainer_playback_active &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone;

    canvas_set_font(canvas, row_font);

    for(i = 0U; i < size; i++) {
        char mark[2] = {'_', '\0'};
        char bot_mark[2] = {'_', '\0'};
        char q[2] = {'\0', '\0'};
        char a[2] = {'\0', '\0'};
        bool have = i < ans_len;
        bool ok = false;
        bool bad = false;
        uint8_t x;
        uint8_t w;

        if(group[i] != '\0') q[0] = morse_flipper_upper_char(group[i]);
        if(have) a[0] = morse_flipper_upper_char(answers[i]);
        if(q[0] != '\0' && a[0] != '\0' && q[0] == a[0]) ok = true;
        if(q[0] != '\0' && i < answer_count && !ok) bad = true;

        if(i >= prompt_count) continue;

        if(app->trainer_playback_active || idle) {
#if _SHOW_ANSWER_WHILE_TRAINING_LCWO
            if(app->trainer_playback_active && q[0] != '\0') mark[0] = q[0];
#endif
            w = canvas_string_width(canvas, mark);
            x = (uint8_t)(centers[i] - (w / 2U));
            canvas_draw_str(canvas, x, top_y, mark);
            continue;
        }

        if(rep || done) {
#if _SHOW_ANSWER_WHILE_TRAINING_LCWO
            if(q[0] != '\0') mark[0] = q[0];
#else
            if(ok && q[0] != '\0') mark[0] = q[0];
#endif
            w = canvas_string_width(canvas, mark);
            x = (uint8_t)(centers[i] - (w / 2U));
            if(bad || (done && i >= answer_count && q[0] != '\0')) {
                uint8_t bw = canvas_string_width(canvas, q);
                uint8_t bx = (uint8_t)(centers[i] - (bw / 2U) - 1U);

                canvas_draw_box(canvas, bx, box_y, bw + 2U, box_h);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, (uint8_t)(centers[i] - (bw / 2U)), top_y, q);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, x, top_y, mark);
            }

            w = canvas_string_width(canvas, bot_mark);
            x = (uint8_t)(centers[i] - (w / 2U));
            canvas_draw_str(canvas, x, bot_y, bot_mark);
#if _SHOW_ANSWER_WHILE_TRAINING_LCWO
            if(have) {
                w = canvas_string_width(canvas, a);
                x = (uint8_t)(centers[i] - (w / 2U));
                canvas_draw_str(canvas, x, bot_y, a);
            }
#endif
            continue;
        }

        w = canvas_string_width(canvas, mark);
        x = (uint8_t)(centers[i] - (w / 2U));
        canvas_draw_str(canvas, x, top_y, mark);
    }
}

static void morse_flipper_draw_session_bottom(Canvas* canvas, const MorseFlipperApp* app) {
    char lesson_line[48];
    char count_line[16];
    char size_line[16];
    char score_line[16];
    char wait_line[16];
    uint8_t asked = 0U;
    uint8_t total = morse_trainer_session_total(&app->trainer);
    uint8_t fail = morse_trainer_session_fail_count(&app->trainer);
    uint8_t scored = 0U;
    uint8_t good = 0U;
    uint8_t fill = 0U;
    uint8_t x;
    Font title_font = FontSecondary;
    bool wait = false;
    uint32_t now_ms;
    uint32_t left_ms;
    uint16_t secs;

    morse_flipper_session_title(app, lesson_line, sizeof(lesson_line));
    now_ms = furi_get_tick();
    wait = app->session_result_hold && app->session_next_group_at > now_ms;

    if(app->session_started) {
        asked = morse_trainer_session_index(&app->trainer);
        if(app->session_round_pending && asked != 0U) scored = (uint8_t)(asked - 1U);
        else scored = asked;
    }

    if(fail > scored) fail = scored;
    good = (uint8_t)(scored - fail);
    if(total != 0U && asked != 0U) fill = (uint8_t)(((uint16_t)asked * 100U) / total);

    snprintf(
        size_line, sizeof(size_line), "size %u", (unsigned)morse_trainer_group_size(&app->trainer));
    snprintf(count_line, sizeof(count_line), "%u/%u", (unsigned)asked, (unsigned)total);
    snprintf(score_line, sizeof(score_line), "%u/%u", (unsigned)good, (unsigned)asked);

    canvas_set_font(canvas, FontSecondary);
    if(!wait) {
        if(canvas_string_width(canvas, lesson_line) > 128U) {
            title_font = FontKeyboard;
            canvas_set_font(canvas, title_font);
        }

        x = (uint8_t)((128 - canvas_string_width(canvas, lesson_line)) / 2);
        canvas_draw_str(canvas, x, 42, lesson_line);
    } else {
        left_ms = app->session_next_group_at - now_ms;
        secs = (uint16_t)((left_ms + 999U) / 1000U);
        if(secs == 0U) secs = 1U;
        snprintf(wait_line, sizeof(wait_line), "%u", (unsigned)secs);
        canvas_set_font(canvas, FontPrimary);
        x = (uint8_t)((128 - canvas_string_width(canvas, wait_line)) / 2);
        canvas_draw_str(canvas, x, 47, wait_line);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 51, size_line);
    if(!wait) {
        x = (uint8_t)((128 - canvas_string_width(canvas, count_line)) / 2);
        canvas_draw_str(canvas, x, 51, count_line);
    }
    x = (uint8_t)(128 - canvas_string_width(canvas, score_line));
    canvas_draw_str(canvas, x, 51, score_line);

    canvas_draw_frame(canvas, 13, 57, 102, 5);
    if(fill != 0U) canvas_draw_box(canvas, 14, 58, fill, 3);
}
