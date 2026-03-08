#include "morse_flipper_tx_groups.h"

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
