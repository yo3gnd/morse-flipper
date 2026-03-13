#include "morse_flipper_app_i.h"

static void morse_flipper_scene_menu_main_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuMain);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuMain);
    submenu_set_header(app->submenu, "Morse Flipper");
    submenu_add_item(
        app->submenu,
        "Training",
        MorseFlipperSceneMenuTraining,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "Settings",
        MorseFlipperSceneMenuSettings,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu, "Help", MorseFlipperSceneMenuHelp, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Flipper Radio", MorseFlipperSceneMenuRf, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Free Practice", MorseFlipperSceneRun, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Ham Keyer", MorseFlipperSceneMenuHam, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "About", MorseFlipperSceneAbout, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneRun && sel != MorseFlipperSceneMenuRf &&
       sel != MorseFlipperSceneMenuTraining && sel != MorseFlipperSceneMenuSettings &&
       sel != MorseFlipperSceneMenuHelp && sel != MorseFlipperSceneMenuHam &&
       sel != MorseFlipperSceneAbout)
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
    submenu_add_item(
        app->submenu,
        "Koch - LCWO groups",
        MorseFlipperSceneSession,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "Straight trainer",
        MorseFlipperSceneStraight,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "TX Groups of 5 letters",
        MorseFlipperSceneTxGroups,
        morse_flipper_scene_menu_pick,
        app);
    if(sel != MorseFlipperSceneSession && sel != MorseFlipperSceneStraight &&
       sel != MorseFlipperSceneTxGroups)
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
        scene_manager_set_scene_state(
            app->scene_manager, MorseFlipperSceneMenuTraining, event.event);
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
    submenu_add_item(
        app->submenu, "Keying", MorseFlipperSceneHome, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu,
        "Audio output",
        MorseFlipperSceneAudioCfg,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu, "Koch - LCWO", MorseFlipperSceneTrainer, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu,
        "Straight trainer",
        MorseFlipperSceneStraightCfg,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "Groups of 5",
        MorseFlipperSceneTxGroupsCfg,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(app->submenu, "USB", MorseFlipperScenePc, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneHome && sel != MorseFlipperSceneAudioCfg &&
       sel != MorseFlipperSceneTrainer && sel != MorseFlipperSceneStraightCfg &&
       sel != MorseFlipperSceneTxGroupsCfg && sel != MorseFlipperScenePc)
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
        scene_manager_set_scene_state(
            app->scene_manager, MorseFlipperSceneMenuSettings, event.event);
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
    submenu_add_item(
        app->submenu,
        "First steps",
        MorseFlipperHelpFirstSteps,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "Input & keys",
        MorseFlipperHelpInputKeys,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "Connecting the paddle",
        MorseFlipperHelpConnectingPaddle,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu, "LCWO", MorseFlipperHelpLcwo, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Prepping", MorseFlipperHelpPrepping, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu,
        "A complete Morse contact",
        MorseFlipperHelpContact,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu, "Contesting", MorseFlipperHelpContesting, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu,
        "USB & live practice",
        MorseFlipperHelpUsbLive,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "Moving forward",
        MorseFlipperHelpMovingForward,
        morse_flipper_scene_menu_pick,
        app);
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
    submenu_set_header(app->submenu, "Flipper Radio");
    submenu_add_item(
        app->submenu, "Transmit", MorseFlipperSceneRf, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Receive monitor", MorseFlipperSceneRfRx, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Frequency", MorseFlipperSceneRfFreq, morse_flipper_scene_menu_pick, app);
    if(sel != MorseFlipperSceneRf && sel != MorseFlipperSceneRfRx &&
       sel != MorseFlipperSceneRfFreq)
        sel = MorseFlipperSceneRf;
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

static void morse_flipper_scene_menu_ham_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneMenuHam);
    char logging[24];

    morse_flipper_scene_enter_now(app, MorseFlipperSceneMenuHam);
    submenu_set_header(app->submenu, "Ham Keyer");
    snprintf(
        logging,
        sizeof(logging),
        "Logging: %s",
        app->ham_keyer.logging_enabled ? "On" : "Off");
    submenu_add_item(
        app->submenu, "Start", MorseFlipperHamMenuStart, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, logging, MorseFlipperHamMenuLogging, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu,
        "Configure messages",
        MorseFlipperHamMenuConfigure,
        morse_flipper_scene_menu_pick,
        app);
    submenu_add_item(
        app->submenu,
        "View assignments",
        MorseFlipperHamMenuAssignments,
        morse_flipper_scene_menu_pick,
        app);
    if(sel < MorseFlipperHamMenuStart || sel > MorseFlipperHamMenuAssignments)
        sel = MorseFlipperHamMenuStart;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_menu_ham_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuHam, event.event);
    if(event.event == MorseFlipperHamMenuStart) {
        scene_manager_next_scene(
            app->scene_manager,
            app->input_source == MorseFlipperInputSourceButtons ? MorseFlipperSceneHamStartRefusal :
                                                                  MorseFlipperSceneHamRun);
        return true;
    }

    if(event.event == MorseFlipperHamMenuLogging) {
        app->ham_keyer.logging_enabled = !app->ham_keyer.logging_enabled;
        morse_flipper_save_config(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuHam);
        return true;
    }

    if(event.event == MorseFlipperHamMenuConfigure) {
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneHamConfigure);
        return true;
    }

    if(event.event == MorseFlipperHamMenuAssignments) {
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneHamAssignments);
        return true;
    }

    return true;
}

