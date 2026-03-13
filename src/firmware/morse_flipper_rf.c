#include "morse_flipper_rf.h"

#ifdef MORSE_FLIPPER_FAP
#include "morse_flipper_app_i.h"
#endif

#include <stdio.h>
#include <string.h>

static void rf_push_tx_log(MorseFlipperRf* rf, const char* line) {
    size_t slot;

    if(!rf || !line) return;

    if(rf->tx_log_count < sizeof(rf->tx_log) / sizeof(rf->tx_log[0])) {
        slot =
            (rf->tx_log_start + rf->tx_log_count) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]));
        rf->tx_log_count++;
    } else {
        slot = rf->tx_log_start;
        rf->tx_log_start = (rf->tx_log_start + 1u) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]));
    }

    snprintf(rf->tx_log[slot], sizeof(rf->tx_log[slot]), "%s", line);
}

static void rf_refresh_text(MorseFlipperRf* rf) {
    if(!rf) return;
    snprintf(
        rf->frequency_text,
        sizeof(rf->frequency_text),
        "%lu.%03lu",
        (unsigned long)(rf->frequency_hz / 1000000u),
        (unsigned long)((rf->frequency_hz / 1000u) % 1000u));
}

void morse_flipper_rf_init(MorseFlipperRf* rf) {
    if(!rf) return;
    memset(rf, 0, sizeof(*rf));
    morse_flipper_rf_timing_init(&rf->rx_timing);
    rf->frequency_hz = MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_HZ;
    rf_refresh_text(rf);
}

void morse_flipper_rf_set_frequency_hz(MorseFlipperRf* rf, uint32_t hz) {
    if(!rf) return;
    rf->frequency_hz = hz;
    rf_refresh_text(rf);
}

void morse_flipper_rf_reset_live(MorseFlipperRf* rf) {
    if(!rf) return;
    rf->tx_active = false;
    rf->tx_edges = 0;
    rf->tx_log_start = 0;
    rf->tx_log_count = 0;
    morse_flipper_rf_timing_init(&rf->rx_timing);
}

