#include "morse_flipper_app_i.h"

void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms) {
    MorseFlipperApp* app = ctx;

    if(app == NULL || duration_ms == 0U) return;
    app->rf_rx_edges_window++;
    if(app->screen == MorseFlipperScreenRfRx) app->rf_carrier_present = level;

#if !MORSE_FLIPPER_RF_LIVE_DECODERS
    UNUSED(level);
    return;
#else
    morse_flipper_rf_capture_rx_timing(&app->rf, level, duration_ms);
    if(level) {
        morse_flipper_cw_decoder_feed_mark(&app->rf_decoder, duration_ms);
    } else {
        morse_flipper_cw_decoder_feed_space(&app->rf_decoder, duration_ms);
    }
    morse_flipper_decoder_drain_into(&app->rf_decoder, app->rf_rx_text, sizeof(app->rf_rx_text));
#endif
}

void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms) {
    float rssi;
    int8_t dbm;
    bool old_valid;
    int8_t old_dbm;
    int8_t old_peak_dbm;
    uint16_t old_activity;
    bool old_carrier;
    bool old_monitor_tone;
    bool carrier_now;
    int8_t thr_dbm;
    int32_t avg_sum;
    uint16_t avg_samples;

    if(app == NULL) return;

    if(!app->rf_live_active) {
        morse_flipper_radio_sync_live(
            &app->radio,
            morse_flipper_rf_frequency_hz(&app->rf),
            false,
            false,
            MorseFlipperRadioProfileOokData);
        morse_flipper_radio_set_tx_level(&app->radio, false);
        return;
    }

    if(app->screen == MorseFlipperScreenRfRx) {
        old_valid = app->rf_rssi_valid;
        old_dbm = app->rf_rssi_dbm;
        old_peak_dbm = app->rf_rssi_peak_dbm;
        old_activity = app->rf_rx_activity;
        old_carrier = app->rf_carrier_present;
        old_monitor_tone = app->rf_monitor_tone;
        morse_flipper_radio_sync_live(
            &app->radio,
            morse_flipper_rf_frequency_hz(&app->rf),
            true,
            false,
            MorseFlipperRadioProfileCarrierSense);
        rssi = furi_hal_subghz_get_rssi();
        dbm = morse_flipper_rssi_dbm_round(rssi);
        carrier_now = morse_flipper_radio_read_rx_level(&app->radio);
        app->rf_carrier_present = carrier_now;
        thr_dbm = morse_flipper_rf_clamp_dbm(app->rf_monitor_threshold_dbm);
        app->rf_rssi_sum_dbm += dbm;
        app->rf_rssi_samples++;

        if(app->rf_rssi_next_at == 0U)
            app->rf_rssi_next_at = now_ms + MORSE_FLIPPER_RF_RSSI_WINDOW_MS;
        if(now_ms >= app->rf_rssi_next_at && app->rf_rssi_samples != 0U) {
            avg_sum = app->rf_rssi_sum_dbm;
            avg_samples = app->rf_rssi_samples;

            if(avg_sum >= 0) {
                app->rf_rssi_dbm =
                    (int8_t)((avg_sum + ((int32_t)avg_samples / 2)) / (int32_t)avg_samples);
            } else {
                app->rf_rssi_dbm =
                    (int8_t)((avg_sum - ((int32_t)avg_samples / 2)) / (int32_t)avg_samples);
            }
            app->rf_rx_activity = app->rf_rx_edges_window;
            app->rf_rssi_sum_dbm = 0;
            app->rf_rssi_samples = 0U;
            app->rf_rx_edges_window = 0U;
            app->rf_rssi_next_at = now_ms + MORSE_FLIPPER_RF_RSSI_WINDOW_MS;

            if(!app->rf_rssi_valid || app->rf_rssi_dbm > app->rf_rssi_peak_dbm) {
                app->rf_rssi_peak_dbm = app->rf_rssi_dbm;
                app->rf_rssi_peak_decay_at = now_ms + MORSE_FLIPPER_RF_RSSI_PEAK_DECAY_MS;
            } else if(
                now_ms >= app->rf_rssi_peak_decay_at && app->rf_rssi_peak_dbm > app->rf_rssi_dbm) {
                app->rf_rssi_peak_dbm--;
                app->rf_rssi_peak_decay_at = now_ms + MORSE_FLIPPER_RF_RSSI_PEAK_DECAY_MS;
            }
            app->rf_rssi_valid = true;
        }

        app->rf_monitor_tone =
            (carrier_now && app->rf_rssi_valid) ||
            (app->rf_rssi_valid && (app->rf_rssi_dbm >= thr_dbm));

        if(!old_valid || old_dbm != app->rf_rssi_dbm || old_peak_dbm != app->rf_rssi_peak_dbm ||
           old_activity != app->rf_rx_activity || old_carrier != app->rf_carrier_present ||
           old_monitor_tone != app->rf_monitor_tone) {
            if(old_monitor_tone != app->rf_monitor_tone) {
                morse_flipper_update_sidetone(app);
            }
            morse_flipper_view_dirty(app);
        }
        return;
    }

    if(!app->radio.tx_on && morse_flipper_any_active_notes(app)) {
        morse_flipper_radio_sync_live(
            &app->radio,
            morse_flipper_rf_frequency_hz(&app->rf),
            true,
            true,
            MorseFlipperRadioProfileOokData);
        morse_flipper_radio_set_tx_level(&app->radio, morse_flipper_any_active_notes(app));
    } else if(app->radio.tx_on && !morse_flipper_any_active_notes(app)) {
        morse_flipper_radio_set_tx_level(&app->radio, false);
        if(app->rf_tx_tail_until != 0U && now_ms >= app->rf_tx_tail_until) {
            morse_flipper_radio_sync_live(
                &app->radio,
                morse_flipper_rf_frequency_hz(&app->rf),
                false,
                false,
                MorseFlipperRadioProfileOokData);
            app->rf_tx_tail_until = 0U;
        }
    }
}
