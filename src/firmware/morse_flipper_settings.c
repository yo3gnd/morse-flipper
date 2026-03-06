static uint32_t morse_flipper_settings_list_state(VariableItemList* list) {
    MorseFlipperVilModel* model;
    View* view;
    uint8_t row;
    uint32_t state;

    if(list == NULL) return 0U;

    view = variable_item_list_get_view(list);
    model = view_get_model(view);
    row = model->position >= model->window_position ?
              (uint8_t)(model->position - model->window_position) :
              0U;
    if(row > 3U) row = 3U;
    state = (uint32_t)model->position | ((uint32_t)row << 8);
    view_commit_model(view, false);
    return state;
}

static void morse_flipper_settings_list_restore(VariableItemList* list, uint32_t state) {
    View* view;
    uint8_t pos;
    uint8_t row;

    if(list == NULL) return;

    pos = (uint8_t)(state & 0xffU);
    row = (uint8_t)((state >> 8) & 0xffU);
    variable_item_list_set_selected_item(list, pos);

    view = variable_item_list_get_view(list);
    with_view_model(
        view,
        MorseFlipperVilModel * model,
        {
            uint8_t count = MorseFlipperVilArray_size(model->items);
            uint8_t win = 0U;

            if(count == 0U) {
                model->position = 0U;
                model->window_position = 0U;
            } else {
                if(pos >= count) pos = 0U;
                if(row > 3U) row = 1U;
                if(row > pos) row = pos;
                if(count > 4U) {
                    uint8_t max_win = (uint8_t)(count - 4U);
                    win = (uint8_t)(pos - row);
                    if(win > max_win) win = max_win;
                }
                model->position = pos;
                model->window_position = win;
            }
        },
        true);
}

static void morse_flipper_trainer_sync_farn_item(MorseFlipperApp* app) {
    VariableItem* it;
    char txt[4];
    uint8_t idx;
    uint8_t wpm;

    if(app == NULL) return;

    morse_flipper_clamp_trainer_settings(app);
    it = app->trainer_items[MorseFlipperTrainerSettingFarnsworth];
    if(it == NULL) return;

    wpm = morse_flipper_local_wpm(app);
    if(wpm == 0U) wpm = 1U;
    variable_item_set_values_count(it, wpm);
    idx = app->trainer_farnsworth_wpm > 0U ? (uint8_t)(app->trainer_farnsworth_wpm - 1U) : 0U;
    variable_item_set_current_value_index(it, idx);
    snprintf(txt, sizeof(txt), "%u", (unsigned)app->trainer_farnsworth_wpm);
    variable_item_set_current_value_text(it, txt);
}

static void morse_flipper_trainer_menu_refresh(MorseFlipperApp* app) {
    VariableItem* it;
    char txt[16];
    uint8_t idx;

    if(app == NULL) return;

    morse_flipper_clamp_trainer_settings(app);

    it = app->trainer_items[MorseFlipperTrainerSettingLesson];
    if(it) {
        idx = morse_trainer_lesson(&app->trainer);
        idx = idx > 0U ? (uint8_t)(idx - 1U) : 0U;
        morse_trainer_lesson_label((uint8_t)(idx + 1U), txt, sizeof(txt));
        variable_item_set_current_value_index(it, idx);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingWpm];
    if(it) {
        idx = (uint8_t)(morse_flipper_local_wpm(app) - 10U);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 10U));
        variable_item_set_current_value_index(it, idx);
        variable_item_set_current_value_text(it, txt);
    }

    morse_flipper_trainer_sync_farn_item(app);

    it = app->trainer_items[MorseFlipperTrainerSettingAnswerTimeout];
    if(it) {
        idx = (uint8_t)(app->trainer_answer_timeout_s - MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->trainer_answer_timeout_s);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingGroupPause];
    if(it) {
        idx = (uint8_t)(app->trainer_group_pause_s - MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->trainer_group_pause_s);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingGroupSize];
    if(it) {
        idx = (uint8_t)(morse_trainer_group_size(&app->trainer) - 1U);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 1U));
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingGroups];
    if(it) {
        idx = (uint8_t)(morse_trainer_session_groups(&app->trainer) - 3U);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 3U));
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingChars];
    if(it) {
        idx = app->trainer.custom_set_idx;
        variable_item_set_current_value_index(it, idx);
        variable_item_set_current_value_text( it, idx == 0U ? "lesson" : app->custom_sets.sets[idx - 1U].name);
    }
}

