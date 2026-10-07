/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
/*
 * Title scene: menu card over a slowly drifting aerial view of a city,
 * plus New City (terrain preview), Load, Scenarios and About pages.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene_title.h"
#include "game.h"
#include "map_view.h"
#include "platform.h"
#include "render.h"
#include "sim_input.h"
#include "ui.h"
#include "sim.h"

typedef enum {
    PAGE_MAIN = 0,
    PAGE_NEW,
    PAGE_LOAD,
    PAGE_SCENARIOS,
    PAGE_ABOUT,
    PAGE_DEMOS
} title_page_t;

typedef enum {
    MAIN_CONTINUE = 0,
    MAIN_NEW,
    MAIN_LOAD,
    MAIN_SCENARIOS,
    MAIN_SOUND,
    MAIN_ABOUT,
    MAIN_COUNT
} main_item_t;

static title_page_t s_page = PAGE_MAIN;
static int s_sel = 0;
static float s_time = 0.0f;
static bool s_has_autosave = false;
static char s_autosave_label[64];

static int s_seed = 1;
static int s_difficulty = 0;
static bool s_terrain_dirty = true;
static bool s_overview_dirty = true;

static float s_rep_timer[SIM_ACT_COUNT];
static bool s_rep[SIM_ACT_COUNT];
static float s_crank_acc = 0.0f;

/* Backdrop: a demo city drifting under the menu, swapped every so often */
#define BACKDROP_PERIOD 40.0f
#define BACKDROP_FADE 0.6f
static int s_demo = 0;
static float s_backdrop_t = 0.0f;
static bool s_backdrop_is_demo = false;
static int s_bx0, s_by0, s_bx1, s_by1; /* built-up area, px at 8px zoom */

static const struct {
    const char *name;
    const char *year;
    const char *brief;
} s_scenarios[8] = {
    { "Dullsville", "1900", "Nothing has happened here in 30 years. Grow sleepy Dullsville into a thriving city." },
    { "San Francisco", "1906", "A huge earthquake has struck. Rebuild the damage and get the city back on its feet." },
    { "Hamburg", "1944", "Firebombing has left the city in flames. Fight the fires and rebuild." },
    { "Bern", "1965", "The roads are jammed solid. Fix traffic using mass transit and smarter roads." },
    { "Tokyo", "1957", "A monster is attacking the city! Repair the damage and restore order." },
    { "Detroit", "1972", "Crime and decay are out of control. Bring crime down and revive the city." },
    { "Boston", "2010", "A nuclear plant has melted down. Clean up and rebuild the city." },
    { "Rio de Janeiro", "2047", "Rising seas flood the coast. Protect the city and keep it growing." },
};

static const char *s_difficulty_names[3] = { "Easy", "Medium", "Hard" };
static const char *s_difficulty_funds[3] = { "$20,000", "$10,000", "$5,000" };

/* ------------------------------------------------------------------------ */

static bool nav(sim_action_t a) { return s_rep[a]; }

static void input_frame(float dt)
{
    static const sim_action_t dirs[] = { SIM_ACT_UP, SIM_ACT_DOWN, SIM_ACT_LEFT, SIM_ACT_RIGHT };
    for (size_t i = 0; i < 4; i++) {
        sim_action_t a = dirs[i];
        s_rep[a] = false;
        if (sim_action_pressed(a)) {
            s_rep[a] = true;
            s_rep_timer[a] = 0.30f;
        } else if (sim_action_held(a)) {
            s_rep_timer[a] -= dt;
            if (s_rep_timer[a] <= 0.0f) {
                s_rep[a] = true;
                s_rep_timer[a] = 0.08f;
            }
        }
    }
}

static int crank_steps(void)
{
    s_crank_acc += sim_input_get_crank_change();
    int n = 0;
    while (s_crank_acc >= 30.0f) { n++; s_crank_acc -= 30.0f; }
    while (s_crank_acc <= -30.0f) { n--; s_crank_acc += 30.0f; }
    return n;
}

