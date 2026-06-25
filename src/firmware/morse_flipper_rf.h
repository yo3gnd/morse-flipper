/*
 * Purpose: Define RF Morse state and host-safe RF operations.
 * Owns: frequency defaults, TX/RX log buffers, and RF timing aggregation.
 * Depends on: morse_flipper_rf_timing.h.
 * Tests: tests/test_rf.c and tests/test_decoder_rf_integration.c.
 */

#ifndef yo3gnd_rf_2239
#define yo3gnd_rf_2239

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "morse_flipper_rf_timing.h"

#define MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ  433150000u
#define MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ 433150u

typedef struct {
    uint32_t frequency_hz;
    bool tx_active;
    uint32_t tx_edges;
    char tx_log[8][48];
    size_t tx_log_start;
    size_t tx_log_count;
    MorseFlipperRfTiming rx_timing;
    char frequency_text[16];
} MorseFlipperRf;

void morse_flipper_rf_init(MorseFlipperRf* rf);
void morse_flipper_rf_set_frequency_hz(MorseFlipperRf* rf, uint32_t hz);
void morse_flipper_rf_reset_live(MorseFlipperRf* rf);
void morse_flipper_rf_handle_tx(MorseFlipperRf* rf, bool active, char symbol);
void morse_flipper_rf_capture_rx_timing(MorseFlipperRf* rf, bool mark, uint16_t duration_ms);
uint32_t morse_flipper_rf_frequency_hz(const MorseFlipperRf* rf);
uint32_t morse_flipper_rf_frequency_khz(const MorseFlipperRf* rf);
const char* morse_flipper_rf_frequency_text(const MorseFlipperRf* rf);
size_t morse_flipper_rf_tx_log_count(const MorseFlipperRf* rf);
const char* morse_flipper_rf_tx_log_line(const MorseFlipperRf* rf, size_t idx);
size_t morse_flipper_rf_rx_count(const MorseFlipperRf* rf);
bool morse_flipper_rf_rx_mark(const MorseFlipperRf* rf, size_t idx);
uint16_t morse_flipper_rf_rx_duration_ms(const MorseFlipperRf* rf, size_t idx);
size_t morse_flipper_rf_rx_log_count(const MorseFlipperRf* rf);
const char* morse_flipper_rf_rx_log_line(const MorseFlipperRf* rf, size_t idx);

#endif
