static void morse_flipper_scene_menu_main_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuMain);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuMain);
    submenu_set_header(app->submenu, "Morse Flipper");
    submenu_add_item(app->submenu, "Free Practice", MorseFlipperSceneRun, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Radio TX & RX", MorseFlipperSceneRf, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Training", MorseFlipperSceneMenuTraining, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Settings", MorseFlipperSceneMenuSettings, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneRun && sel != MorseFlipperSceneRf &&
       sel != MorseFlipperSceneMenuTraining && sel != MorseFlipperSceneMenuSettings)
        sel = MorseFlipperSceneRun;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_menu_main_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuMain, event.event);
        scene_manager_next_scene(app->scene_manager, event.event);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_main_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_menu_training_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuTraining);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuTraining);
    submenu_set_header(app->submenu, "Training");
    submenu_add_item(app->submenu, "Koch - LCWO groups", MorseFlipperSceneSession, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Straight Key trainer", MorseFlipperSceneStraight, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Koch statistics", MorseFlipperSceneBrowse, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneSession && sel != MorseFlipperSceneStraight && sel != MorseFlipperSceneBrowse)
        sel = MorseFlipperSceneSession;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_menu_training_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuTraining, event.event);
        scene_manager_next_scene(app->scene_manager, event.event);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_training_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_menu_settings_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuSettings);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuSettings);
    submenu_set_header(app->submenu, "Settings");
    submenu_add_item(app->submenu, "Main settings", MorseFlipperSceneHome, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Koch - LCWO", MorseFlipperSceneTrainer, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "USB", MorseFlipperScenePc, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Trace menu", MorseFlipperSceneTrace, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneHome && sel != MorseFlipperSceneTrainer && sel != MorseFlipperScenePc &&
       sel != MorseFlipperSceneTrace)
        sel = MorseFlipperSceneHome;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_menu_settings_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuSettings, event.event);
        scene_manager_next_scene(app->scene_manager, event.event);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_settings_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_home_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneHome);
    uint8_t idx;
    char wpm_text[4];

    morse_flipper_scene_enter_now(app, MorseFlipperSceneHome);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_enter_callback, app);

    item = variable_item_list_add(
        app->settings_list,
        "WPM",
        21U,
        morse_flipper_settings_wpm_changed,
        app);
    idx = (uint8_t)(morse_flipper_current_wpm(app) < 10U ? 0U : morse_flipper_current_wpm(app) - 10U);
    if(idx > 20U) {
        idx = 20U;
    }
    snprintf(wpm_text, sizeof(wpm_text), "%u", (unsigned)(idx + 10U));
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, wpm_text);

    item = variable_item_list_add(
        app->settings_list,
        "Input",
        COUNT_OF(morse_flipper_input_values),
        morse_flipper_settings_input_changed,
        app);
    idx = morse_flipper_input_value_index(app->input_source);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, morse_flipper_input_names[idx]);

    item = variable_item_list_add(
        app->settings_list,
        "Keyer",
        COUNT_OF(morse_flipper_keyer_values),
        morse_flipper_settings_keyer_changed,
        app);
    idx = morse_flipper_keyer_value_index(app->keyer_mode);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, morse_keyer_mode_name(morse_flipper_keyer_values[idx]));

    item = variable_item_list_add(
        app->settings_list, "Swap paddles", 2U, morse_flipper_settings_swap_changed, app);
    idx = app->handedness == MorseFlipperHandednessSwapped ? 1U : 0U;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, idx ? "Yes" : "No");

    item = variable_item_list_add(
        app->settings_list,
        "TX tone",
        COUNT_OF(morse_flipper_tones),
        morse_flipper_settings_tone_changed,
        app);
    variable_item_set_current_value_index(item, app->tone_idx);
    variable_item_set_current_value_text(item, morse_flipper_tones[app->tone_idx].name);

    variable_item_list_add(app->settings_list, "GPIO", 0U, NULL, app);
    if(sel > MorseFlipperSettingGpio) {
        sel = MorseFlipperSettingWpm;
    }
    variable_item_list_set_selected_item(app->settings_list, sel);
}

static bool morse_flipper_scene_home_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MorseFlipperSettingGpio) {
            scene_manager_next_scene(app->scene_manager, MorseFlipperSceneGpio);
            return true;
        }
    }

    return false;
}

