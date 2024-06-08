#ifndef yo3gnd_radio_2230ff
#define yo3gnd_radio_2230ff

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool ready;
    bool tx_on;
    bool rx_on;
    bool tx_level;
    uint32_t freq_hz;
    void (*rx_cb)(void* ctx, bool level, uint16_t duration_ms);
    void* rx_ctx;
    bool rx_mark[64];
    uint16_t rx_ms[64];
    uint8_t rx_wr;
    uint8_t rx_rd;
} MorseFlipperRadio;

void morse_flipper_radio_init(MorseFlipperRadio* radio);
void morse_flipper_radio_deinit(MorseFlipperRadio* radio);
void morse_flipper_radio_set_rx_callback(
    MorseFlipperRadio* radio,
    void (*cb)(void* ctx, bool level, uint16_t duration_ms),
    void* ctx);
void morse_flipper_radio_sync_live(
    MorseFlipperRadio* radio,
    uint32_t freq_hz,
    bool active,
    bool tx_on);
void morse_flipper_radio_drain_rx(MorseFlipperRadio* radio);
void morse_flipper_radio_set_tx_level(MorseFlipperRadio* radio, bool level);

#endif
