static void morse_flipper_scene_menu_main_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuMain);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuMain);
    submenu_set_header(app->submenu, "Morse Flipper");
    submenu_add_item(app->submenu, "Training", MorseFlipperSceneMenuTraining, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Settings", MorseFlipperSceneMenuSettings, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Help", MorseFlipperSceneMenuHelp, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Radio TX", MorseFlipperSceneMenuRf, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Free Practice", MorseFlipperSceneRun, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "About", MorseFlipperSceneAbout, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneRun && sel != MorseFlipperSceneMenuRf &&
       sel != MorseFlipperSceneMenuTraining && sel != MorseFlipperSceneMenuSettings &&
       sel != MorseFlipperSceneMenuHelp && sel != MorseFlipperSceneAbout)
        sel = MorseFlipperSceneMenuTraining;
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
    if(sel != MorseFlipperSceneSession && sel != MorseFlipperSceneStraight)
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
    submenu_add_item(app->submenu, "Straight key", MorseFlipperSceneStraightCfg, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "USB", MorseFlipperScenePc, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneHome && sel != MorseFlipperSceneTrainer &&
       sel != MorseFlipperSceneStraightCfg && sel != MorseFlipperScenePc)
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

static void morse_flipper_scene_menu_help_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuHelp);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuHelp);
    submenu_set_header(app->submenu, "Help");
    submenu_add_item(app->submenu, "First steps", MorseFlipperHelpFirstSteps, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Input & keys", MorseFlipperHelpInputKeys, morse_flipper_scene_menu_pick, app);
    submenu_add_item( app->submenu, "Connecting the paddle", MorseFlipperHelpConnectingPaddle, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "LCWO", MorseFlipperHelpLcwo, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Prepping", MorseFlipperHelpPrepping, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "A complete Morse contact", MorseFlipperHelpContact, morse_flipper_scene_menu_pick, app);
    submenu_add_item(app->submenu, "Contesting", MorseFlipperHelpContesting, morse_flipper_scene_menu_pick, app);
    submenu_add_item( app->submenu, "USB & live practice", MorseFlipperHelpUsbLive, morse_flipper_scene_menu_pick, app);
    submenu_add_item( app->submenu, "Moving forward", MorseFlipperHelpMovingForward, morse_flipper_scene_menu_pick, app);
    if(sel >= MorseFlipperHelpCount) sel = MorseFlipperHelpFirstSteps;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_menu_help_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        app->help_topic = event.event;
        app->help_page = 0U;
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuHelp, event.event);
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneHelp);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_help_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_menu_rf_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuRf);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuRf);
    submenu_set_header(app->submenu, "Radio TX");
    submenu_add_item( app->submenu, "Start transmitting", MorseFlipperSceneRf, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneRf) sel = MorseFlipperSceneRf;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_menu_rf_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuRf, event.event);
        scene_manager_next_scene(app->scene_manager, event.event);
        return true;
    }

    return false;
}

static void morse_flipper_scene_menu_rf_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
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

static void morse_flipper_scene_session_end_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneSessionEnd);
}

static void morse_flipper_scene_trace_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTrace);
}

static void morse_flipper_scene_help_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneHelp);
    morse_flipper_help_open(app);
}

static void morse_flipper_scene_about_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneAbout);
    app->about_ok_count = 0U;
    morse_flipper_about_open(app);
}

static void morse_flipper_scene_startup_probe_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneStartupProbe);
}

static bool morse_flipper_scene_help_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;
    uint8_t n;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;
    n = morse_flipper_help_card_count(app->help_topic);
    if(event.event == MorseFlipperCustomHelpPrev) {
        if(app->help_page > 0U) {
            app->help_page--;
            morse_flipper_help_open(app);
        }
        return true;
    }

    if(event.event == MorseFlipperCustomHelpNext) {
        if(app->help_page + 1U < n) {
            app->help_page++;
            morse_flipper_help_open(app);
        }
        return true;
    }

    return false;
}

static bool morse_flipper_scene_about_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_scene_startup_probe_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        app->boot_probe = MorseFlipperGpioProbeOk;
        scene_manager_search_and_switch_to_another_scene( app->scene_manager, MorseFlipperSceneMenuMain);
        return true;
    }

    return false;
}

static const AppSceneOnEnterCallback morse_flipper_scene_on_enter_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_enter,
    morse_flipper_scene_menu_training_on_enter,
    morse_flipper_scene_menu_settings_on_enter,
    morse_flipper_scene_menu_help_on_enter,
    morse_flipper_scene_menu_rf_on_enter,
    morse_flipper_scene_run_on_enter,
    morse_flipper_scene_rf_on_enter,
    morse_flipper_scene_session_on_enter,
    morse_flipper_scene_straight_on_enter,
    morse_flipper_scene_session_end_on_enter,
    morse_flipper_scene_home_on_enter,
    morse_flipper_scene_trainer_on_enter,
    morse_flipper_scene_straight_cfg_on_enter,
    morse_flipper_scene_pc_on_enter,
    morse_flipper_scene_trace_on_enter,
    morse_flipper_scene_gpio_on_enter,
    morse_flipper_scene_help_on_enter,
    morse_flipper_scene_about_on_enter,
    morse_flipper_scene_startup_probe_on_enter,
};

static const AppSceneOnEventCallback morse_flipper_scene_on_event_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_event,
    morse_flipper_scene_menu_training_on_event,
    morse_flipper_scene_menu_settings_on_event,
    morse_flipper_scene_menu_help_on_event,
    morse_flipper_scene_menu_rf_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_home_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_pc_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_gpio_on_event,
    morse_flipper_scene_help_on_event,
    morse_flipper_scene_about_on_event,
    morse_flipper_scene_startup_probe_on_event,
};

static const AppSceneOnExitCallback morse_flipper_scene_on_exit_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_exit,
    morse_flipper_scene_menu_training_on_exit,
    morse_flipper_scene_menu_settings_on_exit,
    morse_flipper_scene_menu_help_on_exit,
    morse_flipper_scene_menu_rf_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_home_on_exit,
    morse_flipper_scene_trainer_on_exit,
    morse_flipper_scene_straight_cfg_on_exit,
    morse_flipper_scene_pc_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_gpio_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
};

static const SceneManagerHandlers morse_flipper_scene_handlers = {
    .on_enter_handlers = morse_flipper_scene_on_enter_handlers,
    .on_event_handlers = morse_flipper_scene_on_event_handlers,
    .on_exit_handlers = morse_flipper_scene_on_exit_handlers,
    .scene_num = MorseFlipperSceneNum,
};
