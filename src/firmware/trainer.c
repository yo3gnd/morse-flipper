#include "trainer.h"
#include "cw.h"

#include <stdio.h>
#include <string.h>

static const char morse_trainer_koch_order[] = "KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X";

enum {
    MorseTrainerPhaseIdle = 0,
    MorseTrainerPhaseListen = 1,
    MorseTrainerPhaseRepeat = 2,
    MorseTrainerPhaseDone = 3,
};

static void morse_trainer_note_session_result(MorseTrainer* trainer, bool missed) {
    if(trainer == NULL || !trainer->session_active) {
        return;
    }

    if(trainer->last_failed) {
        trainer->session_fail_count++;
    }

    if(missed) {
        trainer->session_consecutive_missed++;
    } else {
        trainer->session_consecutive_missed = 0U;
    }

    trainer->session_score_sum = (uint16_t)(trainer->session_score_sum + trainer->last_score);
    trainer->session_scored_groups++;

    if(trainer->session_consecutive_missed >= 4U) {
        trainer->session_aborted = true;
        trainer->session_active = false;
    }
}

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
    return (sizeof(morse_trainer_koch_order) - 2U);
}

void morse_trainer_lesson_label(uint8_t lesson, char* buf, size_t buf_sz) {
    uint8_t max_lesson;

    if(buf == NULL || buf_sz < 2U) {
        return;
    }

    max_lesson = (uint8_t)morse_trainer_lesson_count();
    if(lesson < 1U) {
        lesson = 1U;
    } else if(lesson > max_lesson) {
        lesson = max_lesson;
    }

    if(lesson == 1U) {
        snprintf(buf, buf_sz, "1 - %c %c", morse_trainer_koch_order[0], morse_trainer_koch_order[1]);
        return;
    }

    snprintf(buf, buf_sz, "%u - %c", (unsigned)lesson, morse_trainer_koch_order[lesson]);
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
    size_t n;

    if(trainer != NULL && trainer->charset_override[0] != '\0') return trainer->charset_override;

    n = trainer ? ((size_t)morse_trainer_lesson(trainer) + 1U) : 2U;
    if(n >= sizeof(buf)) n = sizeof(buf) - 1U;

    memcpy(buf, morse_trainer_koch_order, n);
    buf[n] = '\0';
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
    char prev[MORSE_TRAINER_GROUP_CAP];
    size_t clen = 0U;
    size_t wi = 0U;
    size_t i;
    uint8_t tries;

    if(trainer == NULL) return "";

    charset = morse_trainer_charset(trainer);
    strncpy(prev, trainer->last_group, sizeof(prev) - 1U);
    prev[sizeof(prev) - 1U] = '\0';
    while(charset[clen] != '\0') clen++;

    if(clen == 0U) {
        trainer->last_group[0] = '\0';
        trainer->expected[0] = '\0';
        return trainer->last_group;
    }

    tries = 0U;
    do {
        for(i = 0; i + 1U < sizeof(trainer->last_group) && i < trainer->group_size; i++) {
            trainer->last_group[i] = charset[morse_trainer_rand(trainer) % clen];
        }
        trainer->last_group[i] = '\0';
        tries++;
    } while(clen > 1U && prev[0] != '\0' && strcmp(prev, trainer->last_group) == 0 && tries < 8U);

    if(clen > 1U && prev[0] != '\0' && strcmp(prev, trainer->last_group) == 0) {
        size_t last = strlen(trainer->last_group);
        char pick = charset[morse_trainer_rand(trainer) % clen];

        if(last != 0U) {
            if(pick == trainer->last_group[last - 1U]) pick = charset[(morse_trainer_rand(trainer) + 1U) % clen];
            trainer->last_group[last - 1U] = pick;
        }
    }

    for(i = 0; trainer->last_group[i] != '\0' && wi + 1U < sizeof(trainer->expected); i++) {
        const char* morse = morse_trainer_char_morse(trainer->last_group[i]);

        while(*morse != '\0' && wi + 1U < sizeof(trainer->expected)) {
            trainer->expected[wi++] = *morse++;
        }
    }
    trainer->expected[wi] = '\0';
    return trainer->last_group;
}

void morse_trainer_start_repeat(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    morse_trainer_next_group(trainer);
    trainer->answer[0] = '\0';
    trainer->reveal[0] = '\0';
    trainer->wait_ms = 0U;
    trainer->last_score = -1;
    trainer->last_failed = false;
    trainer->phase = MorseTrainerPhaseListen;
}

void morse_trainer_finish_listen(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    trainer->phase = MorseTrainerPhaseRepeat;
    trainer->wait_ms = 0U;
}

void morse_trainer_finish_repeat(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    trainer->phase = MorseTrainerPhaseDone;
}

void morse_trainer_feed_element(MorseTrainer* trainer, char elem) {
    size_t len;

    if(trainer == NULL || trainer->phase != MorseTrainerPhaseRepeat) {
        return;
    }

    if(elem != '.' && elem != '-') {
        return;
    }

    len = strlen(trainer->answer);
    if(len + 1U >= sizeof(trainer->answer)) {
        return;
    }

    trainer->answer[len] = elem;
    trainer->answer[len + 1U] = '\0';
}

