#include "keyer.h"

#define MORSE_KEYER_DIT_TICKS 5
#define MORSE_KEYER_DAH_TICKS 15
#define MORSE_KEYER_GAP_TICKS 5

typedef enum {
    MorseKeyerPhaseIdle = 0,
    MorseKeyerPhaseMark = 1,
    MorseKeyerPhaseGap = 2,
} MorseKeyerPhase;

typedef enum {
    MorseKeyerElementNone = 0,
    MorseKeyerElementDit = 1,
    MorseKeyerElementDah = 2,
} MorseKeyerElement;

static void morse_keyer_start_element(MorseKeyer* keyer, uint8_t element) {
    keyer->phase = MorseKeyerPhaseMark;
    keyer->current_element = element;
    keyer->last_element = element;
    keyer->tone_on = true;

    if(element == MorseKeyerElementDah) {
        keyer->ticks_left = MORSE_KEYER_DAH_TICKS;
    } else {
        keyer->ticks_left = MORSE_KEYER_DIT_TICKS;
    }
}

static uint8_t morse_keyer_opposite_element(uint8_t element) {
    if(element == MorseKeyerElementDah) return MorseKeyerElementDit;
    return MorseKeyerElementDah;
}

static void morse_keyer_tick_mark(MorseKeyer* keyer) {
    if(keyer->ticks_left > 0) {
        keyer->ticks_left--;
    }

    if(keyer->ticks_left == 0) {
        keyer->phase = MorseKeyerPhaseGap;
        keyer->tone_on = false;
        keyer->ticks_left = MORSE_KEYER_GAP_TICKS;
    }
}

static void morse_keyer_tick_gap(MorseKeyer* keyer) {
    if(keyer->ticks_left > 0) {
        keyer->ticks_left--;
    }

    if(keyer->ticks_left == 0) {
        keyer->phase = MorseKeyerPhaseIdle;
        keyer->current_element = MorseKeyerElementNone;
    }
}

void morse_keyer_init(MorseKeyer* keyer) {
    keyer->mode = MorseKeyerModeStraight;
    keyer->dit_down = false;
    keyer->dah_down = false;
    keyer->prev_dit_down = false;
    keyer->prev_dah_down = false;
    keyer->tone_on = false;
    keyer->queued_extra = false;
    keyer->phase = MorseKeyerPhaseIdle;
    keyer->current_element = MorseKeyerElementNone;
    keyer->last_element = MorseKeyerElementNone;
    keyer->last_press = MorseKeyerElementNone;
    keyer->ticks_left = 0;
}

void morse_keyer_reset(MorseKeyer* keyer) {
    keyer->dit_down = false;
    keyer->dah_down = false;
    keyer->prev_dit_down = false;
    keyer->prev_dah_down = false;
    keyer->tone_on = false;
    keyer->queued_extra = false;
    keyer->phase = MorseKeyerPhaseIdle;
    keyer->current_element = MorseKeyerElementNone;
    keyer->last_element = MorseKeyerElementNone;
    keyer->last_press = MorseKeyerElementNone;
    keyer->ticks_left = 0;
}

void morse_keyer_set_mode(MorseKeyer* keyer, uint8_t mode) {
    if(mode < MorseKeyerModeStraight || mode > MorseKeyerModeKeyahead) {
        mode = MorseKeyerModeStraight;
    }

    keyer->mode = mode;
}

void morse_keyer_set_paddles(MorseKeyer* keyer, bool dit_down, bool dah_down) {
    keyer->dit_down = dit_down;
    keyer->dah_down = dah_down;
}

static void morse_keyer_tick_bug(MorseKeyer* keyer) {
    if(keyer->phase == MorseKeyerPhaseMark) {
        morse_keyer_tick_mark(keyer);
        return;
    }

    if(keyer->phase == MorseKeyerPhaseGap) {
        morse_keyer_tick_gap(keyer);
        return;
    }

    if(keyer->dit_down) {
        morse_keyer_start_element(keyer, MorseKeyerElementDit);
    } else if(keyer->dah_down) {
        keyer->tone_on = true;
    } else {
        keyer->tone_on = false;
    }
}

static void morse_keyer_tick_elbug(MorseKeyer* keyer) {
    if(keyer->phase == MorseKeyerPhaseMark) {
        morse_keyer_tick_mark(keyer);
        return;
    }

    if(keyer->phase == MorseKeyerPhaseGap) {
        morse_keyer_tick_gap(keyer);
        return;
    }

    if(keyer->dit_down) {
        morse_keyer_start_element(keyer, MorseKeyerElementDit);
    } else if(keyer->dah_down) {
        morse_keyer_start_element(keyer, MorseKeyerElementDah);
    } else {
        keyer->tone_on = false;
    }
}