static void morse_flipper_settings_enter_callback(void* context, uint32_t index) {
    MorseFlipperApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void morse_flipper_settings_noop_enter(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

static void morse_flipper_settings_wpm_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t wpm = (uint8_t)(10U + idx);
    char text[4];

    snprintf(text, sizeof(text), "%u", (unsigned)wpm);
    variable_item_set_current_value_text(item, text);
    morse_flipper_set_local_wpm(app, wpm);
    morse_flipper_trainer_sync_farn_item(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_input_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t now_ms = furi_get_tick();

    variable_item_set_current_value_text(item, morse_flipper_input_names[idx]);
    app->input_source = morse_flipper_input_values[idx];
    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_keyer_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t now_ms = furi_get_tick();

    app->keyer_mode = morse_flipper_keyer_values[idx];
    variable_item_set_current_value_text(item, morse_keyer_mode_name(app->keyer_mode));
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_swap_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t now_ms = furi_get_tick();

    app->handedness = idx == 0U ? MorseFlipperHandednessNormal : MorseFlipperHandednessSwapped;
    variable_item_set_current_value_text( item, app->handedness == MorseFlipperHandednessSwapped ? "Yes" : "No");
    morse_flipper_resync_button_paddles(app, now_ms);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_tone_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->tone_idx = idx == 0U ? MORSE_FLIPPER_TONE_OFF_IDX : (uint8_t)(idx - 1U);
    variable_item_set_current_value_text(item, morse_flipper_current_tone_name(app));
    app->preview_ticks = MORSE_FLIPPER_PREVIEW_TICKS;

    morse_flipper_update_sidetone(app);
    morse_flipper_save_config(app);
}

static uint8_t morse_flipper_p2_volume_idx(uint8_t pct)
{
    if(pct < 10U) pct = 10U;
    if(pct > 100U) pct = 100U;
    return (uint8_t)((pct - 10U) / 5U);
}

static uint8_t morse_flipper_p2_volume_from_idx(uint8_t idx)
{
    return (uint8_t)(10U + (idx * 5U));
}

static void morse_flipper_audio_menu_refresh(MorseFlipperApp* app)
{
    VariableItem* item;
    char txt[8];

    if(app == NULL) return;

    item = app->audio_cfg_items[MorseFlipperAudioSettingPath];
    if(item) {
        variable_item_set_current_value_index(item, app->audio_path);
        variable_item_set_current_value_text(item, morse_flipper_audio_path_names[app->audio_path]);
    }

    item = app->audio_cfg_items[MorseFlipperAudioSettingTone];
    if(item) {
        variable_item_set_current_value_index(
            item,
            app->tone_idx == MORSE_FLIPPER_TONE_OFF_IDX ? 0U : (uint8_t)(app->tone_idx + 1U));
        variable_item_set_current_value_text(item, morse_flipper_current_tone_name(app));
    }

    item = app->audio_cfg_items[MorseFlipperAudioSettingP2Volume];
    if(item) {
        variable_item_set_current_value_index(item, morse_flipper_p2_volume_idx(app->p2_volume_pct));
        snprintf(txt, sizeof(txt), "%u%%", (unsigned)morse_flipper_p2_volume_pct(app));
        variable_item_set_current_value_text(item, txt);
    }
}

static void morse_flipper_settings_audio_path_changed(VariableItem* item)
{
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > MorseFlipperAudioPathGpioP2Hd) idx = MorseFlipperAudioPathBuzzer;
    app->audio_path = idx;
    variable_item_set_current_value_text(item, morse_flipper_audio_path_names[idx]);
    morse_flipper_sync_audio_output(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_p2_volume_changed(VariableItem* item)
{
    MorseFlipperApp* app = variable_item_get_context(item);
    char txt[8];

    app->p2_volume_pct = morse_flipper_p2_volume_from_idx(variable_item_get_current_value_index(item));
    snprintf(txt, sizeof(txt), "%u%%", (unsigned)app->p2_volume_pct);
    variable_item_set_current_value_text(item, txt);
    if(morse_flipper_audio_output_is_pwm(app)) morse_flipper_sync_audio_output(app);
    morse_flipper_save_config(app);
}

static uint8_t morse_flipper_gpio_ui_pin(uint8_t idx) {
    return (uint8_t)(idx + 1U);
}

static uint8_t morse_flipper_gpio_pin_ui(uint8_t pin) {
    if(pin == MorseFlipperGpioPinP2 || pin >= MORSE_FLIPPER_GPIO_PIN_COUNT) return 0U;
    return (uint8_t)(pin - 1U);
}

static void morse_flipper_settings_gpio_dit_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->gpio_edit_dit_idx = morse_flipper_gpio_ui_pin(idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dit_idx));
}

static void morse_flipper_settings_gpio_dah_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->gpio_edit_dah_idx = morse_flipper_gpio_ui_pin(idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dah_idx));
}

