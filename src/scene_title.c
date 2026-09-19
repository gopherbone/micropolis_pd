#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene_title.h"
#include "game.h"
#include "sim_input.h"
#include "sim_render.h"
#include "render.h"
#include "sound.h"
#include "font.h"
#include "sim.h"

typedef enum {
    TITLE_MENU_MAIN = 0,
    TITLE_MENU_DIFFICULTY,
    TITLE_MENU_LOAD_SLOTS,
    TITLE_MENU_SCENARIOS,
    TITLE_MENU_ABOUT
} title_menu_state_t;

static title_menu_state_t s_menu_state = TITLE_MENU_MAIN;
static int s_selected_index = 0;
static bmfont_t *s_font = NULL;
static float s_backdrop_scroll = 0.0f;

static const char *s_main_options[] = {
    "Start New City",
    "Load Saved City",
    "Play Scenario Campaign",
    "Sound FX: [ON]",
    "About Micropolis"
};
#define MAIN_OPTIONS_COUNT 5

static char s_title_slot_labels[6][48];
static const char *s_title_slot_ptrs[6];
#define LOAD_SLOTS_COUNT 6

static void refresh_title_slots(void)
{
    for (int i = 0; i < 4; i++) {
        city_meta_t meta;
        if (city_get_slot_meta(i + 1, &meta) && meta.exists) {
            snprintf(s_title_slot_labels[i], sizeof(s_title_slot_labels[i]),
                     "Slot %d: %.14s (%d, $%ldk)", i + 1, meta.name, meta.year, (long)(meta.funds / 1000));
        } else {
            snprintf(s_title_slot_labels[i], sizeof(s_title_slot_labels[i]),
                     "Slot %d: [Empty Slot]", i + 1);
        }
        s_title_slot_ptrs[i] = s_title_slot_labels[i];
    }
    snprintf(s_title_slot_labels[4], sizeof(s_title_slot_labels[4]), "Load Example City (About.cty)");
    s_title_slot_ptrs[4] = s_title_slot_labels[4];
    snprintf(s_title_slot_labels[5], sizeof(s_title_slot_labels[5]), "Back");
    s_title_slot_ptrs[5] = s_title_slot_labels[5];
}

static const char *s_diff_options[] = {
    "Easy ($20,000)",
    "Medium ($10,000)",
    "Hard ($5,000)",
    "Back"
};
#define DIFF_OPTIONS_COUNT 4

static const char *s_scenario_options[] = {
    "1. Dullsville (1900 - Stagnation)",
    "2. San Francisco (1906 - Earthquake)",
    "3. Hamburg (1944 - Firestorm)",
    "4. Bern (1965 - Traffic Crisis)",
    "5. Tokyo (1957 - Monster Attack)",
    "6. Detroit (1972 - Crime Wave)",
    "7. Boston (2010 - Nuclear Meltdown)",
    "8. Rio de Janeiro (2047 - Floods)",
    "Back"
};
#define SCENARIO_OPTIONS_COUNT 9

static void scene_title_init(scene_t *self)
{
    (void)self;
    s_menu_state = TITLE_MENU_MAIN;
    s_selected_index = 0;
    s_backdrop_scroll = 0.0f;
    sim_render_init();
    s_font = sim_render_get_font();
    sim_input_init();
}