static void morse_flipper_scene_menu_ham_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_ham_configure_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneHamConfigure);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneHamConfigure);
    submenu_set_header(app->submenu, "Messages");
    submenu_add_item(
        app->submenu, "Add new", MorseFlipperHamConfigureAdd, morse_flipper_scene_menu_pick, app);
    for(uint8_t i = 0U; i < app->ham_keyer.message_count; i++) {
        submenu_add_item(
            app->submenu,
            app->ham_keyer.messages[i],
            MorseFlipperHamConfigureMessageBase + i,
            morse_flipper_scene_menu_pick,
            app);
    }
    if(sel != MorseFlipperHamConfigureAdd &&
       (sel < MorseFlipperHamConfigureMessageBase ||
        sel >= (uint32_t)MorseFlipperHamConfigureMessageBase + app->ham_keyer.message_count))
        sel = MorseFlipperHamConfigureAdd;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_ham_configure_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;
    scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneHamConfigure, event.event);

    if(event.event == MorseFlipperHamConfigureAdd) {
        app->ham_text_mode = MorseFlipperHamTextModeAdd;
        app->ham_text_buffer[0] = '\0';
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneHamTextInput);
        return true;
    }

    if(event.event >= MorseFlipperHamConfigureMessageBase &&
       event.event < (uint32_t)MorseFlipperHamConfigureMessageBase + app->ham_keyer.message_count) {
        app->ham_selected_message = (uint8_t)(event.event - MorseFlipperHamConfigureMessageBase);
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneHamMessageActions);
    }

    return true;
}

static void morse_flipper_scene_ham_configure_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_scene_ham_actions_on_enter(void* context) {
    MorseFlipperApp* app = context;
    uint32_t sel =
        scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneHamMessageActions);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneHamMessageActions);
    submenu_set_header(app->submenu, "Message");
    submenu_add_item(
        app->submenu, "Assign", MorseFlipperHamActionAssign, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Edit", MorseFlipperHamActionEdit, morse_flipper_scene_menu_pick, app);
    submenu_add_item(
        app->submenu, "Delete", MorseFlipperHamActionDelete, morse_flipper_scene_menu_pick, app);
    if(sel < MorseFlipperHamActionAssign || sel > MorseFlipperHamActionDelete)
        sel = MorseFlipperHamActionAssign;
    submenu_set_selected_item(app->submenu, sel);
}