static void morse_flipper_scene_home_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneHome,
        variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static void morse_flipper_scene_gpio_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneGpio);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneGpio);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);
    app->gpio_edit_straight_idx = app->gpio_straight_idx;
    app->gpio_edit_dit_idx = app->gpio_dit_idx;
    app->gpio_edit_dah_idx = app->gpio_dah_idx;
    app->gpio_edit_ground_idx = app->gpio_ground_idx;

    item = variable_item_list_add(
        app->settings_list,
        "Straight key",
        MORSE_FLIPPER_GPIO_PIN_COUNT,
        morse_flipper_settings_gpio_straight_changed,
        app);
    variable_item_set_current_value_index(item, app->gpio_edit_straight_idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_straight_idx));

    item = variable_item_list_add(
        app->settings_list,
        "Paddle dit",
        MORSE_FLIPPER_GPIO_PIN_COUNT,
        morse_flipper_settings_gpio_dit_changed,
        app);
    variable_item_set_current_value_index(item, app->gpio_edit_dit_idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dit_idx));

    item = variable_item_list_add(
        app->settings_list,
        "Paddle dah",
        MORSE_FLIPPER_GPIO_PIN_COUNT,
        morse_flipper_settings_gpio_dah_changed,
        app);
    variable_item_set_current_value_index(item, app->gpio_edit_dah_idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dah_idx));

    item = variable_item_list_add(
        app->settings_list,
        "Force gnd",
        MORSE_FLIPPER_GPIO_PIN_COUNT,
        morse_flipper_settings_gpio_ground_changed,
        app);
    variable_item_set_current_value_index(item, app->gpio_edit_ground_idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ground_idx));

    if(sel > MorseFlipperGpioSettingGround) {
        sel = MorseFlipperGpioSettingStraight;
    }
    variable_item_list_set_selected_item(app->settings_list, sel);
}

static bool morse_flipper_scene_gpio_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;
    MorseFlipperGpioRule rule = MorseFlipperGpioRuleOk;

    if(event.type == SceneManagerEventTypeBack) {
        if(!morse_flipper_gpio_try_apply(
               app,
               app->gpio_edit_straight_idx,
               app->gpio_edit_dit_idx,
               app->gpio_edit_dah_idx,
               app->gpio_edit_ground_idx,
               &rule)) {
            morse_flipper_gpio_alert(app, rule);
            return true;
        }
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static void morse_flipper_scene_gpio_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneGpio,
        variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static bool morse_flipper_scene_live_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static void morse_flipper_scene_live_on_exit(void* context) {
    UNUSED(context);
}

static void morse_flipper_scene_run_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneRun);
}

static void morse_flipper_scene_rf_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneRf);
}

static void morse_flipper_scene_session_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneSession);
}

static void morse_flipper_scene_straight_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneStraight);
}

static void morse_flipper_scene_browse_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneBrowse);
}

static void morse_flipper_scene_trainer_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneTrainer);
    uint8_t gs;
    uint8_t groups;
    bool dirty = false;
    gs = morse_trainer_group_size(&app->trainer);
    groups = morse_trainer_session_groups(&app->trainer);

    if(gs > 9U) {
        morse_trainer_set_group_size(&app->trainer, 9U);
        dirty = true;
    }
    if(groups < 3U) {
        morse_trainer_set_session_groups(&app->trainer, 3U);
        dirty = true;
    } else if(groups > 30U) {
        morse_trainer_set_session_groups(&app->trainer, 30U);
        dirty = true;
    }
    if(app->trainer.custom_set_idx > app->custom_sets.count) {
        app->trainer.custom_set_idx = 0U;
        morse_flipper_apply_trainer_charset_choice(app);
        dirty = true;
    }
    if(dirty) {
        morse_flipper_save_config(app);
    }

    morse_flipper_scene_enter_now(app, MorseFlipperSceneTrainer);
    variable_item_list_reset(app->settings_list);
    memset(app->trainer_items, 0, sizeof(app->trainer_items));
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);

    item = variable_item_list_add(
        app->settings_list,
        "Lesson",
        (uint8_t)morse_trainer_lesson_count(),
        morse_flipper_trainer_lesson_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingLesson] = item;

    item = variable_item_list_add(app->settings_list, "WPM", 21U, morse_flipper_trainer_wpm_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingWpm] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Farnsworth",
        morse_flipper_local_wpm(app),
        morse_flipper_trainer_farnsworth_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingFarnsworth] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Answer timeout",
        (uint8_t)(MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S -
                  MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S + 1U),
        morse_flipper_trainer_answer_timeout_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingAnswerTimeout] = item;

    item = variable_item_list_add(app->settings_list,
        "Group pause",
        (uint8_t)(MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S - MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S + 1U),
        morse_flipper_trainer_group_pause_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingGroupPause] = item;

    item = variable_item_list_add(
        app->settings_list, "Group size", 9U, morse_flipper_trainer_group_size_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingGroupSize] = item;

    item = variable_item_list_add(
        app->settings_list, "Groups", 28U, morse_flipper_trainer_groups_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingGroups] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Chars",
        (uint8_t)(app->custom_sets.count + 1U),
        morse_flipper_trainer_chars_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingChars] = item;

    if((sel & 0xffU) > MorseFlipperTrainerSettingChars) {
        sel = 0U;
    }
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_settings_list_restore(app->settings_list, sel);
}