static uint8_t morse_flipper_ground_choice_from_ui(uint8_t idx) {
    if(idx == 0U) {
        return MORSE_FLIPPER_GPIO_PIN_NONE;
    }

    return morse_flipper_gpio_ui_pin((uint8_t)(idx - 1U));
}

static uint8_t morse_flipper_ground_choice_to_ui(uint8_t idx) {
    if(idx == MORSE_FLIPPER_GPIO_PIN_NONE) {
        return 0U;
    }

    return (uint8_t)(morse_flipper_gpio_pin_ui(idx) + 1U);
}

static void morse_flipper_settings_gpio_ground_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = morse_flipper_ground_choice_from_ui(variable_item_get_current_value_index(item));

    app->gpio_edit_ground_idx = idx;
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ground_idx));
}

static uint8_t morse_flipper_ptt_choice_from_ui(uint8_t idx) {
    return idx == 1U ? MorseFlipperGpioPinP15 : MORSE_FLIPPER_GPIO_PIN_NONE;
}

static uint8_t morse_flipper_ptt_choice_to_ui(uint8_t idx) {
    return idx == MorseFlipperGpioPinP15 ? 1U : 0U;
}

static void morse_flipper_settings_gpio_ptt_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);

    app->gpio_edit_ptt_idx = morse_flipper_ptt_choice_from_ui(variable_item_get_current_value_index(item));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ptt_idx));
}

static void morse_flipper_settings_usb_mode_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > MorseFlipperPcModeMidi) idx = MorseFlipperPcModeOff;

    variable_item_set_current_value_text(item, morse_flipper_usb_mode_names[idx]);
    app->pc_mode_pref = idx;
    morse_flipper_set_pc_mode(app, idx);
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_usb_paddle_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);

    app->pc_paddle_preset = v;
    variable_item_set_current_value_text(item, morse_pc_paddle_preset_name(v));
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_usb_straight_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t pick = variable_item_get_current_value_index(item);

    app->pc_straight_preset = pick;
    variable_item_set_current_value_text(item, morse_pc_straight_preset_name(pick));
    morse_flipper_save_config(app);
}

static void morse_flipper_settings_usb_mouse_swap_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->mouse_invert = idx != 0U;
    variable_item_set_current_value_text(item, app->mouse_invert ? "Yes" : "No");
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_lesson_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    char buf[16];

    morse_trainer_set_lesson(&app->trainer, (uint8_t)(idx + 1U));
    morse_trainer_lesson_label(morse_trainer_lesson(&app->trainer), buf, sizeof(buf));
    variable_item_set_current_value_text(item, buf);
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_wpm_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t w = (uint8_t)(10U + idx);

    morse_flipper_set_local_wpm(app, w);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_farnsworth_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->trainer_farnsworth_wpm = (uint8_t)(idx + 1U);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_answer_timeout_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);

    app->trainer_answer_timeout_s = (uint8_t)(MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S + i);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_group_pause_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);

    app->trainer_group_pause_s = (uint8_t)(MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S + i);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_straight_menu_refresh(MorseFlipperApp* app)
{
    VariableItem* it;
    char txt[4];
    uint8_t idx;

    if(app == NULL) return;

    morse_flipper_clamp_straight_settings(app);

    it = app->straight_cfg_items[0];
    if(it) {
        idx = (uint8_t)(morse_flipper_straight_wpm(app) - 10U);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 10U));
        variable_item_set_current_value_text(it, txt);
    }

    it = app->straight_cfg_items[1];
    if(it) {
        idx = (uint8_t)(app->straight_answer_timeout_s - MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->straight_answer_timeout_s);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->straight_cfg_items[2];
    if(it) {
        idx = (uint8_t)(app->straight_next_delay_s - MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->straight_next_delay_s);
        variable_item_set_current_value_text(it, txt);
    }
}

