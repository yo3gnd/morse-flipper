static bool morse_flipper_help_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenHelp) return false;

    if(event->key == InputKeyLeft && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->help_topic = app->help_topic == 0U ? (MorseFlipperHelpCount - 1U) : app->help_topic - 1U;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyRight && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            app->help_topic = (app->help_topic + 1U) % MorseFlipperHelpCount;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyBack && (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_about_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenAbout) return false;

    if(event->key == InputKeyBack && (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_pc_keys_input(MorseFlipperApp* app, const InputEvent* event)
{
    if(app->screen != MorseFlipperScreenPcKeys) return false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->pc_keys_row = app->pc_keys_row == 0U ? 1U : 0U;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyDown &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app->pc_keys_row = app->pc_keys_row == 0U ? 1U : 0U;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        morse_flipper_cycle_pc_key_preset(app, -1);
        return true;
    }

    if(event->key == InputKeyRight &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        morse_flipper_cycle_pc_key_preset(app, 1);
        return true;
    }

    if((event->key == InputKeyOk || event->key == InputKeyBack) &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

static bool morse_flipper_pc_input(MorseFlipperApp* app, InputEvent* event)
{
    if(app->screen != MorseFlipperScreenPc) return false;

    if(event->key == InputKeyLeft || event->key == InputKeyOk || event->key == InputKeyBack) {
        morse_flipper_key_evt(app, event);
        return true;
    }

    if(event->key == InputKeyUp && event->type == InputTypeShort) {
        morse_flipper_cycle_pc_mode(app, -1);
        return true;
    }

    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        morse_flipper_cycle_pc_mode(app, 1);
        return true;
    }

    if(event->key == InputKeyRight && event->type == InputTypeLong) {
            if(app->pc_mode == MorseFlipperPcModeKeyboard)
                morse_flipper_scene_open(app, MorseFlipperScenePcKeys);
        return true;
    }

    return false;
}

static bool morse_flipper_input_chunk_a(MorseFlipperApp* app, InputEvent* event)
{
    if(morse_flipper_help_input(app, event)) return true;
    if(morse_flipper_about_input(app, event)) return true;
    if(morse_flipper_pc_keys_input(app, event)) return true;
    if(morse_flipper_pc_input(app, event)) return true;
    return false;
}
