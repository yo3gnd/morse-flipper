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

static uint8_t txg_consistency(uint32_t total_diff, uint32_t total_ref)
{
    uint32_t pct;

    if(total_ref == 0U) return 100U;
    pct = (total_diff * 100U + (total_ref / 2U)) / total_ref;
    if(pct >= 100U) return 0U;
    return (uint8_t)(100U - pct);
}

static uint16_t txg_abs100(uint16_t v)
{
    return v > 100U ? (uint16_t)(v - 100U) : (uint16_t)(100U - v);
}

static void txg_fault_try(
    const char** best,
    uint16_t* best_sev,
    uint16_t* best_delta,
    const char* text,
    uint16_t sev,
    uint16_t delta)
{
    if(best == 0 || best_sev == 0 || best_delta == 0 || text == 0) return;
    if(sev > *best_sev || (sev == *best_sev && delta > *best_delta)) {
        *best = text;
        *best_sev = sev;
        *best_delta = delta;
    }
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

    g->result.speed_pct = got_ms ? txg_pct(got_ms, want_ms) : 0U;
    g->result.letter_gap_pct = lgcnt ? txg_pct(got_lgap, want_lgap) : 0U;
    g->result.correct_pass = g->result.correct == MORSE_FLIPPER_TX_GROUP_LEN;
    g->result.speed_pass = g->result.speed_pct >= 90U && g->result.speed_pct <= 110U;
    g->result.letter_gap_pass =
        g->result.letter_gap_pct >= 90U && g->result.letter_gap_pct <= 110U;
    g->result.passed =
        !timed_out && g->result.correct_pass && g->result.speed_pass && g->result.letter_gap_pass;
}

