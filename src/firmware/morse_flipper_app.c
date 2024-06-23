#include <furi.h>

#include <gui/view_dispatcher.h>

typedef struct MorseFlipperApp MorseFlipperApp;

MorseFlipperApp* morse_flipper_boot(void);
ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app);
void morse_flipper_shutdown(MorseFlipperApp* app);

int32_t morse_flipper_fap(void* p) {
    MorseFlipperApp* app;
    ViewDispatcher* vd;

    UNUSED(p);

    app = morse_flipper_boot();
    if(app == NULL) {
        return -1;
    }

    vd = morse_flipper_view_dispatcher_get(app);
    if(vd != NULL) {
        view_dispatcher_run(vd);
    }

    morse_flipper_shutdown(app);
    return 0;
}
