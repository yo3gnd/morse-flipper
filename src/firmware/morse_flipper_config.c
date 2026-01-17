typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t spare0;
    uint16_t local_dit_ms;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_custom_set_idx;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    uint8_t usb_mouse_invert;
    uint8_t trainer_farn_wpm;
    uint8_t trainer_to_s;
    uint8_t trainer_gap_s;
    uint16_t straight_dit_ms;
    uint8_t sk_to_s;
    uint8_t sk_gap_s;
} MorseFlipperConfig;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t spare0;
    uint16_t local_dit_ms;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_custom_set_idx;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    uint8_t usb_mouse_invert;
    uint8_t trainer_farn_wpm;
    uint8_t trainer_to_s;
    uint8_t trainer_gap_s;
} MorseFlipperConfigV6;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t spare0;
    uint16_t local_dit_ms;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_custom_set_idx;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    uint8_t usb_mouse_invert;
} MorseFlipperConfigV5;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t spare0;
    uint16_t local_dit_ms;
} MorseFlipperConfigV2;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t spare0;
    uint16_t local_dit_ms;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
} MorseFlipperConfigV3;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t handedness;
    uint8_t trainer_lesson;
    uint8_t trainer_group_size;
    uint8_t trainer_session_groups;
    uint8_t spare0;
    uint16_t local_dit_ms;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t trainer_custom_set_idx;
} MorseFlipperConfigV4;

typedef struct {
    uint32_t version;
    uint8_t tone_idx;
    uint8_t keyer_mode;
    uint8_t spare0;
    uint8_t spare1;
} MorseFlipperConfigV1;

static uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app)
{
    uint16_t dit;
    uint8_t wpm;

    if(app == NULL) return 0U;
    dit = app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    wpm = (uint8_t)((1200U + (dit / 2U)) / dit);
    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    return wpm;
}

static void morse_flipper_train_fix(MorseFlipperApp* app)
{
    uint8_t w;

    if(app == NULL) return;

      w = morse_flipper_local_wpm(app);
    if(app->trainer_farn_wpm == 0U) app->trainer_farn_wpm = w;
    if(app->trainer_farn_wpm > w) app->trainer_farn_wpm = w;

    if(app->trainer_to_s == 0U) app->trainer_to_s = MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S;
    if(app->trainer_to_s < MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S) app->trainer_to_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S;
    if(app->trainer_to_s > MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S) app->trainer_to_s = MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S;

    if(app->trainer_gap_s == 0U) app->trainer_gap_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S;
    if(app->trainer_gap_s < MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S) app->trainer_gap_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S;
    if(app->trainer_gap_s > MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S) app->trainer_gap_s = MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S;
}

static uint8_t morse_flipper_cfg_tone_idx(uint8_t x)
{
    if(x == MORSE_FLIPPER_TONE_OFF_IDX) return MORSE_FLIPPER_TONE_OFF_IDX;
    if(x < COUNT_OF(morse_flipper_tones)) return x;
    return 0U;
}

static void morse_flipper_sk_fix(MorseFlipperApp* app)
{
    uint8_t w;

    if(app == NULL) return;

    w = morse_flipper_straight_wpm(app);
    if(w == 0U) {
        app->straight_dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS;
        w = morse_flipper_straight_wpm(app);
    }

    if(app->sk_to_s == 0U)
        app->sk_to_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_DEFAULT_S;
    if(app->sk_to_s < MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S)
        app->sk_to_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S;
    if(app->sk_to_s > MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S)
        app->sk_to_s = MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S;

    if(app->sk_gap_s == 0U)
        app->sk_gap_s = MORSE_FLIPPER_STRAIGHT_NEXT_DEFAULT_S;
    if(app->sk_gap_s < MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S)
        app->sk_gap_s = MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S;
    if(app->sk_gap_s > MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S)
        app->sk_gap_s = MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S;

    UNUSED(w);
}

static void morse_flipper_config_apply_gpio( MorseFlipperApp* app, uint8_t dit, uint8_t dah, uint8_t ground)
{
    if(app == NULL) return;

    if(morse_flipper_gpio_validate(dit, dah, ground) != MorseFlipperGpioRuleOk) {
        return;
    }

    app->gpio_dit_idx = dit;
    app->gpio_dah_idx = dah;
    app->gpio_ground_idx = ground;
    morse_flipper_gpio_sync_straight_idx(app);
}

