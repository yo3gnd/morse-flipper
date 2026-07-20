#define _POSIX_C_SOURCE 200809L

#include "morse_flipper_progress.h"
#include "morse_flipper_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned g_checks;

#define CHECK(expr)                                                      \
    do {                                                                 \
        g_checks++;                                                      \
        if(!(expr)) {                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            exit(1);                                                     \
        }                                                                \
    } while(0)

static uint16_t checked_day(uint16_t year, uint8_t month, uint8_t day) {
    uint16_t out = MORSE_FLIPPER_PROGRESS_DAY_NONE;

    CHECK(morse_flipper_progress_date_to_day(year, month, day, &out));
    CHECK(out != MORSE_FLIPPER_PROGRESS_DAY_NONE);
    return out;
}

static void write_bytes(const char* path, const char* data, size_t len) {
    FILE* f = fopen(path, "wb");

    CHECK(f != NULL);
    CHECK(fwrite(data, 1U, len, f) == len);
    CHECK(fclose(f) == 0);
}

static void test_attempt_rules(void) {
    MorseFlipperProgress progress;
    uint16_t day1 = checked_day(2026, 7, 20);
    uint16_t day2 = checked_day(2026, 7, 21);
    uint16_t day4 = checked_day(2026, 7, 23);

    morse_flipper_progress_reset(&progress);
    CHECK(sizeof(progress) == MORSE_FLIPPER_PROGRESS_SIZE);
    CHECK(morse_flipper_progress_stars(69U) == 0U);
    CHECK(morse_flipper_progress_stars(70U) == 1U);
    CHECK(morse_flipper_progress_stars(85U) == 2U);
    CHECK(morse_flipper_progress_stars(95U) == 3U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day1, 5U, 69U);
    CHECK(progress.total_attempts == 1U);
    CHECK(progress.streak_days == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 1U);
    CHECK(progress.last_streak_day == day1);
    CHECK(progress.last_stats_day == day1);
    CHECK(progress.lesson_attempts[5] == 1U);
    CHECK(progress.lesson_best[5] == 69U);
    CHECK(progress.lesson_last[5] == 69U);

    morse_flipper_progress_note_custom_attempt(&progress, true, day1);
    CHECK(progress.total_attempts == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 1U);
    CHECK(progress.lesson_attempts[5] == 1U);
    CHECK(progress.streak_days == 1U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day1, 5U, 95U);
    CHECK(progress.total_attempts == 2U);
    CHECK(progress.today_attempts == 2U);
    CHECK(progress.daily_record == 2U);
    CHECK(progress.lesson_attempts[5] == 2U);
    CHECK(progress.lesson_best[5] == 95U);
    CHECK(progress.lesson_last[5] == 95U);

    morse_flipper_progress_note_custom_attempt(&progress, true, day2);
    CHECK(progress.streak_days == 2U);
    CHECK(progress.last_streak_day == day2);
    CHECK(progress.today_attempts == 2U);
    CHECK(progress.last_stats_day == day1);
    CHECK(progress.total_attempts == 2U);

    morse_flipper_progress_note_standard_attempt(&progress, true, day4, 6U, 101U);
    CHECK(progress.streak_days == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 2U);
    CHECK(progress.total_attempts == 3U);
    CHECK(progress.lesson_best[6] == 100U);

    morse_flipper_progress_note_standard_attempt(
        &progress, false, MORSE_FLIPPER_PROGRESS_DAY_NONE, 6U, 88U);
    CHECK(progress.total_attempts == 4U);
    CHECK(progress.streak_days == 1U);
    CHECK(progress.today_attempts == 1U);
    CHECK(progress.daily_record == 2U);
    CHECK(progress.lesson_best[6] == 100U);
    CHECK(progress.lesson_last[6] == 88U);
}

static void test_weak_letters_are_bounded_and_recent(void) {
    MorseFlipperProgress progress;
    char weak[12];
    char high_byte[2] = {(char)0xFF, '\0'};

    morse_flipper_progress_reset(&progress);
    morse_flipper_progress_note_weak_group(&progress, "KMR", "R", "K");
    morse_flipper_progress_top_weak(&progress, "KMR", weak, sizeof(weak));
    CHECK(strchr(weak, 'R') != NULL);
    CHECK(strchr(weak, 'K') != NULL);
    CHECK(progress.weak_error[(uint8_t)'R'] >= 32U);
    CHECK(progress.weak_error[(uint8_t)'K'] >= 16U);

    morse_flipper_progress_note_weak_group(&progress, high_byte, high_byte, high_byte);

    for(uint8_t i = 0U; i < 6U; i++)
        morse_flipper_progress_note_weak_group(&progress, "KMR", "M", "R");
    morse_flipper_progress_top_weak(&progress, "KMR", weak, sizeof(weak));
    CHECK(strchr(weak, 'M') != NULL);
    CHECK(strchr(weak, 'R') != NULL);
}

