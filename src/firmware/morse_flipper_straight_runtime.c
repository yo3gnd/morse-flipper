#include "morse_flipper_app_i.h"

void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->straight_playback_active = false;
    app->straight_playback_mark = false;
    app->straight_started = false;
    app->straight_wait_answer = false;
    app->straight_done = false;
    app->straight_key_down = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = 0U;
    app->straight_wait_started_at = 0U;
    app->straight_last_input_at = 0U;
    app->straight_mark_started_at = 0U;
    app->straight_next_draw_s = 0xFFU;
    app->straight_trainer.active = false;
    app->straight_trainer.answer[0] = '\0';
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_update_sidetone(app);
    morse_flipper_clear_button_keying(app, now_ms);
}

void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!app->straight_started) morse_flipper_reset_straight_session(app);
    morse_flipper_reset_straight_state(app, now_ms);
    morse_flipper_straight_trainer_start(
        &app->straight_trainer,
        MORSE_FLIPPER_STRAIGHT_CHARSET,
        morse_flipper_current_straight_dit_ms(app));
    app->straight_started = true;
    app->straight_playback_active = true;
    app->straight_playback_mark = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = now_ms;
    morse_flipper_view_dirty(app);
}

static uint8_t morse_flipper_straight_hist_idx(char ch) {
    if(ch >= 'A' && ch <= 'Z') return (uint8_t)(ch - 'A');
    if(ch >= '0' && ch <= '9') return (uint8_t)(26U + (ch - '0'));
    return 0xFFU;
}

static uint16_t morse_flipper_straight_attempt_sum(const MorseFlipperApp* app) {
    uint16_t sum = 0U;

    if(app == NULL) return 0U;
    if(morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] == '\0') return 0U;
    if(!morse_flipper_straight_trainer_symbol_count_match(&app->straight_trainer)) return 0U;
    sum += morse_flipper_straight_trainer_worst_space_score(&app->straight_trainer);
    sum += morse_flipper_straight_trainer_worst_dit_score(&app->straight_trainer);
    sum += morse_flipper_straight_trainer_worst_dah_score(&app->straight_trainer);
    return sum;
}

static void morse_flipper_straight_refresh_worst(MorseFlipperApp* app) {
    uint8_t pick[5] = {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU};
    size_t at = 0U;

    if(app == NULL) return;

    for(uint8_t j = 0U; j < 5U; j++) {
        uint8_t best = 0xFFU;
        uint16_t best_avg = 0xFFFFU;

        for(uint8_t i = 0U; i < 36U; i++) {
            bool used = false;
            uint16_t avg;

            if(app->straight_hist_cnt[i] < 3U) continue;
            for(uint8_t k = 0U; k < j; k++)
                if(pick[k] == i) used = true;
            if(used) continue;

            avg = (uint16_t)(app->straight_hist_sum[i] / app->straight_hist_cnt[i]);
            if(avg < best_avg) {
                best_avg = avg;
                best = i;
            }
        }

        pick[j] = best;
    }

    memset(app->straight_worst_line, 0, sizeof(app->straight_worst_line));
    memcpy(app->straight_worst_line, "Wo:", 3U);
    at = 3U;
    for(uint8_t j = 0U; j < 5U; j++) {
        char ch;

        if(pick[j] == 0xFFU) break;
        ch = pick[j] < 26U ? (char)('A' + pick[j]) : (char)('0' + (pick[j] - 26U));
        if(at + 3U >= sizeof(app->straight_worst_line)) break;
        app->straight_worst_line[at++] = ' ';
        app->straight_worst_line[at++] = ch;
    }

    if(at == 3U) snprintf(app->straight_worst_line, sizeof(app->straight_worst_line), "Wo: -");
}

void morse_flipper_reset_straight_session(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->straight_session_total = 0U;
    app->straight_session_good = 0U;
    memset(app->straight_hist_cnt, 0, sizeof(app->straight_hist_cnt));
    memset(app->straight_hist_sum, 0, sizeof(app->straight_hist_sum));
    snprintf(app->straight_worst_line, sizeof(app->straight_worst_line), "Wo: -");
}

void morse_flipper_note_straight_session(MorseFlipperApp* app) {
    uint8_t idx;

    if(app == NULL) return;

    app->straight_session_total++;
    if(strcmp(
           morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
           morse_flipper_straight_trainer_answer(&app->straight_trainer)) == 0)
        app->straight_session_good++;

    idx = morse_flipper_straight_hist_idx(
        morse_flipper_straight_trainer_target_char(&app->straight_trainer));
    if(idx != 0xFFU) {
        if(app->straight_hist_cnt[idx] != 0xFFU) app->straight_hist_cnt[idx]++;
        app->straight_hist_sum[idx] += morse_flipper_straight_attempt_sum(app);
    }

    morse_flipper_straight_refresh_worst(app);
}

bool morse_flipper_straight_countdown_active(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    return app->screen == MorseFlipperScreenStraight && app->straight_started &&
           !app->straight_playback_active && !app->straight_wait_answer && !app->straight_done &&
           app->straight_next_at != 0U;
}