static void morse_flipper_straight_wpm_changed(VariableItem* item)
{
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t w = (uint8_t)(10U + idx);

    morse_flipper_set_straight_wpm(app, w);
    morse_flipper_straight_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_straight_timeout_changed(VariableItem* item)
{
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->straight_answer_timeout_s = (uint8_t)(MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S + idx);
    morse_flipper_straight_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_straight_next_changed(VariableItem* item)
{
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->straight_next_delay_s = (uint8_t)(MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S + idx);
    morse_flipper_straight_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_group_size_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    char txt[4];

    morse_trainer_set_group_size(&app->trainer, (uint8_t)(idx + 1U));
    snprintf(txt, sizeof(txt), "%u", (unsigned)morse_trainer_group_size(&app->trainer));
    variable_item_set_current_value_text(item, txt);
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_groups_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    char tmp[4];

    morse_trainer_set_session_groups(&app->trainer, (uint8_t)(idx + 3U));
    snprintf(tmp, sizeof(tmp), "%u", (unsigned)morse_trainer_session_groups(&app->trainer));
    variable_item_set_current_value_text(item, tmp);
    morse_flipper_save_config(app);
}

static void morse_flipper_trainer_chars_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->trainer.custom_set_idx = idx;
    morse_flipper_apply_trainer_charset_choice(app);
    variable_item_set_current_value_text( item, idx == 0U ? "lesson" : app->custom_sets.sets[idx - 1U].name);
    morse_flipper_save_config(app);
}

static void morse_flipper_scene_home_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneHome);
    uint8_t idx;
    char wpm_text[4];

    morse_flipper_scene_enter_now(app, MorseFlipperSceneHome);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback( app->settings_list, morse_flipper_settings_enter_callback, app);

    item = variable_item_list_add( app->settings_list, "WPM", 21U, morse_flipper_settings_wpm_changed, app);
    idx = (uint8_t)(morse_flipper_current_wpm(app) < 10U ? 0U : morse_flipper_current_wpm(app) - 10U);
    if(idx > 20U) idx = 20U;
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

    item = variable_item_list_add( app->settings_list, "Swap paddles", 2U, morse_flipper_settings_swap_changed, app);
    idx = app->handedness == MorseFlipperHandednessSwapped ? 1U : 0U;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, idx ? "Yes" : "No");

    variable_item_list_add(app->settings_list, "Audio output", 0U, NULL, app);

    variable_item_list_add(app->settings_list, "GPIO", 0U, NULL, app);
    if(sel > MorseFlipperSettingGpio) sel = MorseFlipperSettingWpm;
    variable_item_list_set_selected_item(app->settings_list, sel);
}

static bool morse_flipper_scene_home_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MorseFlipperSettingAudio) {
            scene_manager_next_scene(app->scene_manager, MorseFlipperSceneAudioCfg);
            return true;
        }
        if(event.event == MorseFlipperSettingGpio) {
            scene_manager_next_scene(app->scene_manager, MorseFlipperSceneGpio);
            return true;
        }
    }

    return false;
}