static void test_persistence_shape_rules(void) {
    MorseFlipperProgress progress;
    MorseFlipperProgress loaded;

    CHECK(morse_flipper_progress_load(&loaded));
    CHECK(morse_flipper_progress_valid(&loaded));
    CHECK(loaded.total_attempts == 0U);

    morse_flipper_progress_reset(&progress);
    morse_flipper_progress_note_standard_attempt(
        &progress, false, MORSE_FLIPPER_PROGRESS_DAY_NONE, 5U, 91U);
    CHECK(morse_flipper_progress_save(&progress));
    memset(&loaded, 0xA5, sizeof(loaded));
    CHECK(morse_flipper_progress_load(&loaded));
    CHECK(loaded.total_attempts == 1U);
    CHECK(loaded.lesson_best[5] == 91U);

    write_bytes(MORSE_FLIPPER_PROGRESS_PATH, "short", 5U);
    memset(&loaded, 0xA5, sizeof(loaded));
    CHECK(morse_flipper_progress_load(&loaded));
    CHECK(morse_flipper_progress_valid(&loaded));
    CHECK(loaded.total_attempts == 0U);
}

static void test_history_format_and_bidirectional_loading(void) {
    MorseFlipperProgressHistoryCursor cursor;
    MorseFlipperProgressHistoryRow rows[5];
    MorseFlipperProgressHistoryRow older;
    MorseFlipperProgressHistoryRow newer;
    uint16_t today = checked_day(2026, 7, 21);
    uint16_t day21 = checked_day(2026, 7, 21);
    char path[128];
    FILE* f;
    char file_buf[32];

    CHECK(morse_flipper_progress_append_history(2026, 7, 19, 17, 0, 5, 80));
    CHECK(morse_flipper_progress_append_history(2026, 7, 20, 10, 0, 6, 70));
    CHECK(morse_flipper_progress_append_history(2026, 7, 20, 10, 10, 6, 85));
    CHECK(morse_flipper_progress_append_history(2026, 7, 20, 10, 20, 6, 95));
    CHECK(morse_flipper_progress_append_history(2026, 7, 21, 9, 0, 7, 90));
    CHECK(morse_flipper_progress_append_history(2026, 7, 21, 9, 10, 7, 100));

    CHECK(morse_flipper_progress_history_path(path, sizeof(path), day21));
    f = fopen(path, "rb");
    CHECK(f != NULL);
    CHECK(fread(file_buf, 1U, sizeof(file_buf), f) == 24U);
    CHECK(fclose(f) == 0);
    CHECK(memcmp(file_buf, "0900 07 090\n0910 07 100\n", 24U) == 0);

    morse_flipper_progress_history_reset(&cursor, today);
    CHECK(morse_flipper_progress_history_load_more(&cursor, rows, 5U) == 5U);
    CHECK(rows[0].lesson_idx == 7U && rows[0].hour == 9U && rows[0].minute == 10U);
    CHECK(rows[0].percent == 100U);
    CHECK(rows[1].lesson_idx == 7U && rows[1].hour == 9U && rows[1].minute == 0U);
    CHECK(rows[2].lesson_idx == 6U && rows[2].hour == 10U && rows[2].minute == 20U);
    CHECK(rows[3].lesson_idx == 6U && rows[3].hour == 10U && rows[3].minute == 10U);
    CHECK(rows[4].lesson_idx == 6U && rows[4].hour == 10U && rows[4].minute == 0U);
    CHECK(!cursor.exhausted);

    CHECK(morse_flipper_progress_history_load_more(&cursor, &older, 1U) == 1U);
    CHECK(older.lesson_idx == 5U);
    CHECK(older.hour == 17U);

    CHECK(morse_flipper_progress_history_load_newer(&older, today, &newer));
    CHECK(newer.lesson_idx == 6U && newer.hour == 10U && newer.minute == 0U);
    CHECK(morse_flipper_progress_history_load_newer(&rows[3], today, &newer));
    CHECK(newer.lesson_idx == 6U && newer.hour == 10U && newer.minute == 20U);
    CHECK(!morse_flipper_progress_history_load_newer(&rows[0], today, &newer));
}

int main(void) {
    char tmp[] = "/tmp/morse_progress_test_XXXXXX";

    CHECK(mkdtemp(tmp) != NULL);
    CHECK(chdir(tmp) == 0);

    test_attempt_rules();
    test_weak_letters_are_bounded_and_recent();
    test_persistence_shape_rules();
    test_history_format_and_bidirectional_loading();

    printf("test_progress: %u checks passed\n", g_checks);
    return 0;
}