static bool morse_flipper_scene_ham_actions_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;
    scene_manager_set_scene_state(
        app->scene_manager, MorseFlipperSceneHamMessageActions, event.event);

    if(event.event == MorseFlipperHamActionAssign) {
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneHamAssign);
    } else if(event.event == MorseFlipperHamActionEdit) {
        app->ham_text_mode = MorseFlipperHamTextModeEdit;
        scene_manager_next_scene(app->scene_manager, MorseFlipperSceneHamTextInput);
    } else if(event.event == MorseFlipperHamActionDelete) {
        morse_flipper_ham_keyer_delete_message(&app->ham_keyer, app->ham_selected_message);
        morse_flipper_save_config(app);
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager,
               MorseFlipperSceneHamConfigure))
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager,
                MorseFlipperSceneHamConfigure);
    }

    return true;
}

static void morse_flipper_scene_ham_actions_on_exit(void* context) {
    MorseFlipperApp* app = context;
    submenu_reset(app->submenu);
}

static void morse_flipper_ham_text_input_callback(void* context) {
    MorseFlipperApp* app = context;

    if(app == NULL || app->view_dispatcher == NULL) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, MorseFlipperCustomHamTextDone);
}

static void morse_flipper_scene_ham_text_input_on_enter(void* context) {
    MorseFlipperApp* app = context;

    morse_flipper_scene_enter_now(app, MorseFlipperSceneHamTextInput);
    text_input_reset(app->text_input);

    if(app->ham_text_mode == MorseFlipperHamTextModeEdit &&
       app->ham_selected_message < app->ham_keyer.message_count) {
        strncpy(
            app->ham_text_buffer,
            app->ham_keyer.messages[app->ham_selected_message],
            sizeof(app->ham_text_buffer) - 1U);
        app->ham_text_buffer[sizeof(app->ham_text_buffer) - 1U] = '\0';
        text_input_set_header_text(app->text_input, "Edit message");
    } else {
        app->ham_text_buffer[0] = '\0';
        text_input_set_header_text(app->text_input, "Add message");
    }

    text_input_set_result_callback(
        app->text_input,
        morse_flipper_ham_text_input_callback,
        app,
        app->ham_text_buffer,
        sizeof(app->ham_text_buffer),
        app->ham_text_mode == MorseFlipperHamTextModeAdd);
}

static bool morse_flipper_scene_ham_text_input_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom ||
       event.event != MorseFlipperCustomHamTextDone)
        return false;

    for(size_t i = 0U; app->ham_text_buffer[i] != '\0'; i++) {
        if(app->ham_text_buffer[i] == '_') app->ham_text_buffer[i] = ' ';
    }

    if(app->ham_text_mode == MorseFlipperHamTextModeEdit) {
        morse_flipper_ham_keyer_edit_message(
            &app->ham_keyer,
            app->ham_selected_message,
            app->ham_text_buffer);
    } else if(app->ham_text_mode == MorseFlipperHamTextModeAdd) {
        morse_flipper_ham_keyer_add_message(&app->ham_keyer, app->ham_text_buffer);
    }
    morse_flipper_save_config(app);

    if(!scene_manager_search_and_switch_to_previous_scene(
           app->scene_manager,
           MorseFlipperSceneHamConfigure))
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager,
            MorseFlipperSceneHamConfigure);

    return true;
}

static void morse_flipper_scene_ham_text_input_on_exit(void* context) {
    MorseFlipperApp* app = context;

    app->ham_text_mode = MorseFlipperHamTextModeNone;
    text_input_reset(app->text_input);
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

static void morse_flipper_scene_rf_rx_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneRfRx);
}

static void morse_flipper_scene_rf_freq_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneRfFreq);
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

static void morse_flipper_scene_tx_groups_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTxGroups);
}

static void morse_flipper_scene_tx_groups_result_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTxGroupsResult);
}

static void morse_flipper_scene_tx_groups_final_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTxGroupsFinal);
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

static void morse_flipper_scene_ham_run_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneHamRun);
}

static void morse_flipper_scene_ham_refusal_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneHamStartRefusal);
}

static void morse_flipper_scene_ham_assign_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneHamAssign);
}

