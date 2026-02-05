#pragma once

#include <stdint.h>

#define MORSE_FLIPPER_RUN_HISTORY_ROWS 3U
#define MORSE_FLIPPER_RUN_HISTORY_COLS 22U

typedef struct {
    char line[MORSE_FLIPPER_RUN_HISTORY_ROWS][MORSE_FLIPPER_RUN_HISTORY_COLS];
    uint8_t current_row;
    uint8_t used_rows;
} MorseFlipperRunHistory;

void morse_flipper_run_history_reset(MorseFlipperRunHistory* history);
void morse_flipper_run_history_append(MorseFlipperRunHistory* history, const char* text);