static int move_sel(int sel, int count)
{
    int old = sel;
    if (nav(SIM_ACT_UP)) sel = (sel + count - 1) % count;
    if (nav(SIM_ACT_DOWN)) sel = (sel + 1) % count;
    int c = crank_steps();
    if (c) sel = ((sel + c) % count + count) % count;
    if (sel != old) ui_sound(UI_SND_MOVE);
    return sel;
}

static void refresh_autosave(void)
{
    char name[32];
    int year = 0;
    long pop = 0;
    s_has_autosave = game_autosave_meta(name, sizeof(name), &year, &pop);
    if (s_has_autosave) {
        char p[24];
        ui_fmt_int(p, sizeof(p), pop);
        snprintf(s_autosave_label, sizeof(s_autosave_label), "%s, %d", name, year);
    }
}

static void go(title_page_t page, int sel)
{
    s_page = page;
    s_sel = sel;
}

/* ------------------------------------------------------------------------ */

static void backdrop_measure(void)
{
    int x0, y0, x1, y1;
    if (game_city_bounds(&x0, &y0, &x1, &y1)) {
        s_bx0 = x0 * 8;
        s_by0 = y0 * 8;
        s_bx1 = (x1 + 1) * 8;
        s_by1 = (y1 + 1) * 8;
    } else {
        s_bx0 = s_by0 = 0;
        s_bx1 = CITY_W * 8;
        s_by1 = CITY_H * 8;
    }
}

static void backdrop_load(int demo)
{
    s_demo = game_load_backdrop(demo);
    s_backdrop_is_demo = true;
    s_backdrop_t = 0.0f;
    backdrop_measure();
}

static void scene_title_init(scene_t *self)
{
    (void)self;
    game_ensure_init();
    ui_init();
    sim_input_init();
    platform_menu_clear();
    s_time = 0.0f;
    refresh_autosave();
    go(PAGE_MAIN, s_has_autosave ? MAIN_CONTINUE : MAIN_NEW);

    /* Backdrop: a random demo city on boot; after quitting, your own city
     * stays up until the next swap */
    if (!game_has_city()) {
        backdrop_load((int)(platform_now() * 7.0) % game_demo_count());
    } else {
        s_backdrop_is_demo = false;
        s_backdrop_t = 0.0f;
        backdrop_measure();
    }
}

static void start_game(void)
{
    scene_set(game_get_scene());
}

static void update_main(void)
{
    int old = s_sel;
    s_sel = move_sel(s_sel, MAIN_COUNT);
    /* Skip Continue when there's nothing to continue */
    if (!s_has_autosave && s_sel == MAIN_CONTINUE) s_sel = (old == MAIN_NEW) ? MAIN_ABOUT : MAIN_NEW;

    if (!sim_action_pressed(SIM_ACT_PRIMARY)) return;
    ui_sound(UI_SND_SELECT);
    switch (s_sel) {
    case MAIN_CONTINUE:
        if (game_continue()) start_game();
        else ui_sound(UI_SND_ERROR);
        break;
    case MAIN_NEW:
        s_seed = (int)(platform_now() * 1000.0) & 0x7fff;
        s_terrain_dirty = true;
        go(PAGE_NEW, 2);
        break;
    case MAIN_LOAD: go(PAGE_LOAD, 0); break;
    case MAIN_SCENARIOS: go(PAGE_SCENARIOS, 0); break;
    case MAIN_SOUND: UserSoundOn = !UserSoundOn; break;
    case MAIN_ABOUT: go(PAGE_ABOUT, 0); break;
    }
}

static void update_new(void)
{
    if (s_terrain_dirty) {
        game_generate_terrain(s_seed);
        s_terrain_dirty = false;
        s_overview_dirty = true;
    }
    s_sel = move_sel(s_sel, 3);
    if (s_sel == 1) {
        int d = s_difficulty;
        if (nav(SIM_ACT_LEFT)) d = (d + 2) % 3;
        if (nav(SIM_ACT_RIGHT)) d = (d + 1) % 3;
        if (d != s_difficulty) {
            s_difficulty = d;
            ui_sound(UI_SND_MOVE);
        }
    }
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        ui_sound(UI_SND_SELECT);
        if (s_sel == 0) {
            s_seed = (s_seed * 1103515245 + 12345) & 0x7fff;
            s_terrain_dirty = true;
        } else if (s_sel == 1) {
            s_difficulty = (s_difficulty + 1) % 3;
        } else {
            game_start_new_city(s_difficulty);
            start_game();
        }
    } else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        backdrop_load(s_demo + 1);
        go(PAGE_MAIN, MAIN_NEW);
    }
}

