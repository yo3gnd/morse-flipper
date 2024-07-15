static const char* morse_flipper_pc_state_name(const MorseFlipperApp* app)
{
    bool up = false;

    if(app->pc_mode == MorseFlipperPcModeMidi)
        return morse_usb_midi_is_connected() ? "usb midi up" : "usb midi wait";

    if(app->pc_mode == MorseFlipperPcModeKeyboard) {
        up = furi_hal_hid_is_connected();
        return up ? "usb hid up" : "usb hid wait";
    }

    if(app->pc_mode == MorseFlipperPcModeMouse) {
        up = furi_hal_hid_is_connected();
        return up ? "usb mouse up" : "usb mouse wait";
    }

    return "usb local off";
}

static uint8_t morse_flipper_nearest_tone_idx_for_midi(uint8_t midi_note)
{
    uint8_t best_idx = 0U;
    uint8_t best_delta = 0xFFU;

    for(uint8_t i = 0; i < COUNT_OF(morse_flipper_tones); i++) {
        uint8_t tone_note = morse_flipper_tones[i].midi_note;
        uint8_t delta = (tone_note > midi_note) ? (tone_note - midi_note) : (midi_note - tone_note);
        if(delta < best_delta) {
            best_delta = delta;
            best_idx = i;
        }
    }

    return best_idx;
}

static bool morse_flipper_transport_connected(const MorseFlipperApp* app)
{
    if(app->pc_mode == MorseFlipperPcModeMidi) return morse_usb_midi_is_connected();

    if(app->pc_mode == MorseFlipperPcModeKeyboard || app->pc_mode == MorseFlipperPcModeMouse)
        return furi_hal_hid_is_connected();

    return false;
}

static uint8_t morse_flipper_keyboard_key_for_note(const MorseFlipperApp* app, uint8_t note)
{
    switch(note) {
    case 0:
        return morse_pc_straight_preset_key(app->pc_straight_preset);
    case 1:
    case 2:
        return morse_pc_paddle_preset_key(app->pc_paddle_preset, note, false);
    default:
        return MorsePcKeyNone;
    }
}

static uint16_t morse_flipper_hid_key_for_pc_key(uint8_t key)
{
    switch(key) {
    case MorsePcKeySpace:
        return HID_KEYBOARD_SPACEBAR;
    case MorsePcKeyX:
        return HID_KEYBOARD_X;
    case MorsePcKeyZ:
        return HID_KEYBOARD_Z;
    case MorsePcKeyC:
        return HID_KEYBOARD_C;
    case MorsePcKeyEnter:
        return HID_KEYBOARD_RETURN;
    case MorsePcKeyNumEnter:
        return HID_KEYPAD_ENTER;
    case MorsePcKeyOpenBracket:
        return HID_KEYBOARD_OPEN_BRACKET;
    case MorsePcKeyCloseBracket:
        return HID_KEYBOARD_CLOSE_BRACKET;
    case MorsePcKeyLeftCtrl:
        return HID_KEYBOARD_L_CTRL;
    case MorsePcKeyRightCtrl:
        return HID_KEYBOARD_R_CTRL;
    case MorsePcKeyLeftShift:
        return HID_KEYBOARD_L_SHIFT;
    case MorsePcKeyRightShift:
        return HID_KEYBOARD_R_SHIFT;
    case MorsePcKeyPeriod:
    case MorsePcKeyGreater:
        return HID_KEYBOARD_DOT;
    case MorsePcKeySlash:
        return HID_KEYBOARD_SLASH;
    case MorsePcKeyComma:
    case MorsePcKeyLess:
        return HID_KEYBOARD_COMMA;
    default:
        return HID_KEYBOARD_NONE;
    }
}

static bool morse_flipper_pc_key_needs_shift(uint8_t key)
{
    return key == MorsePcKeyLess || key == MorsePcKeyGreater;
}

static void morse_flipper_send_midi_note(uint8_t note, bool active)
{
    uint8_t packet[4];

    packet[0] = active ? 0x09U : 0x08U;
    packet[1] = active ? 0x90U : 0x80U;
    packet[2] = note;
    packet[3] = active ? 0x7FU : 0x00U;

    morse_usb_midi_tx(packet, sizeof(packet));
}

static void morse_flipper_send_keyboard_note(MorseFlipperApp* app, uint8_t note, bool active)
{
    uint8_t pc_key = morse_flipper_keyboard_key_for_note(app, note);
    uint16_t key = morse_flipper_hid_key_for_pc_key(pc_key);

    if(key == HID_KEYBOARD_NONE) return;

    if(active) {
        if(morse_flipper_pc_key_needs_shift(pc_key)) furi_hal_hid_kb_press(HID_KEYBOARD_L_SHIFT);
        furi_hal_hid_kb_press(key);
    } else {
        furi_hal_hid_kb_release(key);
        if(morse_flipper_pc_key_needs_shift(pc_key)) furi_hal_hid_kb_release(HID_KEYBOARD_L_SHIFT);
    }
}

static void morse_flipper_release_mouse_buttons(void)
{
    furi_hal_hid_mouse_release(HID_MOUSE_BTN_LEFT);
    furi_hal_hid_mouse_release(HID_MOUSE_BTN_RIGHT);
}

static void morse_flipper_send_mouse_note(MorseFlipperApp* app, uint8_t note, bool active)
{
    uint8_t btn = morse_pc_mouse_button(note, app->mouse_invert);

    if(btn == MorsePcMouseBtnNone) return;

    if(active) {
        furi_hal_hid_mouse_press(btn);
    } else {
        furi_hal_hid_mouse_release(btn);
    }
}

static void morse_flipper_send_transport_note(MorseFlipperApp* app, uint8_t note, bool active)
{
    switch(app->pc_mode) {
    case MorseFlipperPcModeMidi:
        morse_flipper_send_midi_note(note, active);
        break;
    case MorseFlipperPcModeKeyboard:
        morse_flipper_send_keyboard_note(app, note, active);
        break;
    case MorseFlipperPcModeMouse:
        morse_flipper_send_mouse_note(app, note, active);
        break;
    default:
        break;
    }
}