void morse_flipper_rf_handle_tx(MorseFlipperRf* rf, bool active, char symbol) {
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

void morse_flipper_rf_capture_rx_timing(MorseFlipperRf* rf, bool mark, uint16_t duration_ms) {
    if(!rf || !duration_ms) return;
    morse_flipper_rf_timing_capture(&rf->rx_timing, mark, duration_ms, rf->frequency_text);
}

uint32_t morse_flipper_rf_frequency_hz(const MorseFlipperRf* rf) {
    return rf ? rf->frequency_hz : 0;
}

uint32_t morse_flipper_rf_frequency_khz(const MorseFlipperRf* rf) {
    return rf ? (rf->frequency_hz / 1000u) : 0;
}

const char* morse_flipper_rf_frequency_text(const MorseFlipperRf* rf) {
    return rf ? rf->frequency_text : "";
}

size_t morse_flipper_rf_tx_log_count(const MorseFlipperRf* rf) {
    return rf ? rf->tx_log_count : 0;
}

const char* morse_flipper_rf_tx_log_line(const MorseFlipperRf* rf, size_t idx) {
    if(!rf || idx >= rf->tx_log_count) return "";
    return rf->tx_log[(rf->tx_log_start + idx) % (sizeof(rf->tx_log) / sizeof(rf->tx_log[0]))];
}

size_t morse_flipper_rf_rx_count(const MorseFlipperRf* rf) {
    return rf ? morse_flipper_rf_timing_count(&rf->rx_timing) : 0;
}

bool morse_flipper_rf_rx_mark(const MorseFlipperRf* rf, size_t idx) {
    return rf ? morse_flipper_rf_timing_mark(&rf->rx_timing, idx) : false;
}

uint16_t morse_flipper_rf_rx_duration_ms(const MorseFlipperRf* rf, size_t idx) {
    return rf ? morse_flipper_rf_timing_duration_ms(&rf->rx_timing, idx) : 0;
}

size_t morse_flipper_rf_rx_log_count(const MorseFlipperRf* rf) {
    return rf ? morse_flipper_rf_timing_log_count(&rf->rx_timing) : 0;
}

const char* morse_flipper_rf_rx_log_line(const MorseFlipperRf* rf, size_t idx) {
    return rf ? morse_flipper_rf_timing_log_line(&rf->rx_timing, idx) : "";
}

#ifdef MORSE_FLIPPER_FAP
int8_t morse_flipper_rssi_dbm_round(float rssi) {
    if(rssi >= 0.0f) return (int8_t)(rssi + 0.5f);
    return (int8_t)(rssi - 0.5f);
}

int8_t morse_flipper_rf_clamp_dbm(int8_t dbm) {
    if(dbm < -115) return -115;
    if(dbm > -50) return -50;
    return dbm;
}

static bool morse_flipper_rf_tx_allowed_hz(uint32_t hz) {
    return furi_hal_subghz_is_frequency_valid(hz) && furi_hal_region_is_frequency_allowed(hz);
}

bool morse_flipper_rf_tx_allowed_khz(uint32_t khz) {
    return morse_flipper_rf_tx_allowed_hz(khz * 1000U);
}

void morse_flipper_rf_reset_edit(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->rf_edit_khz = morse_flipper_rf_frequency_khz(&app->rf);
    app->rf_freq_focus = 0U;
}

static uint32_t morse_flipper_rf_edit_place(uint8_t focus) {
    static const uint32_t places[MORSE_FLIPPER_RF_FREQ_DIGITS] = {
        100000U, 10000U, 1000U, 100U, 10U, 1U};

    if(focus >= MORSE_FLIPPER_RF_FREQ_DIGITS) return 1U;
    return places[focus];
}

void morse_flipper_rf_bump_focus(MorseFlipperApp* app, int dir) {
    uint8_t focus;

    if(app == NULL) return;
    focus = app->rf_freq_focus % MORSE_FLIPPER_RF_FREQ_DIGITS;
    if(dir < 0) {
        app->rf_freq_focus = focus == 0U ? (MORSE_FLIPPER_RF_FREQ_DIGITS - 1U) :
                                           (uint8_t)(focus - 1U);
    } else {
        app->rf_freq_focus = (uint8_t)((focus + 1U) % MORSE_FLIPPER_RF_FREQ_DIGITS);
    }
}

void morse_flipper_rf_bump_digit(MorseFlipperApp* app, int dir) {
    uint32_t place;
    uint32_t khz;
    uint8_t digit;
    uint8_t next_digit;
    int32_t delta;

    if(app == NULL) return;

    place = morse_flipper_rf_edit_place(app->rf_freq_focus);
    khz = app->rf_edit_khz % 1000000U;
    digit = (uint8_t)((khz / place) % 10U);
    next_digit = dir < 0 ? (uint8_t)((digit + 9U) % 10U) : (uint8_t)((digit + 1U) % 10U);
    delta = ((int32_t)next_digit - (int32_t)digit) * (int32_t)place;
    app->rf_edit_khz = (uint32_t)((int32_t)khz + delta);
}

void morse_flipper_rf_commit_edit(MorseFlipperApp* app) {
    uint32_t khz;

    if(app == NULL) return;

    khz = app->rf_edit_khz % 1000000U;
    if(!morse_flipper_rf_tx_allowed_khz(khz)) khz = MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ;
    app->rf_edit_khz = khz;
    morse_flipper_rf_set_frequency_hz(&app->rf, khz * 1000U);
    morse_flipper_save_rf_config(app);
}

const char* morse_flipper_rf_khz_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    unsigned long khz = MORSE_FLIPPER_RF_DEFAULT_FREQUENCY_KHZ;

    if(buf == NULL || buf_sz == 0U) return "433150 khz";
    if(app != NULL) khz = (unsigned long)morse_flipper_rf_frequency_khz(&app->rf);
    snprintf(buf, buf_sz, "%lu khz", khz);
    return buf;
}

const char* morse_flipper_rf_rssi_line(const MorseFlipperApp* app, char* buf, size_t buf_sz) {
    if(buf == NULL || buf_sz == 0U) return "";

    if(app == NULL || !app->rf_rssi_valid) {
        snprintf(buf, buf_sz, "cs0 --/--");
        return buf;
    }

    snprintf(
        buf,
        buf_sz,
        "cs%d %d/%d",
        app->rf_carrier_present ? 1 : 0,
        (int)app->rf_rssi_dbm,
        (int)app->rf_rssi_peak_dbm);
    return buf;
}
#endif
