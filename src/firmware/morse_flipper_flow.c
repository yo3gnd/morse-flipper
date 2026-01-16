static void morse_flipper_view_dirty(MorseFlipperApp* app)
{
    if(app == NULL || app->live_view == NULL) return;

        with_view_model(app->live_view, MorseFlipperLiveModel * m, { m->bump++; }, true);
}

static void morse_flipper_live_draw(Canvas* canvas, void* model)
{
    MorseFlipperLiveModel* m = model;

    if(m == NULL || m->app == NULL) return;
        morse_flipper_draw(canvas, m->app);
}

static void morse_flipper_scene_open(MorseFlipperApp* app, uint32_t scene)
{
    if(app == NULL || app->scene_manager == NULL) return;
        scene_manager_next_scene(app->scene_manager, scene);
}

static void morse_flipper_scene_back(MorseFlipperApp* app)
{
    if(app == NULL || app->scene_manager == NULL) return;

    if(scene_manager_previous_scene(app->scene_manager)) return;

        scene_manager_stop(app->scene_manager);
    if(app->view_dispatcher) view_dispatcher_stop(app->view_dispatcher);
}


static void morse_flipper_scene_menu_pick(void* ctx, uint32_t idx)
{
    MorseFlipperApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, idx);
}

static uint8_t morse_flipper_scene_screen(uint32_t scene)
{
    switch(scene) {
    case MorseFlipperSceneRun:
        return MorseFlipperScreenRun;
    case MorseFlipperSceneRf:
        return MorseFlipperScreenRf;
    case MorseFlipperSceneSession:
        return MorseFlipperScreenSession;
    case MorseFlipperSceneStraight:
        return MorseFlipperScreenStraight;
    case MorseFlipperSceneBrowse:
        return MorseFlipperScreenBrowse;
    case MorseFlipperSceneSessionEnd:
        return MorseFlipperScreenSessionEnd;
    case MorseFlipperScenePcKeys:
        return MorseFlipperScreenPcKeys;
    case MorseFlipperSceneTrace:
        return MorseFlipperScreenTrace;
    case MorseFlipperSceneHelp:
        return MorseFlipperScreenHelp;
    case MorseFlipperSceneAbout:
        return MorseFlipperScreenAbout;
    default:
        return MorseFlipperScreenMenu;
    }
}

static uint8_t morse_flipper_scene_view(uint32_t scene)
{
    switch(scene) {
    case MorseFlipperSceneHome:
    case MorseFlipperSceneTrainer:
    case MorseFlipperSceneStraightCfg:
    case MorseFlipperScenePc:
    case MorseFlipperSceneGpio:
        return MorseFlipperViewSettings;
    case MorseFlipperSceneHelp:
    case MorseFlipperSceneAbout:
        return MorseFlipperViewWidget;
    case MorseFlipperSceneMenuMain:
    case MorseFlipperSceneMenuTraining:
    case MorseFlipperSceneMenuSettings:
    case MorseFlipperSceneMenuHelp:
        return MorseFlipperViewMenu;
    default:
        return MorseFlipperViewLive;
    }
}

static void morse_flipper_scene_enter_now(MorseFlipperApp* app, uint32_t scene)
{
        morse_flipper_enter_screen(app, morse_flipper_scene_screen(scene), furi_get_tick());
    view_dispatcher_switch_to_view(app->view_dispatcher, morse_flipper_scene_view(scene));
}
