#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MorsePcKeyNone = 0,
    MorsePcKeySpace,
    MorsePcKeyX,
    MorsePcKeyZ,
    MorsePcKeyC,
    MorsePcKeyEnter,
    MorsePcKeyNumEnter,
    MorsePcKeyOpenBracket,
    MorsePcKeyCloseBracket,
    MorsePcKeyLeftCtrl,
    MorsePcKeyRightCtrl,
    MorsePcKeyLeftShift,
    MorsePcKeyRightShift,
    MorsePcKeyPeriod,
    MorsePcKeySlash,
    MorsePcKeyComma,
    MorsePcKeyLess,
    MorsePcKeyGreater,
} MorsePcKey;

uint8_t morse_pc_paddle_preset_count(void);
uint8_t morse_pc_straight_preset_count(void);

const char* morse_pc_paddle_preset_name(uint8_t idx);
const char* morse_pc_straight_preset_name(uint8_t idx);
const char* morse_pc_key_name(uint8_t key);

uint8_t morse_pc_straight_preset_key(uint8_t idx);
uint8_t morse_pc_paddle_preset_key(uint8_t idx, uint8_t note, bool swapped);