static void update_load(void)
{
    s_sel = move_sel(s_sel, 5);
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        if (s_sel < 4) {
            city_meta_t meta;
            if (city_get_slot_meta(s_sel + 1, &meta) && meta.exists && game_load_slot(s_sel + 1)) {
                ui_sound(UI_SND_SELECT);
                start_game();
            } else {
                ui_sound(UI_SND_ERROR);
            }
        } else {
            ui_sound(UI_SND_SELECT);
            go(PAGE_DEMOS, s_demo);
        }
    } else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        go(PAGE_MAIN, MAIN_LOAD);
    }
}

static void update_scenarios(void)
{
    s_sel = move_sel(s_sel, 8);
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        ui_sound(UI_SND_SELECT);
        game_start_scenario(s_sel + 1);
        start_game();
    } else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        go(PAGE_MAIN, MAIN_SCENARIOS);
    }
}

static void update_demos(void)
{
    s_sel = move_sel(s_sel, game_demo_count());
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        ui_sound(UI_SND_SELECT);
        game_start_demo(s_sel);
        start_game();
    } else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        go(PAGE_LOAD, 4);
    }
}

static void scene_title_update(scene_t *self, float dt)
{
    (void)self;
    sim_input_poll(dt);
    input_frame(dt);
    s_time += dt;

    /* Keep the backdrop city's traffic and smoke moving */
    static float blink = 0.0f;
    blink += dt;
    if (blink > 0.25f) {
        blink = 0.0f;
        flagBlink = !flagBlink;
        animateTiles();
    }

    /* Swap the backdrop city now and then (not while previewing terrain) */
    if (s_page != PAGE_NEW) {
        s_backdrop_t += dt;
        if (s_backdrop_t >= BACKDROP_PERIOD) backdrop_load(s_demo + (s_backdrop_is_demo ? 1 : 0));
    }

    switch (s_page) {
    case PAGE_MAIN: update_main(); break;
    case PAGE_NEW: update_new(); break;
    case PAGE_LOAD: update_load(); break;
    case PAGE_SCENARIOS: update_scenarios(); break;
    case PAGE_DEMOS: update_demos(); break;
    case PAGE_ABOUT:
        if (sim_action_pressed(SIM_ACT_PRIMARY) || sim_action_pressed(SIM_ACT_SECONDARY)) {
            ui_sound(UI_SND_BACK);
            go(PAGE_MAIN, MAIN_ABOUT);
        }
        break;
    }
}

/* ------------------------------------------------------------------------ */

/* Drift range for one axis: keep the view over the built-up area */
static float drift_axis(int lo, int hi, int view, int world, float phase)
{
    float a = (float)lo, b = (float)(hi - view);
    if (b < a) a = b = (lo + hi - view) / 2.0f;
    float v = a + (b - a) * (0.5f + 0.5f * phase);
    if (v < 0) v = 0;
    if (v > world - view) v = (float)(world - view);
    return v;
}

