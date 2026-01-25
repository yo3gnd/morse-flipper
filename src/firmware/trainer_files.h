#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "trainer.h"

#define MORSE_TRAINER_CUSTOM_SET_CAP   8U
#define MORSE_TRAINER_CUSTOM_NAME_CAP  24U

typedef struct {
    char name[MORSE_TRAINER_CUSTOM_NAME_CAP];
    char chars[MORSE_TRAINER_CHARSET_CAP];
} MorseTrainerCustomSet;

typedef struct {
    uint8_t count;
    MorseTrainerCustomSet sets[MORSE_TRAINER_CUSTOM_SET_CAP];
} MorseTrainerCustomSets;

typedef struct {
    bool have_best;
    bool have_worst;
    char best_char;
    char worst_char;
    uint16_t best_error_ms;
    uint16_t worst_error_ms;
    uint8_t best_drift_pct;
    uint8_t worst_drift_pct;
    char last_line[96];
} MorseTrainerStraightStats;

const char* morse_trainer_custom_chars_path(void);
bool morse_trainer_load_custom_sets(MorseTrainerCustomSets* sets);
const char* morse_trainer_straight_stats_path(void);
bool morse_trainer_load_straight_stats(MorseTrainerStraightStats* stats);
bool morse_trainer_note_straight_attempt(
    MorseTrainerStraightStats* stats,
    char target_char,
    uint16_t error_ms,
    uint8_t drift_pct,
    const char* target,
    const char* answer);
