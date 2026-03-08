#ifndef yo3gnd_txgroup_90pq
#define yo3gnd_txgroup_90pq

#include <stdbool.h>
#include <stdint.h>

#define MORSE_FLIPPER_TX_GROUP_LEN       5U
#define MORSE_FLIPPER_TX_GROUP_MAX_EDGES 64U

typedef struct
{
    char target[MORSE_FLIPPER_TX_GROUP_LEN + 1U];
    char answer[MORSE_FLIPPER_TX_GROUP_LEN + 1U];
    uint16_t marks[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint16_t spaces[MORSE_FLIPPER_TX_GROUP_MAX_EDGES];
    uint8_t mark_count;
    uint8_t space_count;
    uint32_t rng;
    bool sk;
} MorseFlipperTxGroup;

void morse_flipper_tx_group_init(MorseFlipperTxGroup* g);
void morse_flipper_tx_group_start(MorseFlipperTxGroup* g, bool sk);
void morse_flipper_tx_group_feed_mark(MorseFlipperTxGroup* g, uint16_t ms);
void morse_flipper_tx_group_feed_space(MorseFlipperTxGroup* g, uint16_t ms);
void morse_flipper_tx_group_feed_text(MorseFlipperTxGroup* g, const char* text);
bool morse_flipper_tx_group_complete(const MorseFlipperTxGroup* g);
uint8_t morse_flipper_tx_group_answer_len(const MorseFlipperTxGroup* g);

#endif