static void draw_backdrop(void)
{
    mapview_sync();
    /* Slow Lissajous drift over the 8px city, starting fresh with each city */
    float t = s_backdrop_t + s_demo * 11.0f;
    /* The menu card covers the left of the screen: aim the open right part
     * (from x = MENU_COVER) at the dense area */
    const int MENU_COVER = 200;
    int vx = (int)drift_axis(s_bx0, s_bx1, SCREEN_W - MENU_COVER, CITY_W * 8 + MENU_COVER, sinf(t * 0.11f));
    int cx = vx - MENU_COVER;
    if (cx < 0) cx = 0;
    if (cx > CITY_W * 8 - SCREEN_W) cx = CITY_W * 8 - SCREEN_W;
    int cy = (int)drift_axis(s_by0, s_by1, SCREEN_H, CITY_H * 8, sinf(t * 0.08f + 1.0f));
    /* Move in 2px steps: the tiles' dithers repeat every 2px, so odd-pixel
     * steps would make every patterned area flicker as the city drifts */
    cx &= ~1;
    cy &= ~1;
    mapview_draw(8, cx, cy, 0, 0, SCREEN_W, SCREEN_H);
    mapview_draw_sprites(8, cx, cy, 0, 0, SCREEN_W, SCREEN_H);

    /* Dissolve to black around a city swap */
    float fade = 0.0f;
    if (s_backdrop_t < BACKDROP_FADE) fade = 1.0f - s_backdrop_t / BACKDROP_FADE;
    else if (s_backdrop_t > BACKDROP_PERIOD - BACKDROP_FADE) fade = (s_backdrop_t - (BACKDROP_PERIOD - BACKDROP_FADE)) / BACKDROP_FADE;
    if (fade > 0.02f) {
        uint8_t a = (uint8_t)(fade >= 1.0f ? 255 : fade * 254.0f);
        render_fill_rect(vec2i(0, 0), vec2i(SCREEN_W, SCREEN_H), rgba(0, 0, 0, a));
    }
}

/* Name chip in the corner so you know which city you're looking at */
static void draw_backdrop_label(void)
{
    char year[16];
    snprintf(year, sizeof(year), "%d", CurrentYear());
    const char *name = CityName ? CityName : "";
    int w = ui_text_width(UI_FONT_BOLD, name) + 8 + ui_text_width(UI_FONT_LIGHT, year) + 16;
    int x = SCREEN_W - 8 - w, y = SCREEN_H - 30;
    ui_panel_dark(x, y, w, 20);
    int tx = x + 8 + ui_text(UI_FONT_BOLD, x + 8, y + 4, name, FONT_ALIGN_LEFT, UI_WHITE) + 8;
    ui_text(UI_FONT_LIGHT, tx, y + 4, year, FONT_ALIGN_LEFT, UI_WHITE);
}

static void draw_list_row(int x, int y, int w, const char *label, const char *value, bool sel, bool dim)
{
    ui_ink_t ink = sel ? UI_WHITE : UI_BLACK;
    if (sel) {
        ui_fill(x, y, w, 18, UI_BLACK);
        ui_icon(ICON_ARROW_R, x + 1, y + 1, true);
    }
    ui_text(UI_FONT_BOLD, x + 16, y + 3, label, FONT_ALIGN_LEFT, ink);
    if (value) ui_text(UI_FONT_LIGHT, x + w - 6, y + 3, value, FONT_ALIGN_RIGHT, ink);
    if (dim) for (int k = x + 14; k < x + w - 6; k += 2) ui_fill(k, y + 9, 1, 1, ink);
}

static void draw_logo(int x, int y)
{
    ui_text(UI_FONT_BIG, x, y, "MICROPOLIS", FONT_ALIGN_LEFT, UI_BLACK);
    ui_text(UI_FONT_LIGHT, x + 1, y + 27, "The classic city simulator", FONT_ALIGN_LEFT, UI_BLACK);
}

static void draw_main(void)
{
    draw_backdrop();
    draw_backdrop_label();
    const int px = 10, py = 10, pw = 182, ph = SCREEN_H - 20;
    ui_panel(px, py, pw, ph);
    draw_logo(px + 12, py + 10);
    ui_hline(px + 8, py + 50, pw - 16, UI_BLACK);

    static const char *labels[MAIN_COUNT] = {
        "Continue", "New City", "Load City", "Scenarios", "Sound", "About"
    };
    /* Six rows (with Continue) must clear the context divider at ph - 48 */
    int y = py + 54;
    int step = s_has_autosave ? 19 : 20;
    int row = 0;
    for (int i = 0; i < MAIN_COUNT; i++) {
        if (i == MAIN_CONTINUE && !s_has_autosave) continue;
        const char *val = NULL;
        if (i == MAIN_SOUND) val = UserSoundOn ? "On" : "Off";
        draw_list_row(px + 6, y + row * step, pw - 12, labels[i], val, i == s_sel, false);
        row++;
    }

    /* Context line for the selected item */
    const char *ctx = "";
    switch (s_sel) {
    case MAIN_CONTINUE: ctx = s_has_autosave ? s_autosave_label : "No city in progress"; break;
    case MAIN_NEW: ctx = "Pick terrain and difficulty"; break;
    case MAIN_LOAD: ctx = "Open a saved city"; break;
    case MAIN_SCENARIOS: ctx = "8 historic challenges"; break;
    case MAIN_SOUND: ctx = "Sound effects"; break;
    case MAIN_ABOUT: ctx = "Credits"; break;
    }
    ui_hline(px + 8, py + ph - 48, pw - 16, UI_BLACK);
    ui_text(UI_FONT_LIGHT, px + 12, py + ph - 42, ctx, FONT_ALIGN_LEFT, UI_BLACK);
    ui_hints(px + pw - 8, py + ph - 21, "Select", NULL, UI_BLACK);
}

