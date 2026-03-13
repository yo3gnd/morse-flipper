#include "morse_flipper_gpio.h"

static const char* const morse_flipper_gpio_names[MORSE_FLIPPER_GPIO_PIN_COUNT] = {
    "P2",
    "P3",
    "P4",
    "P5",
    "P6",
    "P7",
    "P15",
    "P16",
};

bool morse_flipper_gpio_pin_valid(uint8_t pin_idx) {
    return pin_idx < MORSE_FLIPPER_GPIO_PIN_COUNT;
}

const char* morse_flipper_gpio_name(uint8_t pin_idx) {
    if(pin_idx == MORSE_FLIPPER_GPIO_PIN_NONE) {
        return "None";
    }

    if(!morse_flipper_gpio_pin_valid(pin_idx)) {
        return "?";
    }

    return morse_flipper_gpio_names[pin_idx];
}

uint8_t morse_flipper_gpio_default_dit(void) {
    return MorseFlipperGpioPinP7;
}

uint8_t morse_flipper_gpio_default_dah(void) {
    return MorseFlipperGpioPinP5;
}

uint8_t morse_flipper_gpio_default_ground(void) {
    return MorseFlipperGpioPinP3;
}

MorseFlipperGpioRule morse_flipper_gpio_validate(uint8_t dit, uint8_t dah, uint8_t ground) {
    if(!morse_flipper_gpio_pin_valid(dit) || !morse_flipper_gpio_pin_valid(dah)) {
        return MorseFlipperGpioRuleBadIndex;
    }

    if(ground != MORSE_FLIPPER_GPIO_PIN_NONE && !morse_flipper_gpio_pin_valid(ground)) {
        return MorseFlipperGpioRuleBadIndex;
    }

    if(dit == dah) {
        return MorseFlipperGpioRulePaddlesSharePin;
    }

    if(ground != MORSE_FLIPPER_GPIO_PIN_NONE && (ground == dit || ground == dah)) {
        return MorseFlipperGpioRuleGroundShared;
    }

    return MorseFlipperGpioRuleOk;
}

const char* morse_flipper_gpio_rule_text(MorseFlipperGpioRule rule) {
    switch(rule) {
    case MorseFlipperGpioRulePaddlesSharePin:
        return "Dit and dah must differ.";
    case MorseFlipperGpioRuleGroundShared:
        return "Force gnd must stay unique.";
    case MorseFlipperGpioRuleBadIndex:
        return "That GPIO choice is not valid.";
    default:
        return "ok";
    }
}
