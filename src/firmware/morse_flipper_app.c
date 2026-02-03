MorseFlipperApp* morse_flipper_boot(void)
{
    static MorseFlipperApp app;
    app = (MorseFlipperApp){
        .q = NULL,
        .view_port = NULL,
        .view_dispatcher = NULL,
        .scene_manager = NULL,
        .submenu = NULL,
        .settings_list = NULL,
        .widget = NULL,
        .live_view = NULL,
        .gui = furi_record_open(RECORD_GUI),
        .dialogs = furi_record_open(RECORD_DIALOGS),
        .notifications = furi_record_open(RECORD_NOTIFICATION),
        .help_text = NULL,
        .exit_requested = false,
        .previous_usb_config = NULL,
        .hid_cfg =
            {
                .vid = 0x6666U,
                .pid = 0x434BU,
                .manuf = "YO3GND",
                .product = "Morse Flipper Kbd",
            },
        .tone_on = false,
        .sp_owned = false,
        .sp_busy = false,
        .transport_connected = false,
        .mouse_invert = false,
        .left_down = false,
        .ok_down = false,
        .back_down = false,
        .trainer_playback_active = false,
        .trainer_playback_mark = false,
        .session_started = false,
        .session_round_pending = false,
        .session_result_hold = false,
        .session_result_tone = false,
        .session_result_good = false,
        .midi_rx_pending = false,
        .screen = MorseFlipperScreenMenu,
        .pc_mode = MorseFlipperPcModeOff,
        .pc_pref = MorseFlipperPcModeOff,
        .pc_paddle_preset = 0U,
        .pc_straight_preset = 0U,
        .handedness = MorseFlipperHandednessSwapped,
        .in_src = MorseFlipperInputSourceStraight,
        .keyer_mode = MorseKeyerModeStraight,
        .gpio_straight_idx = MorseFlipperGpioPinP7,
        .gpio_dit_idx = MorseFlipperGpioPinP7,
        .gpio_dah_idx = MorseFlipperGpioPinP5,
        .gpio_ground_idx = MorseFlipperGpioPinP3,
        .gpio_edit_dit_idx = MorseFlipperGpioPinP7,
        .gpio_edit_dah_idx = MorseFlipperGpioPinP5,
        .gpio_edit_ground_idx = MorseFlipperGpioPinP3,
        .gpio_probe_state = MorseFlipperGpioProbeOk,
        .boot_probe = MorseFlipperGpioProbeOk,
        .sk_to_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_DEFAULT_S,
        .sk_gap_s = MORSE_FLIPPER_STRAIGHT_NEXT_DEFAULT_S,
        .trainer_row = 0U,
        .trainer_char_idx = 0U,
        .trainer_mark_idx = 0U,
        .session_wait_draw_s = 0xFFU,
        .about_ok_count = 0U,
        .vail_mode_active = false,
        .vail_speed_active = false,
        .vail_tone_active = false,
        .vail_keyer_mode = MorseKeyerModeStraight,
        .vail_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS,
        .vail_tone_idx = 0U,
        .keyer = {0},
        .tone_idx = 0U,
        .prev_n = 0U,
        .input_mask = 0U,
        .trainer_next_at = 0U,
        .straight_next_at = 0U,
        .straight_wait_started_at = 0U,
        .straight_last_input_at = 0U,
        .straight_mark_started_at = 0U,
        .straight_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS,
        .straight_session_total = 0U,
        .straight_session_good = 0U,
        .session_last_input_at = 0U,
        .session_result_until = 0U,
        .session_next_group_at = 0U,
        .rf_tail_at = 0U,
        .rf_tx_edge_at = 0U,
        .gpio_edge_at = 0U,
        .gpio_probe_notice_until = 0U,
        .paddle_sources = {0U, 0U},
        .note_src = {0U, 0U, 0U},
        .trainer = {0},
        .custom_sets = {0},
        .straight_stats = {0},
        .straight_hist_cnt = {0},
        .straight_hist_sum = {0},
        .straight_worst_line = {0},
        .straight_playback_active = false,
        .sk_play_mark = false,
        .sk_started = false,
        .sk_wait = false,
        .sk_done = false,
        .sk_down = false,
        .rf_man = false,
        .rf_live = false,
        .rf_tx_level = false,
        .rf_tx_gap_flushed = true,
        .gpio_level = false,
        .gpio_gap_flushed = true,
        .sk_mark_i = 0U,
        .rf_edit_digit = 0U,
        .backlight = MorseFlipperBacklightAuto,
        .rf_edit_khz = {0},
        .rf_rx_text = {0},
        .rf_tx_text = {0},
        .gpio_text = {0},
        .audio_pwm = {0},
        .rf = {0},
        .radio = {0},
        .rf_decoder = {0},
        .tx_decoder = {0},
        .gpio_decoder = {0},
        .straight_trainer = {0},
    };

    morse_trainer_init(&app.trainer);
    morse_flipper_rf_init(&app.rf);
    morse_flipper_rf_load_settings(&app.rf, MORSE_FLIPPER_SUBGHZ_SETTINGS_PATH);
    morse_flipper_radio_init(&app.radio);
    morse_flipper_radio_set_rx_callback(&app.radio, morse_flipper_rf_rx_edge, &app);
    morse_flipper_audio_pwm_reset(&app.audio_pwm);
    morse_flipper_cw_decoder_init(&app.rf_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.tx_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.gpio_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_straight_trainer_init(&app.straight_trainer);
    morse_trainer_load_custom_sets(&app.custom_sets);
   morse_trainer_load_straight_stats(&app.straight_stats);
    morse_flipper_pick_charset(&app);
    morse_flipper_load_config(&app);
    morse_flipper_pick_charset(&app);
    morse_flipper_cw_decoder_init(&app.rf_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.tx_decoder, morse_flipper_current_dit_ms(&app));
    morse_flipper_cw_decoder_init(&app.gpio_decoder, morse_flipper_current_dit_ms(&app));
    morse_keyer_init(&app.keyer, app.keyer_mode, morse_flipper_current_dit_ms(&app));
    morse_flipper_gpio_init(&app);
    app.boot_probe = morse_flipper_gpio_probe_sample_raw(&app);
    morse_flipper_set_pc_mode(&app, app.pc_pref);
    app.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app.view_dispatcher, &app);
    view_dispatcher_set_custom_event_callback(app.view_dispatcher, morse_flipper_custom_evt);
    view_dispatcher_set_navigation_event_callback(app.view_dispatcher, morse_flipper_back_evt);
    view_dispatcher_set_tick_event_callback(app.view_dispatcher, morse_flipper_tick_callback, MORSE_FLIPPER_POLL_MS);
    view_dispatcher_attach_to_gui(app.view_dispatcher, app.gui, ViewDispatcherTypeFullscreen);

    app.scene_manager = scene_manager_alloc(&morse_flipper_scene_handlers, &app);

    app.submenu = submenu_alloc();
    view_dispatcher_add_view(app.view_dispatcher, MorseFlipperViewMenu, submenu_get_view(app.submenu));

    app.settings_list = variable_item_list_alloc();
    view_dispatcher_add_view( app.view_dispatcher, MorseFlipperViewSettings, variable_item_list_get_view(app.settings_list));

    app.widget = widget_alloc();
    app.help_text = furi_string_alloc();
    view_dispatcher_add_view(app.view_dispatcher, MorseFlipperViewWidget, widget_get_view(app.widget));

    app.live_view = view_alloc();
    view_set_context(app.live_view, &app);
    view_allocate_model(app.live_view, ViewModelTypeLockFree, sizeof(MorseFlipperLiveModel));
    with_view_model(app.live_view, MorseFlipperLiveModel * m, {
        m->app = &app;
        m->bump = 0U;
    }, false);
    view_set_draw_callback(app.live_view, morse_flipper_live_draw);
    view_set_input_callback(app.live_view, morse_flipper_live_input);
    view_dispatcher_add_view(app.view_dispatcher, MorseFlipperViewLive, app.live_view);

    if(morse_flipper_gpio_probe_any_short(app.boot_probe)) {
        scene_manager_next_scene(app.scene_manager, MorseFlipperSceneStartupProbe);
    } else {
        scene_manager_next_scene(app.scene_manager, MorseFlipperSceneMenuMain);
    }
    return &app;
}

ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app)
{
    if(app == NULL) return NULL;
    return app->view_dispatcher;
}