static void draw_new(void)
{
    ui_fill(0, 0, SCREEN_W, SCREEN_H, UI_WHITE);
    ui_fill(0, 0, SCREEN_W, 30, UI_BLACK);
    ui_text(UI_FONT_BIG, 10, 2, "New City", FONT_ALIGN_LEFT, UI_WHITE);

    const int mx = 8, my = 35; /* 3px above and below the map frame */
    ui_rect(mx - 2, my - 2, CITY_W * 2 + 4, CITY_H * 2 + 4, UI_BLACK);
    mapview_draw_overview(mx, my, LAYER_CITY, s_overview_dirty);
    s_overview_dirty = false;

    const int rx = mx + CITY_W * 2 + 12, rw = SCREEN_W - rx - 6;
    ui_text(UI_FONT_LIGHT, rx, my, "City name", FONT_ALIGN_LEFT, UI_BLACK);
    ui_text(UI_FONT_BOLD, rx, my + 13, game_random_city_name(s_seed), FONT_ALIGN_LEFT, UI_BLACK);

    char diff[32];
    snprintf(diff, sizeof(diff), "%s", s_difficulty_names[s_difficulty]);
    int y = my + 38;
    draw_list_row(rx - 4, y, rw + 4, "New terrain", NULL, s_sel == 0, false);
    draw_list_row(rx - 4, y + 22, rw + 4, diff, NULL, s_sel == 1, false);
    if (s_sel == 1) ui_icon(ICON_ARROW_L, rx + rw - 30, y + 23, true), ui_icon(ICON_ARROW_R, rx + rw - 14, y + 23, true);
    draw_list_row(rx - 4, y + 44, rw + 4, "Start", NULL, s_sel == 2, false);

    char funds[48];
    snprintf(funds, sizeof(funds), "Start with %s", s_difficulty_funds[s_difficulty]);
    ui_text(UI_FONT_LIGHT, rx, y + 76, funds, FONT_ALIGN_LEFT, UI_BLACK);
    ui_text_wrap(UI_FONT_LIGHT, rx, y + 94, rw, "Dark areas are water, textured areas are forest.", UI_BLACK, 3);

    ui_hints(SCREEN_W - 8, SCREEN_H - 18, "Select", "Back", UI_BLACK);
}

static void draw_load(void)
{
    draw_backdrop();
    const int w = 300, h = 160, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
    ui_panel(x, y, w, h);
    ui_text(UI_FONT_BOLD, x + 12, y + 8, "Load City", FONT_ALIGN_LEFT, UI_BLACK);
    ui_hline(x + 6, y + 24, w - 12, UI_BLACK);
    for (int i = 0; i < 5; i++) {
        char label[64], val[32] = "";
        bool exists = true;
        if (i < 4) {
            city_meta_t meta;
            exists = city_get_slot_meta(i + 1, &meta) && meta.exists;
            if (exists) {
                snprintf(label, sizeof(label), "%.18s", meta.name);
                char pop[24];
                ui_fmt_int(pop, sizeof(pop), meta.population);
                snprintf(val, sizeof(val), "%d  -  %s", meta.year, pop);
            } else {
                snprintf(label, sizeof(label), "Slot %d: empty", i + 1);
            }
        } else {
            snprintf(label, sizeof(label), "Demo Cities");
            snprintf(val, sizeof(val), "%d classics", game_demo_count());
        }
        draw_list_row(x + 6, y + 30 + i * 20, w - 12, label, val[0] ? val : NULL, i == s_sel, !exists);
    }
    ui_hints(x + w - 8, y + h - 20, "Load", "Back", UI_BLACK);
}

