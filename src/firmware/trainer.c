#include "trainer.h"
#include "cw.h"

#include <string.h>

static const char morse_trainer_koch_order[] = "KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X";

static uint32_t morse_trainer_rand(MorseTrainer* trainer) {
    trainer->rng_state = trainer->rng_state * 1664525U + 1013904223U;
    return trainer->rng_state;
}

const char* morse_trainer_char_morse(char ch) {
    static char buf[8];
    size_t i = 0U;
    uint8_t cw_char = cw(ch);

    if(cw_char == CW_INVALID) {
        buf[0] = '\0';
        return buf;
    }

    FOR_EACH_CW_SYMBOL(symbol) {
        if(i + 1U >= sizeof(buf)) break;
        buf[i++] = symbol ? '-' : '.';
    }

    buf[i] = '\0';
    return buf;
}

void morse_trainer_init(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    memset(trainer, 0, sizeof(*trainer));
    trainer->lesson = 1U;
    trainer->group_size = 5U;
    trainer->session_groups = 10U;
    trainer->local_dit_ms = 100U;
    trainer->rng_state = 1U;
}

size_t morse_trainer_lesson_count(void) {
    return sizeof(morse_trainer_koch_order) - 1U;
}

void morse_trainer_set_lesson(MorseTrainer* trainer, uint8_t lesson) {
    uint8_t max_lesson = (uint8_t)morse_trainer_lesson_count();

    if(trainer == NULL) {
        return;
    }

    if(lesson < 1U) {
        lesson = 1U;
    }
    if(lesson > max_lesson) {
        lesson = max_lesson;
    }

    trainer->lesson = lesson;
}

uint8_t morse_trainer_lesson(const MorseTrainer* trainer) {
    if(trainer == NULL || trainer->lesson == 0U) {
        return 1U;
    }

    return trainer->lesson;
}

void morse_trainer_set_group_size(MorseTrainer* trainer, uint8_t group_size) {
    if(trainer == NULL) {
        return;
    }

    if(group_size < 1U) {
        group_size = 1U;
    }
    if(group_size > MORSE_TRAINER_GROUP_CAP - 1U) {
        group_size = MORSE_TRAINER_GROUP_CAP - 1U;
    }

    trainer->group_size = group_size;
}

uint8_t morse_trainer_group_size(const MorseTrainer* trainer) {
    return trainer ? trainer->group_size : 5U;
}

void morse_trainer_set_session_groups(MorseTrainer* trainer, uint8_t session_groups) {
    if(trainer == NULL) {
        return;
    }

    if(session_groups < 1U) {
        session_groups = 1U;
    }
    if(session_groups > 40U) {
        session_groups = 40U;
    }

    trainer->session_groups = session_groups;
}

uint8_t morse_trainer_session_groups(const MorseTrainer* trainer) {
    return trainer ? trainer->session_groups : 10U;
}

const char* morse_trainer_charset(const MorseTrainer* trainer) {
    static char buf[MORSE_TRAINER_CHARSET_CAP];
    size_t lesson;

    if(trainer != NULL && trainer->charset_override[0] != '\0') {
        return trainer->charset_override;
    }

    lesson = trainer ? morse_trainer_lesson(trainer) : 1U;
    if(lesson >= sizeof(buf)) {
        lesson = sizeof(buf) - 1U;
    }

    memcpy(buf, morse_trainer_koch_order, lesson);
    buf[lesson] = '\0';
    return buf;
}

const char* morse_trainer_last_group(const MorseTrainer* trainer) {
    return trainer ? trainer->last_group : "";
}

const char* morse_trainer_expected(const MorseTrainer* trainer) {
    return trainer ? trainer->expected : "";
}

const char* morse_trainer_next_group(MorseTrainer* trainer) {
    const char* charset;
    size_t charset_len = 0U;
    size_t wi = 0U;
    size_t i;

    if(trainer == NULL) {
        return "";
    }

    charset = morse_trainer_charset(trainer);
    while(charset[charset_len] != '\0') {
        charset_len++;
    }

    if(charset_len == 0U) {
        trainer->last_group[0] = '\0';
        trainer->expected[0] = '\0';
        return trainer->last_group;
    }

    for(i = 0; i + 1U < sizeof(trainer->last_group) && i < trainer->group_size; i++) {
        trainer->last_group[i] = charset[morse_trainer_rand(trainer) % charset_len];
    }
    trainer->last_group[i] = '\0';

    for(i = 0; trainer->last_group[i] != '\0' && wi + 1U < sizeof(trainer->expected); i++) {
        const char* morse = morse_trainer_char_morse(trainer->last_group[i]);

        while(*morse != '\0' && wi + 1U < sizeof(trainer->expected)) {
            trainer->expected[wi++] = *morse++;
        }
    }
    trainer->expected[wi] = '\0';
    return trainer->last_group;
}
