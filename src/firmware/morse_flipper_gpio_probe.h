#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MorseFlipperGpioProbeOk = 0,
    MorseFlipperGpioProbeGroundToDit = 1,
    MorseFlipperGpioProbeGroundToDah = 2,
    MorseFlipperGpioProbeGroundToBoth = 3,
} MorseFlipperGpioProbeState;

uint8_t morse_flipper_gpio_probe_paddle(bool dit_down, bool dah_down);
bool morse_flipper_gpio_probe_any_short(uint8_t state);
bool morse_flipper_gpio_probe_forces_straight(uint8_t state);
bool morse_flipper_gpio_probe_blocks(uint8_t state);
