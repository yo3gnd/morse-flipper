#pragma once

#include <stdint.h>

#define MORSE_FLIPPER_RUN_HISTORY_ROWS 3U
#define MORSE_FLIPPER_RUN_HISTORY_TEXT 128U

typedef struct {
    char text[MORSE_FLIPPER_RUN_HISTORY_TEXT];
} MorseFlipperRunHistory;

void morse_flipper_run_history_reset(MorseFlipperRunHistory* history);
void morse_flipper_run_history_append(MorseFlipperRunHistory* history, const char* text);
const char* morse_flipper_run_history_text(const MorseFlipperRunHistory* history);