static void morse_flipper_scene_home_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state( app->scene_manager, MorseFlipperSceneHome, variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static void morse_flipper_scene_audio_cfg_on_enter(void* context)
{
    MorseFlipperApp* app = context;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneAudioCfg);
    VariableItem* item;

    morse_flipper_scene_enter_now(app, MorseFlipperSceneAudioCfg);
    variable_item_list_reset(app->settings_list);
    memset(app->audio_cfg_items, 0, sizeof(app->audio_cfg_items));
    variable_item_list_set_enter_callback(app->settings_list, morse_flipper_settings_noop_enter, app);

    item = variable_item_list_add(
        app->settings_list,
        "Audio path",
        2U,
        morse_flipper_settings_audio_path_changed,
        app);
    app->audio_cfg_items[MorseFlipperAudioSettingPath] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Buzzer TX",
        COUNT_OF(morse_flipper_tones) + 1U,
        morse_flipper_settings_tone_changed,
        app);
    app->audio_cfg_items[MorseFlipperAudioSettingTone] = item;

    item = variable_item_list_add(
        app->settings_list,
        "P2 Volume",
        19U,
        morse_flipper_settings_p2_volume_changed,
        app);
    app->audio_cfg_items[MorseFlipperAudioSettingP2Volume] = item;

    if((sel & 0xffU) > MorseFlipperAudioSettingP2Volume) sel = 0U;
    morse_flipper_audio_menu_refresh(app);
    morse_flipper_settings_list_restore(app->settings_list, sel);
}

static bool morse_flipper_scene_audio_cfg_on_event(void* context, SceneManagerEvent event)
{
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static void morse_flipper_scene_audio_cfg_on_exit(void* context)
{
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneAudioCfg,
        morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static void morse_flipper_scene_gpio_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneGpio);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneGpio);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback( app->settings_list, morse_flipper_settings_noop_enter, app);
    app->gpio_edit_dit_idx = app->gpio_dit_idx;
    app->gpio_edit_dah_idx = app->gpio_dah_idx;
    app->gpio_edit_ground_idx = app->gpio_ground_idx;
    app->gpio_edit_ptt_idx = app->gpio_ptt_idx;

    item = variable_item_list_add(
        app->settings_list,
        "dit/SK",
        MORSE_FLIPPER_GPIO_PIN_COUNT - 1U,
        morse_flipper_settings_gpio_dit_changed,
        app);
    variable_item_set_current_value_index(item, morse_flipper_gpio_pin_ui(app->gpio_edit_dit_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dit_idx));

    item = variable_item_list_add(
        app->settings_list,
        "dah",
        MORSE_FLIPPER_GPIO_PIN_COUNT - 1U,
        morse_flipper_settings_gpio_dah_changed,
        app);
    variable_item_set_current_value_index(item, morse_flipper_gpio_pin_ui(app->gpio_edit_dah_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dah_idx));

    item = variable_item_list_add(
        app->settings_list,
        "Force gnd",
        MORSE_FLIPPER_GPIO_PIN_COUNT,
        morse_flipper_settings_gpio_ground_changed,
        app);
    variable_item_set_current_value_index( item, morse_flipper_ground_choice_to_ui(app->gpio_edit_ground_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ground_idx));

    item = variable_item_list_add(
        app->settings_list, "PTT/TX", 2U, morse_flipper_settings_gpio_ptt_changed, app);
    variable_item_set_current_value_index(item, morse_flipper_ptt_choice_to_ui(app->gpio_edit_ptt_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ptt_idx));

    if(sel > MorseFlipperGpioSettingPtt) sel = MorseFlipperGpioSettingDit;
    variable_item_list_set_selected_item(app->settings_list, sel);
}

