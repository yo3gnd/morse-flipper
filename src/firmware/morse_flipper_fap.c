#include "morse_flipper_app_i.h"

int32_t morse_flipper_fap(void* p) {
    MorseFlipperApp* app;
    UNUSED(p);

    app = morse_flipper_boot();
    view_dispatcher_run(morse_flipper_view_dispatcher_get(app));
    morse_flipper_shutdown(app);
    return 0;
}