static void draw_scenarios(void)
{
    draw_backdrop();
    const int x = 8, y = 8, w = SCREEN_W - 16, h = SCREEN_H - 16;
    ui_panel(x, y, w, h);
    ui_text(UI_FONT_BOLD, x + 12, y + 8, "Scenarios", FONT_ALIGN_LEFT, UI_BLACK);
    ui_hline(x + 6, y + 24, w - 12, UI_BLACK);
    const int lw = 170;
    for (int i = 0; i < 8; i++) {
        draw_list_row(x + 6, y + 30 + i * 20, lw, s_scenarios[i].name, s_scenarios[i].year, i == s_sel, false);
    }
    ui_vline(x + lw + 12, y + 30, 160, UI_BLACK);
    int dx = x + lw + 20, dw = w - lw - 30;
    ui_text(UI_FONT_BIG, dx, y + 28, s_scenarios[s_sel].year, FONT_ALIGN_LEFT, UI_BLACK);
    ui_text(UI_FONT_BOLD, dx, y + 56, s_scenarios[s_sel].name, FONT_ALIGN_LEFT, UI_BLACK);
    ui_text_wrap(UI_FONT_LIGHT, dx, y + 76, dw, s_scenarios[s_sel].brief, UI_BLACK, 6);
    ui_hints(x + w - 8, y + h - 20, "Play", "Back", UI_BLACK);
}

static void draw_demos(void)
{
    draw_backdrop();
    int n = game_demo_count();
    const int w = 240, h = 30 + n * 19 + 28, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
    ui_panel(x, y, w, h);
    ui_text(UI_FONT_BOLD, x + 12, y + 8, "Demo Cities", FONT_ALIGN_LEFT, UI_BLACK);
    ui_hline(x + 6, y + 24, w - 12, UI_BLACK);
    for (int i = 0; i < n; i++) {
        draw_list_row(x + 6, y + 28 + i * 19, w - 12, game_demo_name(i), NULL, i == s_sel, false);
    }
    ui_hints(x + w - 8, y + h - 21, "Play", "Back", UI_BLACK);
}

static void draw_about(void)
{
    draw_backdrop();
    const int w = 372, h = 226, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
    ui_panel(x, y, w, h);
    draw_logo(x + 14, y + 10);
    ui_hline(x + 8, y + 50, w - 16, UI_BLACK);
    int ty = y + 56, tw = w - 28;
    ty += ui_text_wrap(UI_FONT_LIGHT, x + 14, ty, tw,
                       "A modified version of Micropolis, the city simulator Will Wright "
                       "designed at Maxis. Copyright 1989-2007 Electronic Arts, free "
                       "software under the GNU GPL v3. Not the original program, and not "
                       "affiliated with Electronic Arts.", UI_BLACK, 5) + 5;
    ty += ui_text_wrap(UI_FONT_LIGHT, x + 14, ty, tw,
                       "Micropolis is a registered trademark of Micropolis GmbH, licensed "
                       "here as a courtesy of the owner.", UI_BLACK, 2) + 5;
    ui_text_wrap(UI_FONT_LIGHT, x + 14, ty, tw,
                 "Ports: vtcity by tenox7, tiny_micropolis by icedman, micropolis_pd. "
                 "Font: Nontendo by Shaun Inman.", UI_BLACK, 2);
    ui_hints(x + w - 8, y + h - 20, NULL, "Back", UI_BLACK);
}

static void scene_title_draw(scene_t *self)
{
    (void)self;
    switch (s_page) {
    case PAGE_MAIN: draw_main(); break;
    case PAGE_NEW: draw_new(); break;
    case PAGE_LOAD: draw_load(); break;
    case PAGE_SCENARIOS: draw_scenarios(); break;
    case PAGE_ABOUT: draw_about(); break;
    case PAGE_DEMOS: draw_demos(); break;
    }
}

static void scene_title_cleanup(scene_t *self)
{
    (void)self;
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