static void morse_flipper_load_config(MorseFlipperApp* app)
{
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperConfig config;
    MorseFlipperConfigV6 config_v6;
    MorseFlipperConfigV5 config_v5;
    MorseFlipperConfigV4 config_v4;
    MorseFlipperConfigV3 config_v3;
    MorseFlipperConfigV2 config_v2;
    MorseFlipperConfigV1 config_v1;
    uint16_t got = 0U;

    if(storage_file_open(file, MORSE_FLIPPER_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        got = storage_file_read(file, &config, sizeof(config));
        if(got == sizeof(config) && config.version == MORSE_FLIPPER_CONFIG_VERSION) {
            app->tone_idx = morse_flipper_cfg_tone_idx(config.tone_idx);

            if(config.keyer_mode >= MorseKeyerModeStraight &&
               config.keyer_mode <= MorseKeyerModeKeyahead)
                app->keyer_mode = config.keyer_mode;

            if(config.handedness <= MorseFlipperHandednessSwapped) app->handedness = config.handedness;
            if(config.spare0 <= MorseFlipperInputSourceButtons) app->in_src = config.spare0;

            morse_trainer_set_lesson(&app->trainer, config.trainer_lesson);
            morse_trainer_set_group_size(&app->trainer, config.trainer_group_size);
            morse_trainer_set_session_groups(&app->trainer, config.trainer_session_groups);
            if(config.local_dit_ms != 0U) app->trainer.local_dit_ms = config.local_dit_ms;

            morse_flipper_config_apply_gpio( app, config.gpio_dit_idx, config.gpio_dah_idx, config.gpio_ground_idx);

            if(config.trainer_custom_set_idx <= app->custom_sets.count)
                app->trainer.custom_set_idx = config.trainer_custom_set_idx;
            if(config.usb_mode <= MorseFlipperPcModeMidi) app->pc_pref = config.usb_mode;
            if(config.usb_paddle_preset < morse_pc_paddle_preset_count())
                app->pc_paddle_preset = config.usb_paddle_preset;
            if(config.usb_straight_preset < morse_pc_straight_preset_count())
                app->pc_straight_preset = config.usb_straight_preset;

            app->mouse_invert = config.usb_mouse_invert != 0U;
            app->trainer_farn_wpm = config.trainer_farn_wpm;
            app->trainer_to_s = config.trainer_to_s;
            app->trainer_gap_s = config.trainer_gap_s;
            app->straight_dit_ms = config.straight_dit_ms;
            app->sk_to_s = config.sk_to_s;
            app->sk_gap_s = config.sk_gap_s;
        } else if(got == sizeof(config_v6)) {
            memcpy(&config_v6, &config, sizeof(config_v6));
            if(config_v6.version == 6U) {
                app->tone_idx = morse_flipper_cfg_tone_idx(config_v6.tone_idx);

                if(config_v6.keyer_mode >= MorseKeyerModeStraight &&
                   config_v6.keyer_mode <= MorseKeyerModeKeyahead)
                    app->keyer_mode = config_v6.keyer_mode;

                if(config_v6.handedness <= MorseFlipperHandednessSwapped)
                    app->handedness = config_v6.handedness;
                if(config_v6.spare0 <= MorseFlipperInputSourceButtons) app->in_src = config_v6.spare0;

                morse_trainer_set_lesson(&app->trainer, config_v6.trainer_lesson);
                morse_trainer_set_group_size(&app->trainer, config_v6.trainer_group_size);
                morse_trainer_set_session_groups(&app->trainer, config_v6.trainer_session_groups);
                if(config_v6.local_dit_ms != 0U) app->trainer.local_dit_ms = config_v6.local_dit_ms;

                morse_flipper_config_apply_gpio( app, config_v6.gpio_dit_idx, config_v6.gpio_dah_idx, config_v6.gpio_ground_idx);

                if(config_v6.trainer_custom_set_idx <= app->custom_sets.count)
                    app->trainer.custom_set_idx = config_v6.trainer_custom_set_idx;
                if(config_v6.usb_mode <= MorseFlipperPcModeMidi) app->pc_pref = config_v6.usb_mode;
                if(config_v6.usb_paddle_preset < morse_pc_paddle_preset_count())
                    app->pc_paddle_preset = config_v6.usb_paddle_preset;
                if(config_v6.usb_straight_preset < morse_pc_straight_preset_count())
                    app->pc_straight_preset = config_v6.usb_straight_preset;

                app->mouse_invert = config_v6.usb_mouse_invert != 0U;
                app->trainer_farn_wpm = config_v6.trainer_farn_wpm;
                app->trainer_to_s = config_v6.trainer_to_s;
                app->trainer_gap_s = config_v6.trainer_gap_s;
            }
        } else if(got == sizeof(config_v5)) {
            memcpy(&config_v5, &config, sizeof(config_v5));
            if(config_v5.version == 5U) {
                app->tone_idx = morse_flipper_cfg_tone_idx(config_v5.tone_idx);

                if(config_v5.keyer_mode >= MorseKeyerModeStraight &&
                   config_v5.keyer_mode <= MorseKeyerModeKeyahead)
                    app->keyer_mode = config_v5.keyer_mode;

                if(config_v5.handedness <= MorseFlipperHandednessSwapped)
                    app->handedness = config_v5.handedness;
                if(config_v5.spare0 <= MorseFlipperInputSourceButtons) app->in_src = config_v5.spare0;

                morse_trainer_set_lesson(&app->trainer, config_v5.trainer_lesson);
                morse_trainer_set_group_size(&app->trainer, config_v5.trainer_group_size);
                morse_trainer_set_session_groups(&app->trainer, config_v5.trainer_session_groups);
                if(config_v5.local_dit_ms != 0U) app->trainer.local_dit_ms = config_v5.local_dit_ms;

                morse_flipper_config_apply_gpio( app, config_v5.gpio_dit_idx, config_v5.gpio_dah_idx, config_v5.gpio_ground_idx);

                if(config_v5.trainer_custom_set_idx <= app->custom_sets.count)
                    app->trainer.custom_set_idx = config_v5.trainer_custom_set_idx;
                if(config_v5.usb_mode <= MorseFlipperPcModeMidi) app->pc_pref = config_v5.usb_mode;
                if(config_v5.usb_paddle_preset < morse_pc_paddle_preset_count())
                    app->pc_paddle_preset = config_v5.usb_paddle_preset;
                if(config_v5.usb_straight_preset < morse_pc_straight_preset_count())
                    app->pc_straight_preset = config_v5.usb_straight_preset;
                app->mouse_invert = config_v5.usb_mouse_invert != 0U;
            }
        } else if(got == sizeof(config_v4)) {
            memcpy(&config_v4, &config, sizeof(config_v4));
            if(config_v4.version == 4U) {
                app->tone_idx = morse_flipper_cfg_tone_idx(config_v4.tone_idx);

                if(config_v4.keyer_mode >= MorseKeyerModeStraight &&
                   config_v4.keyer_mode <= MorseKeyerModeKeyahead)
                    app->keyer_mode = config_v4.keyer_mode;

                if(config_v4.handedness <= MorseFlipperHandednessSwapped)
                    app->handedness = config_v4.handedness;
                if(config_v4.spare0 <= MorseFlipperInputSourceButtons) app->in_src = config_v4.spare0;

                morse_trainer_set_lesson(&app->trainer, config_v4.trainer_lesson);
                morse_trainer_set_group_size(&app->trainer, config_v4.trainer_group_size);
                morse_trainer_set_session_groups(&app->trainer, config_v4.trainer_session_groups);
                if(config_v4.local_dit_ms != 0U) app->trainer.local_dit_ms = config_v4.local_dit_ms;

                morse_flipper_config_apply_gpio( app, config_v4.gpio_dit_idx, config_v4.gpio_dah_idx, config_v4.gpio_ground_idx);

                if(config_v4.trainer_custom_set_idx <= app->custom_sets.count)
                    app->trainer.custom_set_idx = config_v4.trainer_custom_set_idx;
            }
        } else if(got == sizeof(config_v3)) {
            memcpy(&config_v3, &config, sizeof(config_v3));
            if(config_v3.version == 3U) {
                app->tone_idx = morse_flipper_cfg_tone_idx(config_v3.tone_idx);

                if(config_v3.keyer_mode >= MorseKeyerModeStraight &&
                   config_v3.keyer_mode <= MorseKeyerModeKeyahead)
                    app->keyer_mode = config_v3.keyer_mode;

                if(config_v3.handedness <= MorseFlipperHandednessSwapped)
                    app->handedness = config_v3.handedness;
                if(config_v3.spare0 <= MorseFlipperInputSourceButtons) app->in_src = config_v3.spare0;

                morse_trainer_set_lesson(&app->trainer, config_v3.trainer_lesson);
                morse_trainer_set_group_size(&app->trainer, config_v3.trainer_group_size);
                morse_trainer_set_session_groups(&app->trainer, config_v3.trainer_session_groups);
                if(config_v3.local_dit_ms != 0U) app->trainer.local_dit_ms = config_v3.local_dit_ms;

                morse_flipper_config_apply_gpio( app, config_v3.gpio_dit_idx, config_v3.gpio_dah_idx, config_v3.gpio_ground_idx);
            }
        } else if(got == sizeof(config_v2)) {
            memcpy(&config_v2, &config, sizeof(config_v2));
            if(config_v2.version == 2U) {
                app->tone_idx = morse_flipper_cfg_tone_idx(config_v2.tone_idx);

                if(config_v2.keyer_mode >= MorseKeyerModeStraight &&
                   config_v2.keyer_mode <= MorseKeyerModeKeyahead)
                    app->keyer_mode = config_v2.keyer_mode;

                if(config_v2.handedness <= MorseFlipperHandednessSwapped)
                    app->handedness = config_v2.handedness;
                if(config_v2.spare0 <= MorseFlipperInputSourceButtons) app->in_src = config_v2.spare0;

                morse_trainer_set_lesson(&app->trainer, config_v2.trainer_lesson);
                morse_trainer_set_group_size(&app->trainer, config_v2.trainer_group_size);
                morse_trainer_set_session_groups(&app->trainer, config_v2.trainer_session_groups);
                if(config_v2.local_dit_ms != 0U) app->trainer.local_dit_ms = config_v2.local_dit_ms;
            }
        } else if(got == sizeof(config_v1)) {
            memcpy(&config_v1, &config, sizeof(config_v1));
            if(config_v1.version == 1U) {
                app->tone_idx = morse_flipper_cfg_tone_idx(config_v1.tone_idx);

                if(config_v1.keyer_mode >= MorseKeyerModeStraight &&
                   config_v1.keyer_mode <= MorseKeyerModeKeyahead)
                    app->keyer_mode = config_v1.keyer_mode;
            }
        }
    }

    morse_flipper_train_fix(app);
    morse_flipper_sk_fix(app);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void morse_flipper_save_config(const MorseFlipperApp* app)
{
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperConfig config = {
        .version = MORSE_FLIPPER_CONFIG_VERSION,
        .tone_idx = app->tone_idx,
        .keyer_mode = app->keyer_mode,
        .handedness = app->handedness,
        .trainer_lesson = morse_trainer_lesson(&app->trainer),
        .trainer_group_size = morse_trainer_group_size(&app->trainer),
        .trainer_session_groups = morse_trainer_session_groups(&app->trainer),
        .spare0 = app->in_src,
        .local_dit_ms = app->trainer.local_dit_ms,
        .gpio_straight_idx = morse_flipper_gpio_straight_idx(app),
        .gpio_dit_idx = app->gpio_dit_idx,
        .gpio_dah_idx = app->gpio_dah_idx,
        .gpio_ground_idx = app->gpio_ground_idx,
        .trainer_custom_set_idx = app->trainer.custom_set_idx,
        .usb_mode = app->pc_pref,
        .usb_paddle_preset = app->pc_paddle_preset,
        .usb_straight_preset = app->pc_straight_preset,
        .usb_mouse_invert = app->mouse_invert ? 1U : 0U,
        .trainer_farn_wpm = app->trainer_farn_wpm,
        .trainer_to_s = app->trainer_to_s,
        .trainer_gap_s = app->trainer_gap_s,
        .straight_dit_ms = app->straight_dit_ms,
        .sk_to_s = app->sk_to_s,
        .sk_gap_s = app->sk_gap_s,
    };

    if(storage_file_open(file, MORSE_FLIPPER_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(file, &config, sizeof(config));

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