void morse_flipper_start_straight_countdown(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!app->straight_started) morse_flipper_reset_straight_session(app);
    morse_flipper_reset_straight_state(app, now_ms);
    app->straight_started = true;
    app->straight_next_at = now_ms + ((uint32_t)app->straight_next_delay_s * 1000U);
    app->straight_next_draw_s = 0xFFU;
    morse_flipper_view_dirty(app);
}

void morse_flipper_finish_straight_round(MorseFlipperApp* app, uint32_t now_ms) {
    const char* answer;
    uint16_t err_ms;
    uint8_t drift_pct;
    bool hard_fail;

    if(app == NULL) return;

    app->straight_wait_answer = false;
    app->straight_done = true;
    app->straight_trainer.active = false;
    app->straight_next_at = now_ms + ((uint32_t)app->straight_next_delay_s * 1000U);
    app->straight_next_draw_s = 0xFFU;
    morse_flipper_note_straight_session(app);
    answer = morse_flipper_straight_trainer_answer(&app->straight_trainer);
    hard_fail = answer[0] == '\0' ||
                !morse_flipper_straight_trainer_symbol_count_match(&app->straight_trainer);
    if(hard_fail) {
        err_ms = 0xFFFFU;
        drift_pct = 100U;
    } else {
        err_ms = morse_flipper_straight_trainer_average_mark_error_ms(&app->straight_trainer);
        drift_pct = morse_flipper_straight_trainer_average_drift_percent(&app->straight_trainer);
    }
    morse_trainer_note_straight_attempt(
        &app->straight_stats,
        morse_flipper_straight_trainer_target_char(&app->straight_trainer),
        err_ms,
        drift_pct,
        morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
        answer);
    morse_flipper_view_dirty(app);
}

void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t dit_ms;
    uint32_t left_ms;
    uint8_t marks;
    uint8_t mark_units;
    uint8_t left_s;

    if(app == NULL) return;

    if(morse_flipper_straight_countdown_active(app)) {
        if(now_ms >= app->straight_next_at) {
            morse_flipper_start_straight_round(app, now_ms);
        } else {
            left_ms = app->straight_next_at - now_ms;
            left_s = (uint8_t)((left_ms + 999U) / 1000U);
            if(left_s != app->straight_next_draw_s) {
                app->straight_next_draw_s = left_s;
                morse_flipper_view_dirty(app);
            }
        }
        return;
    }

    if(app->straight_playback_active && now_ms >= app->straight_next_at) {
        dit_ms = morse_flipper_current_straight_dit_ms(app);
        marks = morse_flipper_straight_trainer_target_symbol_count(&app->straight_trainer);

        if(!app->straight_playback_mark) {
            if(app->straight_mark_idx >= marks) {
                app->straight_playback_active = false;
                app->straight_wait_answer = true;
                app->straight_wait_started_at = now_ms;
                app->straight_next_at = 0U;
                morse_flipper_update_sidetone(app);
                morse_flipper_view_dirty(app);
                return;
            }

            app->straight_playback_mark = true;
            mark_units = morse_flipper_straight_trainer_target_mark_units(
                &app->straight_trainer, app->straight_mark_idx);
            app->straight_next_at = now_ms + (dit_ms * mark_units);
            morse_flipper_update_sidetone(app);
            morse_flipper_view_dirty(app);
            return;
        }

        app->straight_playback_mark = false;
        app->straight_mark_idx++;
        if(app->straight_mark_idx >= marks) {
            app->straight_playback_active = false;
            app->straight_wait_answer = true;
            app->straight_wait_started_at = now_ms;
            app->straight_next_at = 0U;
        } else {
            app->straight_next_at = now_ms + dit_ms;
        }
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
    }

    if(app->straight_wait_answer &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] == '\0') {
        uint32_t timeout_ms = (uint32_t)app->straight_answer_timeout_s * 1000U;

        if(app->straight_wait_started_at != 0U &&
           now_ms - app->straight_wait_started_at >= timeout_ms) {
            morse_flipper_finish_straight_round(app, now_ms);
            return;
        }
    }

    if(app->straight_wait_answer &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
        uint32_t settle = morse_flipper_straight_answer_settle_ms(app);

        if(settle != 0U && !app->straight_key_down &&
           now_ms - app->straight_last_input_at >= settle) {
            morse_flipper_finish_straight_round(app, now_ms);
            return;
        }
    }

    if(app->straight_done && app->straight_next_at != 0U) {
        if(now_ms >= app->straight_next_at) {
            morse_flipper_start_straight_round(app, now_ms);
        } else {
            left_ms = app->straight_next_at - now_ms;
            left_s = (uint8_t)((left_ms + 999U) / 1000U);
            if(left_s != app->straight_next_draw_s) {
                app->straight_next_draw_s = left_s;
                morse_flipper_view_dirty(app);
            }
        }
    }
}