void morse_flipper_shutdown(MorseFlipperApp* app)
{
    if(app == NULL) return;

    morse_flipper_btn_clear(app, furi_get_tick());
    morse_flipper_set_pc_mode(app, MorseFlipperPcModeOff);
    morse_flipper_radio_sync_live(&app->radio, morse_flipper_rf_frequency_hz(&app->rf), false, false);
    morse_flipper_radio_set_tx_level(&app->radio, false);
    morse_flipper_radio_deinit(&app->radio);
    morse_flipper_audio_pwm_stop(&app->audio_pwm);
    morse_keyer_reset(&app->keyer);
    morse_flipper_drain_keyer_events(app);
    morse_flipper_release_all_notes(app);
    morse_flipper_tone_stop(app);
    if(app->backlight != MorseFlipperBacklightAuto && app->notifications)
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    if(app->notifications) {
        notification_message(app->notifications, &sequence_reset_green);
        notification_message(app->notifications, &sequence_reset_red);
    }
    morse_flipper_save_config(app);

    morse_flipper_gpio_deinit();
    if(app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewWidget);
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewSettings);
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewLive);
        view_dispatcher_remove_view(app->view_dispatcher, MorseFlipperViewMenu);
    }
    if(app->help_text) furi_string_free(app->help_text);
    if(app->widget) widget_free(app->widget);
    if(app->settings_list) variable_item_list_free(app->settings_list);
    if(app->live_view) view_free(app->live_view);
    if(app->submenu) submenu_free(app->submenu);
    if(app->scene_manager) scene_manager_free(app->scene_manager);
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
}

int32_t morse_flipper_fap(void* p)
{
    MorseFlipperApp* app;
    UNUSED(p);

    app = morse_flipper_boot();
    view_dispatcher_run(morse_flipper_view_dispatcher_get(app));
    morse_flipper_shutdown(app);
    return 0;
}
