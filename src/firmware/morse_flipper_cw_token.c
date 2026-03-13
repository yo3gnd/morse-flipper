#include "morse_flipper_cw_token.h"

#include <string.h>

typedef struct {
    uint8_t token;
    const char* label;
    const char* text;
    const char* morse;
} MorseFlipperCwTokenDef;

static const MorseFlipperCwTokenDef morse_flipper_cw_tokens[] = {
    {MORSE_FLIPPER_CW_TOKEN_SK, "SK", "<SK>", "...-.-"},
    {MORSE_FLIPPER_CW_TOKEN_BK, "BK", "<BK>", "-...-.-"},
    {MORSE_FLIPPER_CW_TOKEN_CT_KA, "CT", "<CT>", "-.-.-"},
    {MORSE_FLIPPER_CW_TOKEN_VE_SN, "VE", "<VE>", "...-."},
    {MORSE_FLIPPER_CW_TOKEN_AA, "AA", "<AA>", ".-.-"},
    {MORSE_FLIPPER_CW_TOKEN_SOS, "SOS", "<SOS>", "...---..."},
};

static char cw_token_up(char ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static const MorseFlipperCwTokenDef* cw_token_find(uint8_t ch) {
    for(size_t i = 0U; i < sizeof(morse_flipper_cw_tokens) / sizeof(morse_flipper_cw_tokens[0]); i++) {
        if(morse_flipper_cw_tokens[i].token == ch) return &morse_flipper_cw_tokens[i];
    }

    return NULL;
}

static bool cw_token_label_eq(const char* a, const char* b) {
    size_t i = 0U;

    if(a == NULL || b == NULL) return false;

    while(a[i] != '\0' && b[i] != '\0') {
        if(cw_token_up(a[i]) != cw_token_up(b[i])) return false;
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static bool cw_token_append(char* out, size_t out_sz, size_t* at, const char* text) {
    if(out == NULL || out_sz == 0U || at == NULL || text == NULL) return false;

    while(*text != '\0') {
        if(*at + 1U >= out_sz) {
            out[out_sz - 1U] = '\0';
            return false;
        }
        out[(*at)++] = *text++;
    }

    out[*at] = '\0';
    return true;
}

bool morse_flipper_cw_token_is_private(uint8_t ch) {
    return cw_token_find(ch) != NULL;
}

const char* morse_flipper_cw_token_label(uint8_t ch) {
    const MorseFlipperCwTokenDef* def = cw_token_find(ch);
    return def != NULL ? def->label : "";
}

const char* morse_flipper_cw_token_text(uint8_t ch) {
    const MorseFlipperCwTokenDef* def = cw_token_find(ch);
    return def != NULL ? def->text : "";
}

const char* morse_flipper_cw_token_morse(uint8_t ch) {
    const MorseFlipperCwTokenDef* def = cw_token_find(ch);
    return def != NULL ? def->morse : "";
}

uint16_t morse_flipper_cw_token_code(uint8_t ch) {
    const char* morse = morse_flipper_cw_token_morse(ch);
    uint16_t code = 1U;
    uint8_t count = 0U;

    while(*morse != '\0' && count < 15U) {
        uint16_t bit = (uint16_t)(1U << count);

        if(*morse == '-') code |= bit;
        else code &= (uint16_t)~bit;
        count++;
        code |= (uint16_t)(1U << count);
        morse++;
    }

    return count == 0U ? 0U : code;
}

bool morse_flipper_cw_token_parse(const char* text, uint8_t* token, size_t* consumed) {
    char label[4];
    size_t n = 0U;

    if(token) *token = 0U;
    if(consumed) *consumed = 0U;
    if(text == NULL || text[0] != '<') return false;

    text++;
    while(*text != '\0' && *text != '>' && n + 1U < sizeof(label)) {
        label[n++] = cw_token_up(*text++);
    }
    label[n] = '\0';

    if(*text != '>' || n == 0U) return false;

    for(size_t i = 0U; i < sizeof(morse_flipper_cw_tokens) / sizeof(morse_flipper_cw_tokens[0]); i++) {
        if(cw_token_label_eq(label, morse_flipper_cw_tokens[i].label)) {
            if(token) *token = morse_flipper_cw_tokens[i].token;
            if(consumed) *consumed = n + 2U;
            return true;
        }
    }

    if(cw_token_label_eq(label, "KA")) {
        if(token) *token = MORSE_FLIPPER_CW_TOKEN_CT_KA;
        if(consumed) *consumed = n + 2U;
        return true;
    }
    if(cw_token_label_eq(label, "SN")) {
        if(token) *token = MORSE_FLIPPER_CW_TOKEN_VE_SN;
        if(consumed) *consumed = n + 2U;
        return true;
    }

    return false;
}

size_t morse_flipper_cw_token_expand_text(char* out, size_t out_sz, const char* text) {
    size_t at = 0U;

    if(out == NULL || out_sz == 0U) return 0U;
    out[0] = '\0';
    if(text == NULL) return 0U;

    while(*text != '\0') {
        uint8_t ch = (uint8_t)*text++;

        if(morse_flipper_cw_token_is_private(ch)) {
            cw_token_append(out, out_sz, &at, morse_flipper_cw_token_text(ch));
            continue;
        }

        if(at + 1U >= out_sz) break;
        out[at++] = (char)ch;
        out[at] = '\0';
    }

    return at;
}
