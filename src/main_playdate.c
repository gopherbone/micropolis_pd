#include "game.h"
#include "scene_title.h"
#include "sim_input.h"
#include "../../src/core/render.h"
#include "../../src/core/scene.h"
#include "../../src/core/sound.h"
#include "../../src/platform/platform.h"
#include "pd_api.h"

extern void platform_playdate_set_api(PlaydateAPI *api);

static float prev_time = 0.0f;

static void pd_menu_budget_cb(void *userdata) {
    (void)userdata;
    if (scene_get_current() != game_get_scene()) {
        scene_set(game_get_scene());
    }
    game_open_budget();
}

static void pd_menu_eval_cb(void *userdata) {
    (void)userdata;
    if (scene_get_current() != game_get_scene()) {
        scene_set(game_get_scene());
    }
    game_open_eval();
}

static void pd_menu_sysmenu_cb(void *userdata) {
    (void)userdata;
    if (scene_get_current() != game_get_scene()) {
        scene_set(game_get_scene());
    }
    game_open_system_menu();
}

static int update(void *userdata) {
    (void)userdata;
    PlaydateAPI *pd = (PlaydateAPI *)platform_playdate_get_api();
    float dt = 0.01667f;

    if (pd) {
        float curr_time = pd->system->getElapsedTime();
        if (prev_time > 0.0f) {
            dt = curr_time - prev_time;
        }
        prev_time = curr_time;
    }

    if (dt <= 0.0001f || dt > 0.05f) {
        dt = 0.01667f;
    }

    platform_poll_events();
    scene_update(dt);
    scene_draw();
    platform_show_fps();
    platform_present();
    return 1;
}

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI *pd, PDSystemEvent event, uint32_t arg) {
    (void)arg;
    if (event == kEventInit) {
        platform_playdate_set_api(pd);
        pd->display->setRefreshRate(60.0f);
        platform_init("micropolis", 400, 240);
        render_init(400, 240);
        sound_init(44100);
        scene_manager_init();

        /* Register Playdate native system menu items */
        pd->system->addMenuItem("System Menu", pd_menu_sysmenu_cb, NULL);
        pd->system->addMenuItem("City Budget", pd_menu_budget_cb, NULL);
        pd->system->addMenuItem("Evaluation", pd_menu_eval_cb, NULL);

        /* Boot into title scene */
        scene_set(scene_title_create());
        pd->system->setUpdateCallback(update, NULL);
    }
    return 0;
}