static void scene_title_update(scene_t *self, float dt)
{
    (void)self;
    sim_input_poll(dt);
    s_backdrop_scroll += dt * 10.0f;

    int count = 0;
    switch (s_menu_state) {
    case TITLE_MENU_MAIN:       count = MAIN_OPTIONS_COUNT; break;
    case TITLE_MENU_DIFFICULTY: count = DIFF_OPTIONS_COUNT; break;
    case TITLE_MENU_LOAD_SLOTS: count = LOAD_SLOTS_COUNT; break;
    case TITLE_MENU_SCENARIOS:  count = SCENARIO_OPTIONS_COUNT; break;
    case TITLE_MENU_ABOUT:      count = 1; break;
    }

    /* D-Pad / Arrow Navigation */
    if (sim_action_pressed(SIM_ACT_UP)) {
        s_selected_index = (s_selected_index + count - 1) % count;
        sound_play_sound("assets/sounds/button.wav", 0.7f);
    }
    if (sim_action_pressed(SIM_ACT_DOWN)) {
        s_selected_index = (s_selected_index + 1) % count;
        sound_play_sound("assets/sounds/button.wav", 0.7f);
    }

    /* Mouse Hover / Click */
    const sim_mouse_t *m = sim_input_get_mouse();
    int menu_w = (s_menu_state == TITLE_MENU_LOAD_SLOTS) ? 300 : 260;
    int menu_x = (400 - menu_w) / 2;
    int menu_y = (s_menu_state == TITLE_MENU_SCENARIOS) ? 55 : ((s_menu_state == TITLE_MENU_LOAD_SLOTS) ? 65 : 95);
    int item_h = 16;

    for (int i = 0; i < count; i++) {
        int ry = menu_y + 20 + i * item_h;
        if (m->pos.x >= menu_x + 8 && m->pos.x <= menu_x + menu_w - 8 &&
            m->pos.y >= ry - 2 && m->pos.y < ry + item_h - 2) {
            if (s_selected_index != i) {
                s_selected_index = i;
            }
            if (m->left_pressed) {
                sim_input_trigger_action(SIM_ACT_PRIMARY);
            }
        }
    }

    /* Back / Secondary */
    if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        if (s_menu_state != TITLE_MENU_MAIN) {
            s_menu_state = TITLE_MENU_MAIN;
            s_selected_index = 0;
            sound_play_sound("assets/sounds/button.wav", 0.7f);
            return;
        }
    }

    /* Selection / Confirm */
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        sound_play_sound("assets/sounds/build.wav", 1.0f);

        if (s_menu_state == TITLE_MENU_MAIN) {
            switch (s_selected_index) {
            case 0: /* Start New City */
                s_menu_state = TITLE_MENU_DIFFICULTY;
                s_selected_index = 0;
                break;
            case 1: /* Load Saved City */
                refresh_title_slots();
                s_menu_state = TITLE_MENU_LOAD_SLOTS;
                s_selected_index = 0;
                break;
            case 2: /* Scenario Campaign */
                s_menu_state = TITLE_MENU_SCENARIOS;
                s_selected_index = 0;
                break;
            case 3: /* Sound FX Toggle */
                UserSoundOn = !UserSoundOn;
                s_main_options[3] = UserSoundOn ? "Sound FX: [ON]" : "Sound FX: [OFF]";
                break;
            case 4: /* About */
                s_menu_state = TITLE_MENU_ABOUT;
                s_selected_index = 0;
                break;
            }
        } else if (s_menu_state == TITLE_MENU_DIFFICULTY) {
            if (s_selected_index < 3) {
                game_start_new_city(s_selected_index);
                scene_set(game_get_scene());
            } else {
                s_menu_state = TITLE_MENU_MAIN;
                s_selected_index = 0;
            }
        } else if (s_menu_state == TITLE_MENU_LOAD_SLOTS) {
            if (s_selected_index < 4) {
                city_meta_t meta;
                if (city_get_slot_meta(s_selected_index + 1, &meta) && meta.exists) {
                    game_load_slot(s_selected_index + 1);
                    scene_set(game_get_scene());
                } else {
                    sound_play_sound("assets/sounds/uh-uh.wav", 1.0f);
                }
            } else if (s_selected_index == 4) {
                game_start_embedded_city("about.cty");
                scene_set(game_get_scene());
            } else {
                s_menu_state = TITLE_MENU_MAIN;
                s_selected_index = 1;
            }
        } else if (s_menu_state == TITLE_MENU_SCENARIOS) {
            if (s_selected_index < 8) {
                game_start_scenario(s_selected_index + 1);
                scene_set(game_get_scene());
            } else {
                s_menu_state = TITLE_MENU_MAIN;
                s_selected_index = 2;
            }
        } else if (s_menu_state == TITLE_MENU_ABOUT) {
            s_menu_state = TITLE_MENU_MAIN;
            s_selected_index = 4;
        }
    }
}

