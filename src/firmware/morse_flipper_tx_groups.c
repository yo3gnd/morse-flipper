#include "morse_flipper_tx_groups.h"
#include "cw.h"

#include <string.h>

static uint32_t txg_rand(MorseFlipperTxGroup* g)
{
    g->rng = g->rng * 1103515245u + 12345u;
    return g->rng;
}

static char txg_pick(MorseFlipperTxGroup* g)
{
    uint32_t r = txg_rand(g);

    if((r % 100U) < 20U) return (char)('0' + ((r / 100U) % 10U));
    return (char)('A' + ((r / 100U) % 26U));
}

static char txg_up(char ch)
{
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static bool txg_ok(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}

static uint8_t txg_pct(uint32_t got, uint32_t want)
{
    if(got == 0U || want == 0U) return 0U;
    got = (got * 100U + (want / 2U)) / want;
    if(got > 255U) got = 255U;
    return (uint8_t)got;
}

static void txg_expected(
    const char* text,
    uint8_t* mark_units,
    uint8_t* mark_cnt,
    uint8_t* space_units,
    uint8_t* space_cnt)
{
    uint8_t mi = 0U;
    uint8_t si = 0U;

    if(mark_cnt) *mark_cnt = 0U;
    if(space_cnt) *space_cnt = 0U;
    if(text == 0) return;

    for(uint8_t c = 0U; c < MORSE_FLIPPER_TX_GROUP_LEN && text[c] != '\0'; c++) {
        uint8_t code = cw(text[c]);
        uint8_t cw_char = code;
        bool first = true;

        if(code == CW_INVALID) continue;
        if(c != 0U && si < MORSE_FLIPPER_TX_GROUP_MAX_EDGES) space_units[si++] = 3U;
        FOR_EACH_CW_SYMBOL(symbol) {
            if(!first && si < MORSE_FLIPPER_TX_GROUP_MAX_EDGES) space_units[si++] = 1U;
            first = false;
            if(mi < MORSE_FLIPPER_TX_GROUP_MAX_EDGES) mark_units[mi++] = symbol ? 3U : 1U;
        }
    }

    if(mark_cnt) *mark_cnt = mi;
    if(space_cnt) *space_cnt = si;
}

void morse_flipper_tx_group_init(MorseFlipperTxGroup* g)
{
    if(g == 0) return;
    memset(g, 0, sizeof(*g));
    g->rng = 0x55443322U;
    memcpy(g->target, "ABCDE", MORSE_FLIPPER_TX_GROUP_LEN + 1U);
}

void morse_flipper_tx_group_start(MorseFlipperTxGroup* g, bool sk)
{
    uint8_t i;
    uint32_t oldrng;

    if(g == 0) return;
    oldrng = g->rng ? g->rng : 0x55443322U;
    memset(g, 0, sizeof(*g));
    g->rng = oldrng;
    g->sk = sk;
    for(i = 0U; i < MORSE_FLIPPER_TX_GROUP_LEN; i++) g->target[i] = txg_pick(g);
    g->target[MORSE_FLIPPER_TX_GROUP_LEN] = '\0';
}

void morse_flipper_tx_group_feed_mark(MorseFlipperTxGroup* g, uint16_t ms)
{
    if(g == 0 || ms == 0U) return;
    if(g->mark_count < MORSE_FLIPPER_TX_GROUP_MAX_EDGES)
        g->marks[g->mark_count++] = ms;
}

void morse_flipper_tx_group_feed_space(MorseFlipperTxGroup* g, uint16_t ms)
{
    if(g == 0 || ms == 0U) return;
    if(g->space_count < MORSE_FLIPPER_TX_GROUP_MAX_EDGES)
        g->spaces[g->space_count++] = ms;
}

void morse_flipper_tx_group_feed_text(MorseFlipperTxGroup* g, const char* text)
{
    char ch;
    uint8_t n;

    if(g == 0 || text == 0) return;
    n = morse_flipper_tx_group_answer_len(g);

    while(*text != '\0' && n < MORSE_FLIPPER_TX_GROUP_LEN) {
        ch = txg_up(*text++);
        if(!txg_ok(ch)) continue;
        g->answer[n++] = ch;
        g->answer[n] = '\0';
    }
}

void morse_flipper_tx_group_score_common(MorseFlipperTxGroup* g, uint16_t dit_ms, bool timed_out)
{
    uint8_t mu[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint8_t su[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint8_t mc;
    uint8_t sc;
    uint32_t want_ms = 0U;
    uint32_t got_ms = 0U;
    uint32_t want_lgap = 0U;
    uint32_t got_lgap = 0U;
    uint8_t lgcnt = 0U;

    if(g == 0) return;
    memset(&g->result, 0, sizeof(g->result));
    g->result.timed_out = timed_out;
    txg_expected(g->target, mu, &mc, su, &sc);

    for(uint8_t i = 0U; i < MORSE_FLIPPER_TX_GROUP_LEN; i++)
        if(g->target[i] != '\0' && g->target[i] == g->answer[i]) g->result.correct++;

    for(uint8_t i = 0U; i < mc; i++) {
        want_ms += (uint32_t)mu[i] * dit_ms;
        if(i < g->mark_count) got_ms += g->marks[i];
    }

    for(uint8_t i = 0U; i < sc; i++) {
        want_ms += (uint32_t)su[i] * dit_ms;
        if(i < g->space_count) got_ms += g->spaces[i];
        if(su[i] == 3U) {
            want_lgap += 3U * dit_ms;
            if(i < g->space_count) got_lgap += g->spaces[i];
            lgcnt++;
        }
    }

    g->result.speed_pct = got_ms ? txg_pct(want_ms, got_ms) : 0U;
    g->result.letter_gap_pct = lgcnt ? txg_pct(got_lgap, want_lgap) : 0U;
    g->result.correct_pass = g->result.correct == MORSE_FLIPPER_TX_GROUP_LEN;
    g->result.speed_pass = g->result.speed_pct >= 90U && g->result.speed_pct <= 110U;
    g->result.letter_gap_pass =
        g->result.letter_gap_pct >= 90U && g->result.letter_gap_pct <= 110U;
    g->result.passed =
        !timed_out && g->result.correct_pass && g->result.speed_pass && g->result.letter_gap_pass;
}

bool morse_flipper_tx_group_complete(const MorseFlipperTxGroup* g)
{
    return morse_flipper_tx_group_answer_len(g) >= MORSE_FLIPPER_TX_GROUP_LEN;
}

uint8_t morse_flipper_tx_group_answer_len(const MorseFlipperTxGroup* g)
{
    uint8_t n = 0U;

    if(g == 0) return 0U;
    while(n < MORSE_FLIPPER_TX_GROUP_LEN && g->answer[n] != '\0') n++;
    return n;
}
