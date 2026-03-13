#include "trainer.h"
#include "trainer_files.h"
#include "morse_flipper_paths.h"

#ifdef MORSE_FLIPPER_FAP
#include <storage/storage.h>
#include <furi.h>
#else
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#endif

#include <string.h>

static const char* morse_trainer_custom_path_value = MORSE_FLIPPER_CUSTOM_CHARS_PATH;
static const char* morse_trainer_straight_stats_path_value = MORSE_FLIPPER_STRAIGHT_STATS_PATH;
static const char* morse_trainer_custom_defaults =
    "numbers=0123456789\n"
    "dits=EISH5\n"
    "more dits=EISH5AUVNDB\n";

const char* morse_trainer_custom_chars_path(void) {
    return morse_trainer_custom_path_value;
}

const char* morse_trainer_straight_stats_path(void) {
    return morse_trainer_straight_stats_path_value;
}

#ifdef MORSE_FLIPPER_FAP
static bool morse_trainer_read_custom_text(char* buf, size_t buf_sz) {
    Storage* storage;
    File* file;
    uint16_t got = 0U;

    if(buf == NULL || buf_sz == 0U) {
        return false;
    }

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    storage_common_mkdir(storage, "/ext/ham");

    if(!storage_file_open( file, morse_trainer_custom_path_value, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_open( file, morse_trainer_custom_path_value, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, morse_trainer_custom_defaults, strlen(morse_trainer_custom_defaults));
        }
        storage_file_close(file);
        storage_file_open(file, morse_trainer_custom_path_value, FSAM_READ, FSOM_OPEN_EXISTING);
    }

    got = storage_file_read(file, buf, buf_sz - 1U);
    buf[got] = '\0';
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return got != 0U;
}
#else
static bool morse_trainer_read_custom_text(char* buf, size_t buf_sz) {
    FILE* f;
    size_t got;

    if(buf == NULL || buf_sz == 0U) {
        return false;
    }

    f = fopen(morse_trainer_custom_path_value, "rb");
    if(f == NULL) {
        mkdir("/ext", 0777);
        mkdir("/ext/ham", 0777);
        f = fopen(morse_trainer_custom_path_value, "wb");
        if(f != NULL) {
            fwrite(morse_trainer_custom_defaults, 1, strlen(morse_trainer_custom_defaults), f);
            fclose(f);
        }
        f = fopen(morse_trainer_custom_path_value, "rb");
    }

    if(f == NULL) {
        return false;
    }

    got = fread(buf, 1, buf_sz - 1U, f);
    buf[got] = '\0';
    fclose(f);
    return got != 0U;
}
#endif

bool morse_trainer_load_custom_sets(MorseTrainerCustomSets* sets) {
    char buf[512];
    char* line;
    char* next;
    char* eq;

    if(sets == NULL) {
        return false;
    }

    memset(sets, 0, sizeof(*sets));
    if(!morse_trainer_read_custom_text(buf, sizeof(buf))) {
        return false;
    }

    line = buf;
    while(line != NULL && *line != '\0' && sets->count < MORSE_TRAINER_CUSTOM_SET_CAP) {
        next = strchr(line, '\n');
        if(next != NULL) {
            *next++ = '\0';
        }

        eq = strchr(line, '=');
        if(eq != NULL) {
            *eq++ = '\0';
            if(line[0] != '\0' && eq[0] != '\0') {
                strncpy(sets->sets[sets->count].name, line, MORSE_TRAINER_CUSTOM_NAME_CAP - 1U);
                strncpy( sets->sets[sets->count].chars, eq, MORSE_TRAINER_CHARSET_CAP - 1U);
                sets->count++;
            }
        }

        line = next;
    }

    return sets->count != 0U;
}

static void morse_trainer_clear_straight_stats(MorseTrainerStraightStats* stats) {
    if(stats == NULL) return;
    memset(stats, 0, sizeof(*stats));
}

static void morse_trainer_note_straight_best_worst(
    MorseTrainerStraightStats* stats,
    char target_char,
    uint16_t error_ms,
    uint8_t drift_pct) {
    if(stats == NULL) return;

    if(!stats->have_best || error_ms < stats->best_error_ms) {
        stats->have_best = true;
        stats->best_char = target_char;
        stats->best_error_ms = error_ms;
        stats->best_drift_pct = drift_pct;
    }

    if(!stats->have_worst || error_ms >= stats->worst_error_ms) {
        stats->have_worst = true;
        stats->worst_char = target_char;
        stats->worst_error_ms = error_ms;
        stats->worst_drift_pct = drift_pct;
    }
}

bool morse_trainer_load_straight_stats(MorseTrainerStraightStats* stats) {
    char buf[2048];
    char* line;
    char* next;
    char ch;
    unsigned err;
    unsigned drift;

    if(stats == NULL) return false;
    morse_trainer_clear_straight_stats(stats);

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint16_t got = 0U;

    if(storage_file_open( file, morse_trainer_straight_stats_path_value, FSAM_READ, FSOM_OPEN_EXISTING)) {
        got = storage_file_read(file, buf, sizeof(buf) - 1U);
    }
    buf[got] = '\0';
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
#else
    FILE* f = fopen(morse_trainer_straight_stats_path_value, "rb");
    size_t got = 0U;

    if(f != NULL) {
        got = fread(buf, 1, sizeof(buf) - 1U, f);
        fclose(f);
    }
    buf[got] = '\0';
#endif

    line = buf;
    while(line != NULL && *line != '\0') {
        next = strchr(line, '\n');
        if(next != NULL) {
            *next++ = '\0';
        }

        if(sscanf(line, "%c err=%u drift=%u", &ch, &err, &drift) == 3) {
            morse_trainer_note_straight_best_worst(stats, ch, (uint16_t)err, (uint8_t)drift);
            strncpy(stats->last_line, line, sizeof(stats->last_line) - 1U);
            stats->last_line[sizeof(stats->last_line) - 1U] = '\0';
        }

        line = next;
    }

    return stats->have_best || stats->have_worst;
}

bool morse_trainer_note_straight_attempt(
    MorseTrainerStraightStats* stats,
    char target_char,
    uint16_t error_ms,
    uint8_t drift_pct,
    const char* target,
    const char* answer) {
    char line[160];

    snprintf(
        line,
        sizeof(line),
        "%c err=%u drift=%u target=%s answer=%s\n",
        target_char ? target_char : '?',
        (unsigned)error_ms,
        (unsigned)drift_pct,
        target ? target : "",
        answer ? answer : "");

#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok;

    storage_common_mkdir(storage, "/ext/ham");
    ok = storage_file_open( file, morse_trainer_straight_stats_path_value, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) {
        storage_file_write(file, line, strlen(line));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!ok) return false;
#else
    FILE* f = fopen(morse_trainer_straight_stats_path_value, "ab");

    if(f == NULL) {
        mkdir("ext", 0777);
        mkdir("ext/ham", 0777);
        f = fopen(morse_trainer_straight_stats_path_value, "ab");
    }
    if(f == NULL) return false;
    fwrite(line, 1, strlen(line), f);
    fclose(f);
#endif

    if(stats != NULL) {
        strncpy(stats->last_line, line, sizeof(stats->last_line) - 1U);
        stats->last_line[sizeof(stats->last_line) - 1U] = '\0';
        if(stats->last_line[0] != '\0') {
            size_t len = strlen(stats->last_line);
            if(len != 0U && stats->last_line[len - 1U] == '\n') stats->last_line[len - 1U] = '\0';
        }
        morse_trainer_note_straight_best_worst(stats, target_char, error_ms, drift_pct);
    }

    return true;
}