void morse_trainer_tick(MorseTrainer* trainer, uint32_t ms) {
    if(trainer == NULL || trainer->phase != MorseTrainerPhaseRepeat) {
        return;
    }

    if(trainer->answer[0] != '\0') {
        return;
    }

    trainer->wait_ms += ms;
    if(trainer->wait_ms < 6000U) {
        return;
    }

    trainer->last_score = 0;
    trainer->last_failed = true;
    trainer->phase = MorseTrainerPhaseDone;
    morse_trainer_note_session_result(trainer, true);
}

int16_t morse_trainer_score_repeat(MorseTrainer* trainer) {
    size_t expected_len;
    size_t answer_len;
    size_t matched = 0U;

    if(trainer == NULL) {
        return -1;
    }

    expected_len = strlen(trainer->expected);
    answer_len = strlen(trainer->answer);

    while(matched < expected_len && matched < answer_len &&
          trainer->expected[matched] == trainer->answer[matched]) {
        matched++;
    }

    trainer->last_score =
        expected_len == 0U ? 0 : (int16_t)(((int32_t)matched * 100) / (int32_t)expected_len);
    trainer->last_failed = trainer->last_score != 100 || trainer->answer[0] == '\0';
    trainer->phase = MorseTrainerPhaseDone;
    morse_trainer_note_session_result(trainer, trainer->answer[0] == '\0');
    return trainer->last_score;
}

void morse_trainer_reset_session(MorseTrainer* trainer) {
    if(trainer == NULL) return;

    trainer->wait_ms = 0U;
    trainer->phase = MorseTrainerPhaseIdle;
    trainer->last_score = -1;
    trainer->last_failed = false;
    trainer->session_active = false;
    trainer->session_aborted = false;
    trainer->session_index = 0U;
    trainer->session_fail_count = 0U;
    trainer->session_consecutive_missed = 0U;
    trainer->session_score_sum = 0U;
    trainer->session_scored_groups = 0U;

    trainer->last_group[0] = '\0';
    trainer->expected[0] = '\0';
    trainer->answer[0] = '\0';
    trainer->reveal[0] = '\0';
}

void morse_trainer_start_session(MorseTrainer* trainer) {
    if(trainer == NULL) {
        return;
    }

    morse_trainer_reset_session(trainer);
    trainer->session_active = true;
    trainer->session_index = 1U;
    morse_trainer_start_repeat(trainer);
}

bool morse_trainer_next_session_group(MorseTrainer* trainer) {
    if(trainer == NULL || !trainer->session_active) {
        return false;
    }

    if(trainer->session_index >= trainer->session_groups) {
        trainer->session_active = false;
        return false;
    }

    trainer->session_index++;
    morse_trainer_start_repeat(trainer);
    return true;
}

const char* morse_trainer_answer(const MorseTrainer* trainer) {
    return trainer ? trainer->answer : "";
}

const char* morse_trainer_phase_name(const MorseTrainer* trainer) {
    if(trainer == NULL) {
        return "idle";
    }

    switch(trainer->phase) {
    case MorseTrainerPhaseListen:
        return "listen";
    case MorseTrainerPhaseRepeat:
        return "repeat";
    case MorseTrainerPhaseDone:
        return "done";
    default:
        return "idle";
    }
}

int16_t morse_trainer_last_score(const MorseTrainer* trainer) {
    return trainer ? trainer->last_score : -1;
}

bool morse_trainer_last_failed(const MorseTrainer* trainer) {
    return trainer ? trainer->last_failed : true;
}

bool morse_trainer_session_active(const MorseTrainer* trainer) {
    return trainer ? trainer->session_active : false;
}

uint8_t morse_trainer_session_index(const MorseTrainer* trainer) {
    return trainer ? trainer->session_index : 0U;
}

uint8_t morse_trainer_session_total(const MorseTrainer* trainer) {
    return trainer ? trainer->session_groups : 0U;
}

bool morse_trainer_session_aborted(const MorseTrainer* trainer) {
    return trainer ? trainer->session_aborted : false;
}

uint8_t morse_trainer_session_fail_count(const MorseTrainer* trainer) {
    return trainer ? trainer->session_fail_count : 0U;
}

uint8_t morse_trainer_session_consecutive_missed(const MorseTrainer* trainer) {
    return trainer ? trainer->session_consecutive_missed : 0U;
}

uint8_t morse_trainer_session_average_score(const MorseTrainer* trainer) {
    if(trainer == NULL || trainer->session_scored_groups == 0U) {
        return 0U;
    }

    return (uint8_t)(trainer->session_score_sum / trainer->session_scored_groups);
}

bool morse_trainer_session_completed(const MorseTrainer* trainer) {
    if(trainer == NULL) return false;
    return !trainer->session_active && !trainer->session_aborted && trainer->phase == MorseTrainerPhaseDone &&
           trainer->session_index >= trainer->session_groups &&
           trainer->session_scored_groups >= trainer->session_groups;
}