static void morse_flipper_scene_pc_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* it;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperScenePc);

    morse_flipper_scene_enter_now(app, MorseFlipperScenePc);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);

    it = variable_item_list_add(
        app->settings_list,
        "Connection",
        COUNT_OF(morse_flipper_usb_mode_names),
        morse_flipper_settings_usb_mode_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_mode_pref);
    variable_item_set_current_value_text(it, morse_flipper_usb_mode_names[app->pc_mode_pref]);

    it = variable_item_list_add(
        app->settings_list,
        "Paddle",
        morse_pc_paddle_preset_count(),
        morse_flipper_settings_usb_paddle_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_paddle_preset);
    variable_item_set_current_value_text(it, morse_pc_paddle_preset_name(app->pc_paddle_preset));

    it = variable_item_list_add(
        app->settings_list,
        "Straight",
        morse_pc_straight_preset_count(),
        morse_flipper_settings_usb_straight_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_straight_preset);
    variable_item_set_current_value_text(it, morse_pc_straight_preset_name(app->pc_straight_preset));

    it = variable_item_list_add(
        app->settings_list, "Invert mouse", 2U, morse_flipper_settings_usb_mouse_swap_changed, app);
    variable_item_set_current_value_index(it, app->mouse_invert ? 1U : 0U);
    variable_item_set_current_value_text(it, app->mouse_invert ? "Yes" : "No");

    if(sel > MorseFlipperUsbSettingMouseSwap) sel = MorseFlipperUsbSettingConnection;
    variable_item_list_set_selected_item(app->settings_list, sel);
}

static void morse_flipper_scene_pc_keys_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperScenePcKeys);
}

static void morse_flipper_scene_trace_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTrace);
}

static void morse_flipper_scene_trainer_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneTrainer,
        morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static bool morse_flipper_scene_pc_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static void morse_flipper_scene_pc_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperScenePc,
        variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static const AppSceneOnEnterCallback morse_flipper_scene_on_enter_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_enter,
    morse_flipper_scene_menu_training_on_enter,
    morse_flipper_scene_menu_settings_on_enter,
    morse_flipper_scene_run_on_enter,
    morse_flipper_scene_rf_on_enter,
    morse_flipper_scene_session_on_enter,
    morse_flipper_scene_straight_on_enter,
    morse_flipper_scene_browse_on_enter,
    morse_flipper_scene_home_on_enter,
    morse_flipper_scene_trainer_on_enter,
    morse_flipper_scene_pc_on_enter,
    morse_flipper_scene_pc_keys_on_enter,
    morse_flipper_scene_trace_on_enter,
    morse_flipper_scene_gpio_on_enter,
};

static const AppSceneOnEventCallback morse_flipper_scene_on_event_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_event,
    morse_flipper_scene_menu_training_on_event,
    morse_flipper_scene_menu_settings_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_home_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_pc_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_gpio_on_event,
};

static const AppSceneOnExitCallback morse_flipper_scene_on_exit_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_exit,
    morse_flipper_scene_menu_training_on_exit,
    morse_flipper_scene_menu_settings_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_home_on_exit,
    morse_flipper_scene_trainer_on_exit,
    morse_flipper_scene_pc_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_gpio_on_exit,
};

static const SceneManagerHandlers morse_flipper_scene_handlers = {
    .on_enter_handlers = morse_flipper_scene_on_enter_handlers,
    .on_event_handlers = morse_flipper_scene_on_event_handlers,
    .on_exit_handlers = morse_flipper_scene_on_exit_handlers,
    .scene_num = MorseFlipperSceneNum,
};