static void morse_flipper_scene_ham_assignments_on_enter(void* context) {
    MorseFlipperApp* app = context;
    morse_flipper_scene_enter_now(app, MorseFlipperSceneHamAssignments);
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
        app->startup_gpio_probe_state = MorseFlipperGpioProbeOk;
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuMain);
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
    morse_flipper_scene_menu_ham_on_enter,
    morse_flipper_scene_run_on_enter,
    morse_flipper_scene_rf_on_enter,
    morse_flipper_scene_rf_rx_on_enter,
    morse_flipper_scene_rf_freq_on_enter,
    morse_flipper_scene_session_on_enter,
    morse_flipper_scene_straight_on_enter,
    morse_flipper_scene_session_end_on_enter,
    morse_flipper_scene_home_on_enter,
    morse_flipper_scene_audio_cfg_on_enter,
    morse_flipper_scene_trainer_on_enter,
    morse_flipper_scene_straight_cfg_on_enter,
    morse_flipper_scene_pc_on_enter,
    morse_flipper_scene_trace_on_enter,
    morse_flipper_scene_gpio_on_enter,
    morse_flipper_scene_help_on_enter,
    morse_flipper_scene_about_on_enter,
    morse_flipper_scene_startup_probe_on_enter,
    morse_flipper_scene_ham_run_on_enter,
    morse_flipper_scene_ham_refusal_on_enter,
    morse_flipper_scene_ham_configure_on_enter,
    morse_flipper_scene_ham_actions_on_enter,
    morse_flipper_scene_ham_text_input_on_enter,
    morse_flipper_scene_ham_assign_on_enter,
    morse_flipper_scene_ham_assignments_on_enter,
    morse_flipper_scene_tx_groups_on_enter,
    morse_flipper_scene_tx_groups_result_on_enter,
    morse_flipper_scene_tx_groups_final_on_enter,
    morse_flipper_scene_tx_groups_cfg_on_enter,
};

static const AppSceneOnEventCallback morse_flipper_scene_on_event_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_event,
    morse_flipper_scene_menu_training_on_event,
    morse_flipper_scene_menu_settings_on_event,
    morse_flipper_scene_menu_help_on_event,
    morse_flipper_scene_menu_rf_on_event,
    morse_flipper_scene_menu_ham_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_home_on_event,
    morse_flipper_scene_audio_cfg_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_pc_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_gpio_on_event,
    morse_flipper_scene_help_on_event,
    morse_flipper_scene_about_on_event,
    morse_flipper_scene_startup_probe_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_ham_configure_on_event,
    morse_flipper_scene_ham_actions_on_event,
    morse_flipper_scene_ham_text_input_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_live_on_event,
    morse_flipper_scene_tx_groups_cfg_on_event,
};

static const AppSceneOnExitCallback morse_flipper_scene_on_exit_handlers[MorseFlipperSceneNum] = {
    morse_flipper_scene_menu_main_on_exit,
    morse_flipper_scene_menu_training_on_exit,
    morse_flipper_scene_menu_settings_on_exit,
    morse_flipper_scene_menu_help_on_exit,
    morse_flipper_scene_menu_rf_on_exit,
    morse_flipper_scene_menu_ham_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_home_on_exit,
    morse_flipper_scene_audio_cfg_on_exit,
    morse_flipper_scene_trainer_on_exit,
    morse_flipper_scene_straight_cfg_on_exit,
    morse_flipper_scene_pc_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_gpio_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_ham_configure_on_exit,
    morse_flipper_scene_ham_actions_on_exit,
    morse_flipper_scene_ham_text_input_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_live_on_exit,
    morse_flipper_scene_tx_groups_cfg_on_exit,
};

const SceneManagerHandlers morse_flipper_scene_handlers = {
    .on_enter_handlers = morse_flipper_scene_on_enter_handlers,
    .on_event_handlers = morse_flipper_scene_on_event_handlers,
    .on_exit_handlers = morse_flipper_scene_on_exit_handlers,
    .scene_num = MorseFlipperSceneNum,
};