static void morse_keyer_tick_single_dot(MorseKeyer* keyer) {
    bool dit_press = keyer->dit_down && !keyer->prev_dit_down;

    if(keyer->phase == MorseKeyerPhaseMark) {
        morse_keyer_tick_mark(keyer);
        return;
    }

    if(keyer->phase == MorseKeyerPhaseGap) {
        morse_keyer_tick_gap(keyer);
        return;
    }

    if(dit_press) {
        morse_keyer_start_element(keyer, MorseKeyerElementDit);
    } else if(keyer->dah_down) {
        keyer->tone_on = true;
    } else {
        keyer->tone_on = false;
    }
}

static uint8_t morse_keyer_pick_iambic_element(MorseKeyer* keyer) {
    if(keyer->dit_down && keyer->dah_down) {
        if(keyer->last_element == MorseKeyerElementDit) {
            return MorseKeyerElementDah;
        } else if(keyer->last_element == MorseKeyerElementDah) {
            return MorseKeyerElementDit;
        } else if(keyer->last_press == MorseKeyerElementDah) {
            return MorseKeyerElementDah;
        } else {
            return MorseKeyerElementDit;
        }
    }

    if(keyer->dit_down) return MorseKeyerElementDit;
    if(keyer->dah_down) return MorseKeyerElementDah;
    return MorseKeyerElementNone;
}

static void morse_keyer_tick_plain_iambic(MorseKeyer* keyer) {
    if(keyer->phase == MorseKeyerPhaseMark) {
        morse_keyer_tick_mark(keyer);
        return;
    }

    if(keyer->phase == MorseKeyerPhaseGap) {
        morse_keyer_tick_gap(keyer);
        return;
    }

    uint8_t element = morse_keyer_pick_iambic_element(keyer);

    if(element != MorseKeyerElementNone) {
        morse_keyer_start_element(keyer, element);
    } else {
        keyer->tone_on = false;
    }
}

static void morse_keyer_tick_iambic_a(MorseKeyer* keyer) {
    if(keyer->phase == MorseKeyerPhaseMark) {
        if(keyer->dit_down && keyer->dah_down) {
            keyer->queued_extra = true;
        }

        morse_keyer_tick_mark(keyer);
        return;
    }

    if(keyer->phase == MorseKeyerPhaseGap) {
        morse_keyer_tick_gap(keyer);
        return;
    }

    if(keyer->queued_extra) {
        keyer->queued_extra = false;
        morse_keyer_start_element(keyer, morse_keyer_opposite_element(keyer->last_element));
        return;
    }

    uint8_t element = morse_keyer_pick_iambic_element(keyer);

    if(element != MorseKeyerElementNone) {
        morse_keyer_start_element(keyer, element);
    } else {
        keyer->tone_on = false;
    }
}

void morse_keyer_tick(MorseKeyer* keyer) {
    if(keyer->dit_down && !keyer->prev_dit_down) {
        keyer->last_press = MorseKeyerElementDit;
    }

    if(keyer->dah_down && !keyer->prev_dah_down) {
        keyer->last_press = MorseKeyerElementDah;
    }

    switch(keyer->mode) {
    case MorseKeyerModeIambicA:
        morse_keyer_tick_iambic_a(keyer);
        break;
    case MorseKeyerModePlainIambic:
        morse_keyer_tick_plain_iambic(keyer);
        break;
    case MorseKeyerModeSingleDot:
        morse_keyer_tick_single_dot(keyer);
        break;
    case MorseKeyerModeElBug:
        morse_keyer_tick_elbug(keyer);
        break;
    case MorseKeyerModeBug:
        morse_keyer_tick_bug(keyer);
        break;
    case MorseKeyerModeStraight:
    default:
        keyer->phase = MorseKeyerPhaseIdle;
        keyer->current_element = MorseKeyerElementNone;
        keyer->ticks_left = 0;
        keyer->tone_on = keyer->dit_down || keyer->dah_down;
        break;
    }

    keyer->prev_dit_down = keyer->dit_down;
    keyer->prev_dah_down = keyer->dah_down;
}

bool morse_keyer_tone(const MorseKeyer* keyer) {
    return keyer->tone_on;
}

const char* morse_keyer_mode_name(uint8_t mode) {
    switch(mode) {
    case MorseKeyerModeSingleDot:
        return "s-dot";
    case MorseKeyerModeElBug:
        return "elbug";
    case MorseKeyerModePlainIambic:
        return "plain";
    case MorseKeyerModeIambicA:
        return "iambic a";
    case MorseKeyerModeBug:
        return "bug";
    case MorseKeyerModeStraight:
    default:
        return "straight";
    }
}

uint8_t morse_keyer_next_ui_mode(uint8_t mode) {
    switch(mode) {
    case MorseKeyerModeStraight:
        return MorseKeyerModeBug;
    case MorseKeyerModeBug:
        return MorseKeyerModePlainIambic;
    case MorseKeyerModePlainIambic:
        return MorseKeyerModeIambicA;
    case MorseKeyerModeIambicA:
    default:
        return MorseKeyerModeStraight;
    }
}
