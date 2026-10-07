/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#include "game.h"
#include "scene_title.h"
#include "sim_input.h"
#include "render.h"
#include "scene.h"
#include "sound.h"
#include "platform.h"
#include "pd_api.h"

extern void platform_playdate_set_api(PlaydateAPI *api);

static float prev_time = 0.0f;

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
        pd->display->setRefreshRate(30.0f);
        platform_init("micropolis", 400, 240);
        render_init(400, 240);
        sound_init(44100);
        scene_manager_init();

        /* Boot into title scene; the game scene adds its own system menu items */
        scene_set(scene_title_create());
        pd->system->setUpdateCallback(update, NULL);
    } else if (event == kEventTerminate || event == kEventLock || event == kEventLowPower) {
        /* "Continue" picks up exactly where you left off */
        game_autosave();
    }
    return 0;
}