static void scene_title_draw(scene_t *self)
{
    (void)self;

    /* 1. Backdrop */
    render_clear(rgba(12, 16, 28, 255));

    /* Animated night sky horizon & water gradient */
    for (int y = 0; y < 240; y += 4) {
        uint8_t b = (uint8_t)(25 + y * 0.25f);
        render_fill_rect(vec2i(0, y), vec2i(400, 4), rgba(10, 15, b, 255));
    }

    /* Distant building silhouettes */
    render_fill_rect(vec2i(20, 160), vec2i(35, 80), rgba(18, 22, 38, 255));
    render_fill_rect(vec2i(65, 140), vec2i(45, 100), rgba(14, 18, 32, 255));
    render_fill_rect(vec2i(120, 170), vec2i(30, 70), rgba(20, 25, 42, 255));
    render_fill_rect(vec2i(270, 150), vec2i(40, 90), rgba(15, 20, 35, 255));
    render_fill_rect(vec2i(320, 130), vec2i(50, 110), rgba(18, 22, 38, 255));

    /* Stars in sky */
    static const vec2i_t stars[] = {
        {35, 25}, {90, 18}, {150, 32}, {220, 15}, {290, 28}, {360, 22},
        {50, 60}, {180, 55}, {260, 70}, {340, 65}
    };
    for (size_t i = 0; i < sizeof(stars) / sizeof(stars[0]); i++) {
        render_draw_point(stars[i], rgba(200, 220, 255, 200));
    }

    /* 2. Title Banner */
    if (s_menu_state != TITLE_MENU_SCENARIOS) {
        /* Large high-contrast logo banner */
        render_fill_rect(vec2i(50, 16), vec2i(300, 48), rgba_black());
        render_draw_rect(vec2i(50, 16), vec2i(300, 48), rgba_white());
        render_draw_rect(vec2i(52, 18), vec2i(296, 44), rgba_white());

        if (s_font) {
            font_draw_bmfont(s_font, vec2i(200, 24), "MICROPOLIS", FONT_ALIGN_CENTER, rgba(255, 230, 80, 255));
            font_draw_bmfont(s_font, vec2i(200, 44), "SimCity Classic - Tiny Engine Port", FONT_ALIGN_CENTER, rgba(180, 200, 230, 255));
        }
    }

    /* 3. Menu Box */
    int menu_w = (s_menu_state == TITLE_MENU_LOAD_SLOTS) ? 310 : 270;
    int menu_h = (s_menu_state == TITLE_MENU_SCENARIOS) ? 175 : ((s_menu_state == TITLE_MENU_LOAD_SLOTS) ? 140 : 125);
    int menu_x = (400 - menu_w) / 2;
    int menu_y = (s_menu_state == TITLE_MENU_SCENARIOS) ? 35 : ((s_menu_state == TITLE_MENU_LOAD_SLOTS) ? 50 : 82);

    render_fill_rect(vec2i(menu_x, menu_y), vec2i(menu_w, menu_h), rgba_black());
    render_draw_rect(vec2i(menu_x, menu_y), vec2i(menu_w, menu_h), rgba_white());
    render_draw_rect(vec2i(menu_x + 2, menu_y + 2), vec2i(menu_w - 4, menu_h - 4), rgba_white());
    render_draw_line(vec2i(menu_x + 2, menu_y + 20), vec2i(menu_x + menu_w - 3, menu_y + 20), rgba_white());

    /* Header title inside box */
    const char *header = "MAIN MENU";
    if (s_menu_state == TITLE_MENU_DIFFICULTY) header = "SELECT DIFFICULTY";
    else if (s_menu_state == TITLE_MENU_LOAD_SLOTS) header = "LOAD SAVED CITY";
    else if (s_menu_state == TITLE_MENU_SCENARIOS) header = "SCENARIO CAMPAIGN";
    else if (s_menu_state == TITLE_MENU_ABOUT) header = "ABOUT MICROPOLIS";

    if (s_font) {
        font_draw_bmfont(s_font, vec2i(menu_x + menu_w / 2, menu_y + 5), header, FONT_ALIGN_CENTER, rgba(255, 215, 60, 255));
    }

    /* Menu Items */
    if (s_menu_state == TITLE_MENU_ABOUT) {
        if (s_font) {
            font_draw_bmfont(s_font, vec2i(menu_x + 12, menu_y + 28), "Micropolis / SimCity Classic", FONT_ALIGN_LEFT, rgba(255, 240, 120, 255));
            font_draw_bmfont(s_font, vec2i(menu_x + 12, menu_y + 44), "Original Game by Will Wright (Maxis)", FONT_ALIGN_LEFT, rgba_white());
            font_draw_bmfont(s_font, vec2i(menu_x + 12, menu_y + 60), "GPL v3 Release by Don Hopkins / EA", FONT_ALIGN_LEFT, rgba(200, 200, 210, 255));
            font_draw_bmfont(s_font, vec2i(menu_x + 12, menu_y + 76), "Ported to Tiny Engine & Playdate", FONT_ALIGN_LEFT, rgba(160, 220, 180, 255));
            font_draw_bmfont(s_font, vec2i(menu_x + menu_w / 2, menu_y + 100), "[ Press (A) or (B) to return ]", FONT_ALIGN_CENTER, rgba(140, 150, 170, 255));
        }
    } else {
        const char **items = s_main_options;
        int count = MAIN_OPTIONS_COUNT;
        if (s_menu_state == TITLE_MENU_DIFFICULTY) {
            items = s_diff_options;
            count = DIFF_OPTIONS_COUNT;
        } else if (s_menu_state == TITLE_MENU_LOAD_SLOTS) {
            items = s_title_slot_ptrs;
            count = LOAD_SLOTS_COUNT;
        } else if (s_menu_state == TITLE_MENU_SCENARIOS) {
            items = s_scenario_options;
            count = SCENARIO_OPTIONS_COUNT;
        }

        int item_h = (s_menu_state == TITLE_MENU_SCENARIOS) ? 15 : ((s_menu_state == TITLE_MENU_LOAD_SLOTS) ? 16 : 18);
        int start_y = menu_y + 22;

        for (int i = 0; i < count; i++) {
            int ry = start_y + i * item_h;
            bool sel = (s_selected_index == i);

            if (sel) {
                render_draw_rect(vec2i(menu_x + 6, ry - 1), vec2i(menu_w - 12, item_h), rgba_white());
            }

            char buf[64];
            snprintf(buf, sizeof(buf), "%s%s", sel ? "> " : "  ", items[i]);
            if (s_font) {
                font_draw_bmfont(s_font, vec2i(menu_x + 10, ry), buf, FONT_ALIGN_LEFT,
                                 sel ? rgba(255, 235, 80, 255) : rgba(220, 225, 235, 255));
            }
        }
    }

    /* 4. Footer Hint */
    if (s_font) {
        font_draw_bmfont(s_font, vec2i(200, 222), "D-Pad / Mouse: Move | (A): Select | (B): Back",
                         FONT_ALIGN_CENTER, rgba(140, 150, 170, 255));
    }
}

static void scene_title_cleanup(scene_t *self)
{
    (void)self;
    s_font = NULL;
}

static const scene_vtab_t s_scene_title_vtab = {
    .init = scene_title_init,
    .update = scene_title_update,
    .draw = scene_title_draw,
    .cleanup = scene_title_cleanup
};

static scene_t s_scene_title = {
    .vtab = &s_scene_title_vtab,
    .user_data = NULL,
    .is_initialized = false
};

scene_t *scene_title_create(void)
{
    return &s_scene_title;
}