static void morse_flipper_tx_group_score_sk(MorseFlipperTxGroup* g, uint16_t dit_ms)
{
    uint8_t mu[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint8_t su[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint8_t mc;
    uint8_t sc;
    uint32_t want_marks = 0U;
    uint32_t got_marks = 0U;
    uint32_t got_dits = 0U;
    uint32_t got_dahs = 0U;
    uint32_t dit_cnt = 0U;
    uint32_t dah_cnt = 0U;
    uint32_t got_ditgaps = 0U;
    uint32_t ditgap_cnt = 0U;
    uint32_t var_ref = 0U;
    uint32_t var_diff = 0U;
    uint32_t avg_dit = 0U;
    uint32_t avg_dah = 0U;

    if(g == 0) return;
    txg_expected(g->target, mu, &mc, su, &sc);

    for(uint8_t i = 0U; i < mc && i < g->mark_count; i++) {
        uint32_t want = (uint32_t)mu[i] * dit_ms;
        want_marks += want;
        got_marks += g->marks[i];
        if(mu[i] == 3U) {
            got_dahs += g->marks[i];
            dah_cnt++;
        } else {
            got_dits += g->marks[i];
            dit_cnt++;
        }
    }

    if(dit_cnt) avg_dit = got_dits / dit_cnt;
    if(dah_cnt) avg_dah = got_dahs / dah_cnt;
    if(avg_dit && avg_dah) g->result.ratio_x100 = (uint16_t)((avg_dah * 100U) / avg_dit);
    else g->result.ratio_x100 = 300U;

    for(uint8_t i = 0U; i < sc && i < g->space_count; i++) {
        if(su[i] == 1U) {
            got_ditgaps += g->spaces[i];
            ditgap_cnt++;
        }
    }

    for(uint8_t i = 0U; i < mc && i < g->mark_count; i++) {
        uint32_t ref = mu[i] == 3U ? avg_dah : avg_dit;
        if(ref == 0U) continue;
        var_ref += ref;
        var_diff += g->marks[i] > ref ? (g->marks[i] - ref) : (ref - g->marks[i]);
    }

    g->result.accuracy_pct = got_marks ? txg_pct(got_marks, want_marks) : 0U;
    g->result.dit_pct = avg_dit ? txg_pct(avg_dit, dit_ms) : 100U;
    g->result.dah_pct = avg_dah ? txg_pct(avg_dah, (uint32_t)dit_ms * 3U) : 100U;
    g->result.dit_gap_pct = ditgap_cnt ? txg_pct(got_ditgaps, ditgap_cnt * dit_ms) : 100U;
    g->result.variance_pct = txg_consistency(var_diff, var_ref);

    g->result.ratio_pass = g->result.ratio_x100 >= 285U && g->result.ratio_x100 <= 315U;
    g->result.accuracy_pass = g->result.accuracy_pct >= 90U && g->result.accuracy_pct <= 110U;
    g->result.dit_gap_pass = g->result.dit_gap_pct >= 90U && g->result.dit_gap_pct <= 110U;
    g->result.variance_pass = g->result.variance_pct >= 90U && g->result.variance_pct <= 110U;
    g->result.passed = g->result.passed && g->result.ratio_pass && g->result.accuracy_pass &&
        g->result.dit_gap_pass && g->result.variance_pass;
}

static void morse_flipper_tx_group_pick_fault(MorseFlipperTxGroup* g)
{
    const char* best = "";
    uint16_t sev = 0U;
    uint16_t delta = 0U;
    uint8_t len;

    if(g == 0) return;
    len = morse_flipper_tx_group_answer_len(g);

    if(g->result.timed_out) {
        txg_fault_try(&best, &sev, &delta, len == 0U ? "no answer" : "incomplete group", 220U, 100U);
    }
    if(!g->result.correct_pass)
        txg_fault_try(&best, &sev, &delta, "wrong letter sent", 210U, (uint16_t)(5U - g->result.correct) * 20U);

    if(!g->result.speed_pass)
        txg_fault_try(
            &best,
            &sev,
            &delta,
            g->result.speed_pct > 110U ? "too slow" : "too fast",
            160U,
            txg_abs100(g->result.speed_pct));

    if(!g->result.letter_gap_pass)
        txg_fault_try(
            &best,
            &sev,
            &delta,
            g->result.letter_gap_pct > 110U ? "letter gaps long" : "letter gaps short",
            150U,
            txg_abs100(g->result.letter_gap_pct));

    if(g->sk && !g->result.ratio_pass)
        txg_fault_try(
            &best,
            &sev,
            &delta,
            g->result.ratio_x100 > 315U ? "long dah ratio" : "short dah ratio",
            130U,
            g->result.ratio_x100 > 300U ? (uint16_t)(g->result.ratio_x100 - 300U) :
                                           (uint16_t)(300U - g->result.ratio_x100));

    if(g->sk && !g->result.accuracy_pass)
    {
        bool dit_worse = txg_abs100(g->result.dit_pct) >= txg_abs100(g->result.dah_pct);
        uint8_t p = dit_worse ? g->result.dit_pct : g->result.dah_pct;
        txg_fault_try(
            &best,
            &sev,
            &delta,
            dit_worse ? (p > 110U ? "dits too long" : "dits too short") :
                        (p > 110U ? "dahs too long" : "dahs too short"),
            120U,
            txg_abs100(p));
    }

    if(g->sk && !g->result.dit_gap_pass)
        txg_fault_try(
            &best,
            &sev,
            &delta,
            g->result.dit_gap_pct > 110U ? "symbol gaps long" : "symbol gaps short",
            110U,
            txg_abs100(g->result.dit_gap_pct));

    if(g->sk && !g->result.variance_pass)
        txg_fault_try(
            &best,
            &sev,
            &delta,
            "inconsistent speed",
            100U,
            txg_abs100(g->result.variance_pct));

    g->result.fault = best;
}

void morse_flipper_tx_group_score(MorseFlipperTxGroup* g, uint16_t dit_ms, bool timed_out)
{
    if(g == 0) return;
    morse_flipper_tx_group_score_common(g, dit_ms, timed_out);
    if(g->sk) morse_flipper_tx_group_score_sk(g, dit_ms);
    morse_flipper_tx_group_pick_fault(g);
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
