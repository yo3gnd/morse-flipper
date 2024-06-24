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

static void morse_flipper_scene_pc_keys_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperScenePcKeys);
}

static void morse_flipper_scene_trace_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTrace);
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
