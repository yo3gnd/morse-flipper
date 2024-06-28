static void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms) {
    MorseFlipperApp* app = ctx;

    if(app == NULL || duration_ms == 0U) return;

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

static void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!app->rf_live_active) {
        morse_flipper_radio_sync_live(
            &app->radio, morse_flipper_rf_frequency_hz(&app->rf), false, false);
        morse_flipper_radio_set_tx_level(&app->radio, false);
        return;
    }

    if(!app->radio.tx_on && morse_flipper_any_active_notes(app)) {
        morse_flipper_radio_sync_live(
            &app->radio, morse_flipper_rf_frequency_hz(&app->rf), true, true);
        morse_flipper_radio_set_tx_level(&app->radio, morse_flipper_any_active_notes(app));
    } else if(app->radio.tx_on && !morse_flipper_any_active_notes(app)) {
        morse_flipper_radio_set_tx_level(&app->radio, false);
        if(app->rf_tx_tail_until != 0U && now_ms >= app->rf_tx_tail_until) {
            morse_flipper_radio_sync_live(
                &app->radio, morse_flipper_rf_frequency_hz(&app->rf), false, false);
            app->rf_tx_tail_until = 0U;
        }
    }
}
