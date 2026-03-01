#include "morse_flipper_gpio_probe.h"

uint8_t morse_flipper_gpio_probe_paddle(bool dit_down, bool dah_down) {
    if(dit_down && dah_down) {
        return MorseFlipperGpioProbeGroundToBoth;
    }

    if(dit_down) {
        return MorseFlipperGpioProbeGroundToDit;
    }

    if(dah_down) {
        return MorseFlipperGpioProbeGroundToDah;
    }

    return MorseFlipperGpioProbeOk;
}

bool morse_flipper_gpio_probe_any_short(uint8_t state) {
    return state != MorseFlipperGpioProbeOk;
}

bool morse_flipper_gpio_probe_forces_straight(uint8_t state) {
    return state == MorseFlipperGpioProbeGroundToDah;
}

bool morse_flipper_gpio_probe_blocks(uint8_t state) {
    return state == MorseFlipperGpioProbeGroundToDit ||
           state == MorseFlipperGpioProbeGroundToBoth;
}