static bool morse_flipper_scene_gpio_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;
    MorseFlipperGpioRule rule = MorseFlipperGpioRuleOk;

    if(event.type == SceneManagerEventTypeBack) {
        if(!morse_flipper_gpio_try_apply(
               app,
               app->gpio_edit_dit_idx,
               app->gpio_edit_dah_idx,
               app->gpio_edit_ground_idx,
               app->gpio_edit_ptt_idx,
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
    scene_manager_set_scene_state( app->scene_manager, MorseFlipperSceneGpio, variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
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
    if(dirty) morse_flipper_save_config(app);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneTrainer);
    variable_item_list_reset(app->settings_list);
    memset(app->trainer_items, 0, sizeof(app->trainer_items));
    variable_item_list_set_enter_callback( app->settings_list, morse_flipper_settings_noop_enter, app);

    item = variable_item_list_add(
        app->settings_list,
        "Lesson",
        (uint8_t)morse_trainer_lesson_count(),
        morse_flipper_trainer_lesson_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingLesson] = item;

    item = variable_item_list_add( app->settings_list, "WPM", 21U, morse_flipper_trainer_wpm_changed, app);
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

    item = variable_item_list_add(
        app->settings_list,
        "Group pause",
        (uint8_t)(MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S -
                  MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S + 1U),
        morse_flipper_trainer_group_pause_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingGroupPause] = item;

    item = variable_item_list_add( app->settings_list, "Group size", 9U, morse_flipper_trainer_group_size_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingGroupSize] = item;

    item = variable_item_list_add( app->settings_list, "Groups", 28U, morse_flipper_trainer_groups_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingGroups] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Chars",
        (uint8_t)(app->custom_sets.count + 1U),
        morse_flipper_trainer_chars_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingChars] = item;

    if((sel & 0xffU) > MorseFlipperTrainerSettingChars) sel = 0U;
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_settings_list_restore(app->settings_list, sel);
}

static void morse_flipper_scene_trainer_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state( app->scene_manager, MorseFlipperSceneTrainer, morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static void morse_flipper_scene_straight_cfg_on_enter(void* context)
{
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneStraightCfg);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneStraightCfg);
    variable_item_list_reset(app->settings_list);
    memset(app->straight_cfg_items, 0, sizeof(app->straight_cfg_items));
    variable_item_list_set_enter_callback( app->settings_list, morse_flipper_settings_noop_enter, app);

        item = variable_item_list_add( app->settings_list, "WPM", 21U, morse_flipper_straight_wpm_changed, app);
    app->straight_cfg_items[0] = item;
    variable_item_set_current_value_index(item, 0U);
    variable_item_set_current_value_text(item, "10");

    item = variable_item_list_add(
        app->settings_list,
        "Answer timeout",
        (uint8_t)(MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S -
                  MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S + 1U),
        morse_flipper_straight_timeout_changed,
        app);
    app->straight_cfg_items[1] = item;
    variable_item_set_current_value_index(item, 0U);
    variable_item_set_current_value_text(item, "3");

    item = variable_item_list_add(
        app->settings_list,
        "Next delay",
        (uint8_t)(MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S -
                  MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S + 1U),
        morse_flipper_straight_next_changed,
        app);
    app->straight_cfg_items[2] = item;
    variable_item_set_current_value_index(item, 0U);
    variable_item_set_current_value_text(item, "3");

    morse_flipper_straight_menu_refresh(app);
    if((sel & 0xffU) > 2U) sel = 0U;
    morse_flipper_settings_list_restore(app->settings_list, sel);
}

static void morse_flipper_scene_straight_cfg_on_exit(void* context)
{
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state( app->scene_manager, MorseFlipperSceneStraightCfg, morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

static void morse_flipper_scene_pc_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* it;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperScenePc);

    morse_flipper_scene_enter_now(app, MorseFlipperScenePc);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback( app->settings_list, morse_flipper_settings_noop_enter, app);

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
        "Paddle keys",
        morse_pc_paddle_preset_count(),
        morse_flipper_settings_usb_paddle_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_paddle_preset);
    variable_item_set_current_value_text(it, morse_pc_paddle_preset_name(app->pc_paddle_preset));

    it = variable_item_list_add(
        app->settings_list,
        "Straight key",
        morse_pc_straight_preset_count(),
        morse_flipper_settings_usb_straight_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_straight_preset);
    variable_item_set_current_value_text(it, morse_pc_straight_preset_name(app->pc_straight_preset));

    it = variable_item_list_add( app->settings_list, "Invert mouse", 2U, morse_flipper_settings_usb_mouse_swap_changed, app);
    variable_item_set_current_value_index(it, app->mouse_invert ? 1U : 0U);
    variable_item_set_current_value_text(it, app->mouse_invert ? "Yes" : "No");

    if(sel > MorseFlipperUsbSettingMouseSwap) sel = MorseFlipperUsbSettingConnection;
    variable_item_list_set_selected_item(app->settings_list, sel);
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
    scene_manager_set_scene_state( app->scene_manager, MorseFlipperScenePc, variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}
