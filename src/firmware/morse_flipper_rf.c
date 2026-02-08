#include "morse_flipper_rf.h"

#ifdef MORSE_FLIPPER_FAP
#include <furi.h>
#endif

#include <stdio.h>
#include <string.h>

static void rf_push_tx_log(MorseFlipperRf* rf, const char* line)
{
    size_t slot;

    if(!rf || !line) return;

    if(rf->tx_log_count < sizeof(rf->tx_log) / sizeof(rf->tx_log[0]))
    {
        slot = (rf->tx_log_start + rf->tx_log_count) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]));
        rf->tx_log_count++;
    }
    else
    {
        slot = rf->tx_log_start;
        rf->tx_log_start = (rf->tx_log_start + 1u) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]));
    }

    snprintf(rf->tx_log[slot], sizeof(rf->tx_log[slot]), "%s", line);
}

static void rf_refresh_text(MorseFlipperRf* rf)
{
    if(!rf) return;
    snprintf(
        rf->frequency_text,
        sizeof(rf->frequency_text),
        "%lu.%03lu",
        (unsigned long)(rf->frequency_hz / 1000000u),
        (unsigned long)((rf->frequency_hz / 1000u) % 1000u));
}

void morse_flipper_rf_init(MorseFlipperRf* rf)
{
    if(!rf) return;
    memset(rf, 0, sizeof(*rf));
    morse_flipper_rf_timing_init(&rf->rx_timing);
    rf->frequency_hz = 434150000u;
    rf_refresh_text(rf);
}

void morse_flipper_rf_set_frequency_hz(MorseFlipperRf* rf, uint32_t hz)
{
    if(!rf) return;
    rf->frequency_hz = hz;
    rf_refresh_text(rf);
}

void morse_flipper_rf_reset_live(MorseFlipperRf* rf)
{
    if(!rf) return;
    rf->tx_active = false;
    rf->tx_edges = 0;
    rf->tx_log_start = 0;
    rf->tx_log_count = 0;
    morse_flipper_rf_timing_init(&rf->rx_timing);
}

void morse_flipper_rf_handle_tx(MorseFlipperRf* rf, bool active, char symbol)
{
    char line[48];

    if(!rf) return;
    if(rf->tx_active == active) return;

    rf->tx_active = active;
    rf->tx_edges++;
    snprintf(
        line,
        sizeof(line),
        "ook %s %c @ %s",
        active ? "on" : "off",
        symbol ? symbol : '?',
        rf->frequency_text);
    rf_push_tx_log(rf, line);
}

void morse_flipper_rf_capture_rx_timing(MorseFlipperRf* rf, bool mark, uint16_t duration_ms)
{
    if(!rf || !duration_ms) return;
    morse_flipper_rf_timing_capture(&rf->rx_timing, mark, duration_ms, rf->frequency_text);
}

uint32_t morse_flipper_rf_frequency_hz(const MorseFlipperRf* rf)
{
    return rf ? rf->frequency_hz : 0;
}

const char* morse_flipper_rf_frequency_text(const MorseFlipperRf* rf)
{
    return rf ? rf->frequency_text : "";
}

size_t morse_flipper_rf_tx_log_count(const MorseFlipperRf* rf)
{
    return rf ? rf->tx_log_count : 0;
}

const char* morse_flipper_rf_tx_log_line(const MorseFlipperRf* rf, size_t idx)
{
    if(!rf || idx >= rf->tx_log_count) return "";
    return rf->tx_log[(rf->tx_log_start + idx) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]))];
}

size_t morse_flipper_rf_rx_count(const MorseFlipperRf* rf)
{
    return rf ? morse_flipper_rf_timing_count(&rf->rx_timing) : 0;
}

bool morse_flipper_rf_rx_mark(const MorseFlipperRf* rf, size_t idx)
{
    return rf ? morse_flipper_rf_timing_mark(&rf->rx_timing, idx) : false;
}

uint16_t morse_flipper_rf_rx_duration_ms(const MorseFlipperRf* rf, size_t idx)
{
    return rf ? morse_flipper_rf_timing_duration_ms(&rf->rx_timing, idx) : 0;
}

size_t morse_flipper_rf_rx_log_count(const MorseFlipperRf* rf)
{
    return rf ? morse_flipper_rf_timing_log_count(&rf->rx_timing) : 0;
}

const char* morse_flipper_rf_rx_log_line(const MorseFlipperRf* rf, size_t idx)
{
    return rf ? morse_flipper_rf_timing_log_line(&rf->rx_timing, idx) : "";
}
