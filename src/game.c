/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
/*
 * In-game scene: simulation glue, the main map view (cursor, camera, zoom,
 * building), HUD, and dispatch to the modal screens in screens.c.
 *
 * Controls (map view):
 *   D-pad   move cursor (accelerates when held; camera follows smoothly)
 *   A       build with the current tool; hold A + D-pad to drag-build
 *   B       open the build sheet (tools + city menus)
 *   Crank   zoom between near (16px) and far (8px)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "game_internal.h"
#include "platform.h"
#include "render.h"
#include "scene.h"
#include "scene_title.h"
#include "sound.h"
#include "types.h"
#include "ui.h"

#include "sim.h"

extern QUAD CostOf[];

game_state_t G = { .mode = MODE_PLAY, .tool = TOOL_ROAD, .zoom = 16 };

const tool_def_t g_tools[TOOL_COUNT] = {
    [TOOL_DOZER]   = { dozeState,        1, ICON_BULLDOZER,   "Bulldozer",
                       "Clears land, rubble and buildings. Hold A and move to clear a path.", RUBBLE, PREVIEW_ROW },
    [TOOL_ROAD]    = { roadState,        1, ICON_ROAD,        "Road",
                       "Connects zones so people and goods can move. Hold A and move to pave.", 66, PREVIEW_ROW },
    [TOOL_RAIL]    = { rrState,          1, ICON_RAIL,        "Rail",
                       "Mass transit that cuts traffic. Can't carry power.", 226, PREVIEW_ROW },
    [TOOL_WIRE]    = { wireState,        1, ICON_WIRE,        "Power Line",
                       "Carries electricity from plants to zones. Zones also pass power along.", 210, PREVIEW_ROW },
    [TOOL_RES]     = { residentialState, 3, ICON_RESIDENTIAL, "Residential",
                       "Homes for your citizens. Grows when there's demand, power and roads.", RESBASE, PREVIEW_BLOCK },
    [TOOL_COM]     = { commercialState,  3, ICON_COMMERCIAL,  "Commercial",
                       "Shops and offices. Needs customers from homes and a good location.", COMBASE, PREVIEW_BLOCK },
    [TOOL_IND]     = { industrialState,  3, ICON_INDUSTRIAL,  "Industrial",
                       "Factories provide jobs but pollute. Keep them away from homes.", INDBASE, PREVIEW_BLOCK },
    [TOOL_PARK]    = { parkState,        1, ICON_PARK,        "Park",
                       "Trees and fountains raise land value and lift spirits.", 840, PREVIEW_ROW },
    [TOOL_POLICE]  = { policeState,      3, ICON_POLICE,      "Police Station",
                       "Cuts crime in the surrounding area. Needs funding to work.", POLICESTBASE, PREVIEW_BLOCK },
    [TOOL_FIRE]    = { fireState,        3, ICON_FIRE,        "Fire Station",
                       "Puts out fires and protects nearby buildings.", FIRESTBASE, PREVIEW_BLOCK },
    [TOOL_STADIUM] = { stadiumState,     4, ICON_STADIUM,     "Stadium",
                       "Citizens demand one once the city grows. Boosts residential growth.", STADIUMBASE, PREVIEW_BLOCK },
    [TOOL_QUERY]   = { queryState,       1, ICON_QUERY,       "Inspect",
                       "Free. Shows what's on a tile: density, value, crime and pollution.", 0, PREVIEW_ICON },
    [TOOL_COAL]    = { powerState,       4, ICON_COAL,        "Coal Plant",
                       "Cheap power for about 50 zones, but heavy pollution.", COALBASE, PREVIEW_BLOCK },
    [TOOL_NUCLEAR] = { nuclearState,     4, ICON_NUCLEAR,     "Nuclear Plant",
                       "Clean power for about 150 zones. Small risk of meltdown.", NUCLEARBASE, PREVIEW_BLOCK },
    [TOOL_SEAPORT] = { seaportState,     4, ICON_SEAPORT,     "Seaport",
                       "Lets industry ship goods. Build on the coast.", PORTBASE, PREVIEW_BLOCK },
    [TOOL_AIRPORT] = { airportState,     6, ICON_AIRPORT,     "Airport",
                       "Commerce needs one to keep growing in a big city.", AIRPORTBASE, PREVIEW_BLOCK },
};

static bool s_initialized = false;
static bool s_city_loaded = false;
static float s_sim_accumulator = 0.0f;
static float s_blink_accumulator = 0.0f;

/* Cursor movement */
static float s_move_timer = 0.0f;
static float s_move_held = 0.0f;
static int s_last_dx = 0, s_last_dy = 0;

/* Menu-style auto-repeat for D-pad */
static float s_rep_timer[SIM_ACT_COUNT];
static bool s_rep_fire[SIM_ACT_COUNT];

/* Crank */
static float s_crank = 0.0f;
static float s_crank_accum = 0.0f;
static float s_zoom_accum = 0.0f;

/* Feedback */
typedef struct {
    char text[16];
    int tx, ty;     /* tile */
    float t;
} floater_t;
#define MAX_FLOATERS 6
static floater_t s_floaters[MAX_FLOATERS];
static float s_error_flash = 0.0f;

static char s_ticker[128] = "";
static int s_ticker_icon = ICON_ALERT;
static float s_ticker_t = 0.0f;

/* Pending notice from the sim (picture id arrives before its text) */
static int s_pending_notice = 0;
static int s_pending_x = -1, s_pending_y = -1;

/* System menu */
static int s_menu_speed = -1;
static int s_menu_zoom = -1;
static bool s_menu_open_game = false;

static int s_new_seed = 0;

/* ------------------------------------------------------------------------ */
/* Input helpers                                                             */

bool input_nav(sim_action_t act)
{
    return s_rep_fire[act];
}

float input_crank(void)
{
    return s_crank;
}

int input_crank_steps(float detent)
{
    s_crank_accum += s_crank;
    int steps = 0;
    while (s_crank_accum >= detent) {
        steps++;
        s_crank_accum -= detent;
    }
    while (s_crank_accum <= -detent) {
        steps--;
        s_crank_accum += detent;
    }
    return steps;
}

static void input_frame(float dt)
{
    static const sim_action_t dirs[] = { SIM_ACT_UP, SIM_ACT_DOWN, SIM_ACT_LEFT, SIM_ACT_RIGHT };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        sim_action_t a = dirs[i];
        s_rep_fire[a] = false;
        if (sim_action_pressed(a)) {
            s_rep_fire[a] = true;
            s_rep_timer[a] = 0.30f;
        } else if (sim_action_held(a)) {
            s_rep_timer[a] -= dt;
            if (s_rep_timer[a] <= 0.0f) {
                s_rep_fire[a] = true;
                s_rep_timer[a] = 0.075f;
            }
        }
    }
    s_crank = sim_input_get_crank_change();
}

/* ------------------------------------------------------------------------ */
/* Services                                                                  */

int game_tool_cost(int tool)
{
    if (tool < 0 || tool >= TOOL_COUNT) return 0;
    return (int)CostOf[g_tools[tool].state];
}

void game_select_tool(int tool)
{
    if (tool < 0 || tool >= TOOL_COUNT) return;
    G.tool = tool;
    if (sim && sim->editor) setWandState(sim->editor, g_tools[tool].state);
    /* Keep the footprint on the map */
    int size = g_tools[tool].size;
    if (G.cur_x > CITY_W - size) G.cur_x = CITY_W - size;
    if (G.cur_y > CITY_H - size) G.cur_y = CITY_H - size;
}

void game_set_speed(int speed)
{
    if (speed < 0) speed = 0;
    if (speed > 3) speed = 3;
    setSpeed((short)speed);
    SimSpeed = (short)speed;
    platform_menu_set_value(s_menu_speed, speed);
}

static void clamp_camera(float *cx, float *cy)
{
    float max_x = (float)(CITY_W * G.zoom - SCREEN_W);
    float min_y = (float)-TOP_BAR_H;
    float max_y = (float)(CITY_H * G.zoom - SCREEN_H + BOTTOM_BAR_H);
    if (*cx < 0) *cx = 0;
    if (*cx > max_x) *cx = max_x;
    if (*cy < min_y) *cy = min_y;
    if (*cy > max_y) *cy = max_y;
}

void game_set_zoom(int zoom)
{
    if (zoom != 8 && zoom != 16) return;
    if (zoom == G.zoom) return;
    /* Keep the cursor at the same spot on screen */
    float sx = G.cur_x * G.zoom - G.cam_x;
    float sy = G.cur_y * G.zoom - G.cam_y;
    G.zoom = zoom;
    G.cam_x = G.cur_x * zoom - sx;
    G.cam_y = G.cur_y * zoom - sy;
    clamp_camera(&G.cam_x, &G.cam_y);
    platform_menu_set_value(s_menu_zoom, zoom == 16 ? 0 : 1);
    ui_sound(UI_SND_ZOOM);
}

void game_center_on(int tx, int ty)
{
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx >= CITY_W) tx = CITY_W - 1;
    if (ty >= CITY_H) ty = CITY_H - 1;
    int size = g_tools[G.tool].size;
    G.cur_x = tx - size / 2;
    G.cur_y = ty - size / 2;
    if (G.cur_x < 0) G.cur_x = 0;
    if (G.cur_y < 0) G.cur_y = 0;
    if (G.cur_x > CITY_W - size) G.cur_x = CITY_W - size;
    if (G.cur_y > CITY_H - size) G.cur_y = CITY_H - size;
    G.cam_x = tx * G.zoom + G.zoom / 2 - SCREEN_W / 2;
    G.cam_y = ty * G.zoom + G.zoom / 2 - SCREEN_H / 2;
    clamp_camera(&G.cam_x, &G.cam_y);
}

void game_show_message(const char *msg, int icon)
{
    if (!msg || !*msg) return;
    snprintf(s_ticker, sizeof(s_ticker), "%s", msg);
    s_ticker_icon = icon;
    s_ticker_t = 4.5f;
}

void game_quit_to_title(void)
{
    game_autosave();
    scene_set(scene_title_create());
}

static long s_drag_spent = 0;
static int s_drag_floater = -1;

static void add_floater(const char *text, int tx, int ty)
{
    int slot = 0;
    for (int i = 0; i < MAX_FLOATERS; i++) {
        if (s_floaters[i].t <= 0.0f) {
            slot = i;
            break;
        }
        if (s_floaters[i].t < s_floaters[slot].t) slot = i;
    }
    snprintf(s_floaters[slot].text, sizeof(s_floaters[slot].text), "%s", text);
    s_floaters[slot].tx = tx;
    s_floaters[slot].ty = ty;
    s_floaters[slot].t = 0.9f;
    s_drag_floater = slot;
}

/* ------------------------------------------------------------------------ */
/* Sim callbacks                                                             */

static void cb_on_auto_goto(int x, int y)
{
    /* Don't yank the camera; remember where it happened for the notice card */
    s_pending_x = x;
    s_pending_y = y;
}

static void cb_on_show_notice(int id)
{
    s_pending_notice = id;
}

static void cb_on_budget_modal(void)
{
    budget_open();
}

static void cb_on_show_zone_status(char *str, char *s0, char *s1, char *s2, char *s3, char *s4, int x, int y)
{
    bool powered = (x >= 0 && x < CITY_W && y >= 0 && y < CITY_H) ? ((Map[x][y] & PWRBIT) != 0) : false;
    query_set(str, s0, s1, s2, s3, s4, x, y, powered);
}

static void cb_on_did_tool(const char *name, int x, int y)
{
    (void)x;
    (void)y;
    if (!name || !UserSoundOn) return;
    static const struct { const char *name; const char *path; float vol; } sounds[] = {
        { "Road", "assets/sounds/road.wav", 0.8f },
        { "Rail", "assets/sounds/rail.wav", 0.8f },
        { "Wire", "assets/sounds/wire.wav", 0.8f },
        { "Park", "assets/sounds/park.wav", 0.8f },
        { "Res",  "assets/sounds/res.wav", 1.0f },
        { "Com",  "assets/sounds/com.wav", 1.0f },
        { "Ind",  "assets/sounds/ind.wav", 1.0f },
        { "Fire", "assets/sounds/fire.wav", 1.0f },
        { "Pol",  "assets/sounds/police.wav", 1.0f },
        { "Stad", "assets/sounds/stadium.wav", 1.0f },
        { "Coal", "assets/sounds/coal.wav", 1.0f },
        { "Nuc",  "assets/sounds/nuclear.wav", 1.0f },
        { "Seap", "assets/sounds/seaport.wav", 1.0f },
        { "Airp", "assets/sounds/airport.wav", 1.0f },
        { "Dozr", "assets/sounds/bulldozer.wav", 0.7f },
        { "Qry",  "assets/sounds/query.wav", 0.8f },
    };
    for (size_t i = 0; i < sizeof(sounds) / sizeof(sounds[0]); i++) {
        if (strcmp(name, sounds[i].name) == 0) {
            sound_play_sound(sounds[i].path, sounds[i].vol);
            return;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Lifecycle                                                                 */

bool game_has_city(void)
{
    return s_city_loaded;
}

static void reset_view(void)
{
    s_city_loaded = true;
    G.mode = MODE_PLAY;
    G.zoom = 16;
    game_center_on(CITY_W / 2, CITY_H / 2);
    s_ticker_t = 0.0f;
    s_pending_notice = 0;
    HaveLastMessage = 0;
    mapview_invalidate();
}

void game_ensure_init(void)
{
    if (s_initialized) return;

    g_sim_ui_callbacks.on_auto_goto = cb_on_auto_goto;
    g_sim_ui_callbacks.on_show_notice = cb_on_show_notice;
    g_sim_ui_callbacks.on_budget_modal = cb_on_budget_modal;
    g_sim_ui_callbacks.on_show_zone_status = cb_on_show_zone_status;
    g_sim_ui_callbacks.on_did_tool = cb_on_did_tool;

    initMapArrays();
    sim = MakeNewSim();
    if (sim) {
        sim->editor = MakeNewView();
        if (sim->editor) {
            sim->editor->w_width = SCREEN_W;
            sim->editor->w_height = SCREEN_H;
            sim->editor->tool_state = roadState;
        }
    }
    InitWillStuff();
    InitGame();
    StartingYear = 1900;
    autoBulldoze = 1;

    ui_init();
    mapview_init();
    s_initialized = true;
}

static const char *s_city_names[] = {
    "Micropolis", "Port Crank", "New Wright", "Hopkinsville", "Maxis Falls",
    "Bit Harbor", "Pixelton", "Yellow Bay", "Duskwood", "Riverside",
    "Cedar Point", "Ashford", "Glen Haven", "Larkspur", "Oakmont",
};

const char *game_random_city_name(int seed)
{
    int n = (int)(sizeof(s_city_names) / sizeof(s_city_names[0]));
    return s_city_names[(unsigned)seed % (unsigned)n];
}

/* The best of the classic Micropolis example cities embedded in the game */
static const struct {
    const char *file;
    const char *name;
    bool backdrop; /* dense enough on screen to sit behind the title menu */
} s_demos[] = {
    { "haight.cty",   "Haight",           true },
    { "kowloon.cty",  "Kowloon",          false }, /* mostly harbour on screen */
    { "kyoto.cty",    "Kyoto",            true },
    { "kobe.cty",     "Kobe",             true },
    { "yokohama.cty", "Yokohama",         true },
    { "radial.cty",   "Radial",           true },
    { "kamakura.cty", "Kamakura",         true },
    { "joffburg.cty", "Joffburg",         false }, /* mostly river on screen */
    { "ndulls.cty",   "North Dullsville", true },
};
#define DEMO_COUNT ((int)(sizeof(s_demos) / sizeof(s_demos[0])))

int game_demo_count(void)
{
    return DEMO_COUNT;
}

const char *game_demo_name(int i)
{
    return (i >= 0 && i < DEMO_COUNT) ? s_demos[i].name : "";
}

static bool load_demo(int i)
{
    if (i < 0 || i >= DEMO_COUNT) return false;
    if (!LoadEmbeddedCity((char *)s_demos[i].file)) return false;
    setAnyCityName((char *)s_demos[i].name);
    return true;
}

int game_load_backdrop(int demo)
{
    game_ensure_init();
    demo = ((demo % DEMO_COUNT) + DEMO_COUNT) % DEMO_COUNT;
    for (int i = 0; i < DEMO_COUNT && !s_demos[demo].backdrop; i++) demo = (demo + 1) % DEMO_COUNT;
    if (!load_demo(demo)) GenerateNewCity();
    mapview_invalidate();
    s_city_loaded = true;
    return demo;
}

void game_start_demo(int demo)
{
    game_ensure_init();
    if (!load_demo(demo)) GenerateNewCity();
    reset_view();
    game_set_speed(2);
}

bool game_city_bounds(int *x0, int *y0, int *x1, int *y1)
{
    /* Bounding box of the densely built part of the map: 8x8-tile blocks
     * where at least a quarter of the tiles are buildings or zones */
    enum { B = 8 };
    int bx0 = CITY_W, by0 = CITY_H, bx1 = -1, by1 = -1;
    if (!Map[0]) return false;
    for (int by = 0; by < CITY_H; by += B) {
        for (int bx = 0; bx < CITY_W; bx += B) {
            int built = 0;
            for (int y = by; y < by + B && y < CITY_H; y++)
                for (int x = bx; x < bx + B && x < CITY_W; x++)
                    if ((Map[x][y] & LOMASK) >= RESBASE) built++;
            if (built * 4 < B * B) continue;
            if (bx < bx0) bx0 = bx;
            if (by < by0) by0 = by;
            if (bx + B - 1 > bx1) bx1 = bx + B - 1;
            if (by + B - 1 > by1) by1 = by + B - 1;
        }
    }
    if (bx1 < 0) return false;
    *x0 = bx0; *y0 = by0;
    *x1 = bx1 < CITY_W ? bx1 : CITY_W - 1;
    *y1 = by1 < CITY_H ? by1 : CITY_H - 1;
    return true;
}

void game_generate_terrain(int seed)
{
    game_ensure_init();
    s_new_seed = seed;
    GenerateSomeCity(seed);
    mapview_invalidate();
    s_city_loaded = true;
}

void game_start_new_city(int difficulty)
{
    game_ensure_init();
    setAnyCityName((char *)game_random_city_name(s_new_seed)); /* keep spaces */
    StartupGameLevel = (difficulty >= 0 && difficulty <= 2) ? difficulty : 0;
    SetGameLevelFunds(StartupGameLevel);
    CityTime = 0;
    CityTax = 7;
    roadPercent = policePercent = firePercent = 1.0f;
    reset_view();
    game_set_speed(2);
    game_show_message("Zone some land and connect it with roads.", ICON_RESIDENTIAL);
}

void game_start_scenario(int scenario_id)
{
    game_ensure_init();
    LoadScenario((short)scenario_id);
    reset_view();
    game_set_speed(2);
}

void game_start_embedded_city(const char *name)
{
    game_ensure_init();
    if (!LoadEmbeddedCity((char *)name)) GenerateNewCity();
    reset_view();
    game_set_speed(2);
}

int game_save_slot(int slot)
{
    game_ensure_init();
    if (!city_save_slot(slot)) {
        game_show_message("Couldn't save the city.", ICON_ALERT);
        return 0;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Saved %s to slot %d.", CityName ? CityName : "city", slot);
    game_show_message(buf, ICON_GAME);
    return 1;
}

int game_load_slot(int slot)
{
    game_ensure_init();
    if (!city_load_slot(slot)) return 0;
    reset_view();
    game_set_speed(2);
    return 1;
}

#define AUTOSAVE_FILE "autosave.cty"

bool game_has_autosave(void)
{
    city_meta_t meta;
    return city_read_meta(AUTOSAVE_FILE, &meta) && meta.exists;
}

bool game_autosave_meta(char *name, int name_len, int *year, long *pop)
{
    city_meta_t meta;
    if (!city_read_meta(AUTOSAVE_FILE, &meta) || !meta.exists) return false;
    if (name) snprintf(name, (size_t)name_len, "%s", meta.name);
    if (year) *year = meta.year;
    if (pop) *pop = meta.population;
    return true;
}

void game_autosave(void)
{
    if (!s_initialized || scene_get_current() != game_get_scene()) return;
    SaveCityAs(AUTOSAVE_FILE);
}

bool game_continue(void)
{
    game_ensure_init();
    if (!LoadCity(AUTOSAVE_FILE)) return false;
    reset_view();
    game_set_speed(2);
    return true;
}

void game_open_budget(void) { game_ensure_init(); budget_open(); }
void game_open_eval(void) { game_ensure_init(); report_open(); }
void game_open_system_menu(void) { game_ensure_init(); menu_open_game(); }

/* System menu callbacks: just record, handled in update */
static void sysmenu_speed_cb(void *ud)
{
    (void)ud;
    game_set_speed(platform_menu_get_value(s_menu_speed));
}

static void sysmenu_zoom_cb(void *ud)
{
    (void)ud;
    game_set_zoom(platform_menu_get_value(s_menu_zoom) == 0 ? 16 : 8);
}

static void sysmenu_game_cb(void *ud)
{
    (void)ud;
    s_menu_open_game = true;
}

static void game_scene_init(scene_t *self)
{
    (void)self;
    game_ensure_init();
    sim_input_init();
    G.time = 0.0f;

    static const char *speeds[] = { "pause", "slow", "normal", "fast" };
    static const char *zooms[] = { "near", "far" };
    platform_menu_clear();
    s_menu_speed = platform_menu_add_options("speed", speeds, 4, sysmenu_speed_cb, NULL);
    s_menu_zoom = platform_menu_add_options("zoom", zooms, 2, sysmenu_zoom_cb, NULL);
    platform_menu_add_item("city menu", sysmenu_game_cb, NULL);
    platform_menu_set_value(s_menu_speed, SimSpeed);
    platform_menu_set_value(s_menu_zoom, G.zoom == 16 ? 0 : 1);

    /* Nothing loaded yet (shouldn't happen from the title, but be safe) */
    if (!s_city_loaded) {
        if (!load_demo(0)) GenerateNewCity();
        reset_view();
    }
}

static void game_scene_cleanup(scene_t *self)
{
    (void)self;
    platform_menu_clear();
    s_menu_speed = s_menu_zoom = -1;
}

/* ------------------------------------------------------------------------ */
/* Map view: building                                                        */

static void apply_tool(int cx, int cy, bool dragging)
{
    if (!sim || !sim->editor) return;
    const tool_def_t *t = &g_tools[G.tool];
    int off = t->size > 1 ? 1 : 0;
    long before = TotalFunds;
    int res = DoTool(sim->editor, (short)t->state, (short)(cx + off), (short)(cy + off));

    if (res == -1 || res == -2) {
        ClearMes(); /* we report it ourselves */
        if (!dragging) {
            ui_sound(UI_SND_ERROR);
            s_error_flash = 0.4f;
            if (res == -1) game_show_message("Clear the land first (use the Bulldozer).", ICON_BULLDOZER);
            else game_show_message("Not enough money for that.", ICON_BUDGET);
        }
        return;
    }
    long spent = before - TotalFunds;
    if (spent > 0) {
        char buf[16];
        if (dragging && s_drag_floater >= 0 && s_floaters[s_drag_floater].t > 0.0f) {
            /* One running total that follows the cursor while drag-building */
            s_drag_spent += spent;
            floater_t *f = &s_floaters[s_drag_floater];
            ui_fmt_money(f->text, sizeof(f->text), -s_drag_spent);
            f->tx = cx + t->size / 2;
            f->ty = cy;
            f->t = 0.9f;
        } else {
            s_drag_spent = spent;
            ui_fmt_money(buf, sizeof(buf), -spent);
            add_floater(buf, cx + t->size / 2, cy);
        }
    }
}

static void move_cursor(int dx, int dy, bool building)
{
    const tool_def_t *t = &g_tools[G.tool];
    int step = (building && t->size > 1) ? t->size : 1;

    int nx = G.cur_x + dx * step;
    int ny = G.cur_y + dy * step;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > CITY_W - t->size) nx = CITY_W - t->size;
    if (ny > CITY_H - t->size) ny = CITY_H - t->size;
    if (nx == G.cur_x && ny == G.cur_y) return;

    /* Diagonal drag with a network tool: lay the corner so it stays connected */
    if (building && t->size == 1 && nx != G.cur_x && ny != G.cur_y) {
        apply_tool(nx, G.cur_y, true);
    }
    G.cur_x = nx;
    G.cur_y = ny;
    if (building) apply_tool(G.cur_x, G.cur_y, true);
}

static void update_cursor(float dt)
{
    int dx = 0, dy = 0;
    if (sim_action_held(SIM_ACT_LEFT)) dx -= 1;
    if (sim_action_held(SIM_ACT_RIGHT)) dx += 1;
    if (sim_action_held(SIM_ACT_UP)) dy -= 1;
    if (sim_action_held(SIM_ACT_DOWN)) dy += 1;

    bool moved = false;
    if (dx || dy) {
        bool fresh = sim_action_pressed(SIM_ACT_LEFT) || sim_action_pressed(SIM_ACT_RIGHT) ||
                     sim_action_pressed(SIM_ACT_UP) || sim_action_pressed(SIM_ACT_DOWN);
        if (fresh || dx != s_last_dx || dy != s_last_dy) {
            moved = true;
            s_move_held = 0.0f;
            s_move_timer = 0.20f;
        } else {
            s_move_held += dt;
            s_move_timer -= dt;
            if (s_move_timer <= 0.0f) {
                moved = true;
                /* Accelerate the longer the pad is held; faster when zoomed out */
                float rate = s_move_held > 1.0f ? 0.030f : (s_move_held > 0.4f ? 0.055f : 0.085f);
                if (G.zoom == 8) rate *= 0.8f;
                s_move_timer += rate;
                if (s_move_timer < 0.0f) s_move_timer = 0.0f;
            }
        }
    } else {
        s_move_held = 0.0f;
        s_move_timer = 0.0f;
    }
    s_last_dx = dx;
    s_last_dy = dy;

    if (moved) move_cursor(dx, dy, sim_action_held(SIM_ACT_PRIMARY));
}

static void update_camera(float dt)
{
    const tool_def_t *t = &g_tools[G.tool];
    int z = G.zoom;
    float px = (float)(G.cur_x * z), py = (float)(G.cur_y * z);
    float pw = (float)(t->size * z), ph = (float)(t->size * z);
    float margin = (float)(z == 16 ? 48 : 32);

    float tx = G.cam_x, ty = G.cam_y;
    if (px - margin < tx) tx = px - margin;
    if (px + pw + margin > tx + SCREEN_W) tx = px + pw + margin - SCREEN_W;
    if (py - margin < ty + TOP_BAR_H) ty = py - margin - TOP_BAR_H;
    if (py + ph + margin > ty + SCREEN_H - BOTTOM_BAR_H) ty = py + ph + margin - SCREEN_H + BOTTOM_BAR_H;
    clamp_camera(&tx, &ty);

    float k = dt * 14.0f;
    if (k > 1.0f) k = 1.0f;
    G.cam_x += (tx - G.cam_x) * k;
    G.cam_y += (ty - G.cam_y) * k;
    if (abs((int)(tx - G.cam_x)) < 1) G.cam_x = tx;
    if (abs((int)(ty - G.cam_y)) < 1) G.cam_y = ty;
}

static void update_play(float dt)
{
    /* Crank: zoom with a firm detent */
    s_zoom_accum += input_crank();
    if (s_zoom_accum > 50.0f) {
        game_set_zoom(8);
        s_zoom_accum = 0.0f;
    } else if (s_zoom_accum < -50.0f) {
        game_set_zoom(16);
        s_zoom_accum = 0.0f;
    }

    if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        build_open();
        return;
    }

    update_cursor(dt);

    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        apply_tool(G.cur_x, G.cur_y, false);
    }
}

static void tick_sim(float dt)
{
    s_blink_accumulator += dt;
    if (s_blink_accumulator >= 0.25f) {
        s_blink_accumulator = 0.0f;
        flagBlink = !flagBlink;
        animateTiles();
    }

    s_sim_accumulator += dt;
    float interval = SimSpeed == 1 ? 0.30f : (SimSpeed == 3 ? 0.05f : 0.15f);
    if (SimSpeed > 0 && s_sim_accumulator >= interval) {
        s_sim_accumulator = 0.0f;
        SimFrame();
        MoveObjects();
        DoUpdateHeads();
        scoreDoer();
    }
}

static void handle_sim_messages(void)
{
    if (!HaveLastMessage) return;
    HaveLastMessage = 0;
    if (s_pending_notice) {
        notice_open(s_pending_notice, LastMessage, s_pending_x, s_pending_y);
        s_pending_notice = 0;
        s_pending_x = s_pending_y = -1;
    } else {
        game_show_message(LastMessage, ICON_ALERT);
        s_pending_x = s_pending_y = -1;
    }
}

static void game_scene_update(scene_t *self, float dt)
{
    (void)self;
    sim_input_poll(dt);
    input_frame(dt);
    G.time += dt;

    if (s_menu_open_game) {
        s_menu_open_game = false;
        menu_open_game();
    }

    for (int i = 0; i < MAX_FLOATERS; i++)
        if (s_floaters[i].t > 0.0f) s_floaters[i].t -= dt;
    if (s_ticker_t > 0.0f) s_ticker_t -= dt;
    if (s_error_flash > 0.0f) s_error_flash -= dt;

    switch (G.mode) {
    case MODE_PLAY:   update_play(dt); break;
    case MODE_BUILD:  build_update(dt); break;
    case MODE_BUDGET: budget_update(dt); break;
    case MODE_REPORT: report_update(dt); break;
    case MODE_MAP:    citymap_update(dt); break;
    case MODE_MENU:   menu_update(dt); break;
    case MODE_QUERY:  query_update(dt); break;
    case MODE_NOTICE: notice_update(dt); break;
    }

    update_camera(dt);

    /* The city keeps living only while you're looking at it */
    if (G.mode == MODE_PLAY) {
        tick_sim(dt);
        handle_sim_messages();
    }
}

/* ------------------------------------------------------------------------ */
/* Drawing                                                                   */

static const char *s_month_names[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* R/C/I demand: letter + framed well per zone type. The well fills from the
 * bottom; the tick marks zero, so above it is demand and below it surplus.
 * Each type keeps its own pattern (R solid, C checker, I stripes). */
static void draw_rci(int x, int y)
{
    static const char *labels[3] = { "R", "C", "I" };
    int vals[3] = { RValve * 7 / 2000, CValve * 7 / 1500, IValve * 7 / 1500 };
    const int base = y + 15;
    for (int i = 0; i < 3; i++) {
        int lx = x + i * 16;
        int bx = lx + 8;
        int v = vals[i];
        if (v > 7) v = 7;
        if (v < -7) v = -7;
        int level = v + 7; /* 0..14 */
        ui_text(UI_FONT_BOLD, lx, y + 3, labels[i], FONT_ALIGN_LEFT, UI_WHITE);
        ui_rect(bx - 1, y + 1, 8, 15, UI_WHITE);
        for (int r = 0; r < level - 1; r++)
            for (int c = 0; c < 6; c++) {
                bool on = i == 0 || (i == 1 && ((c + r) & 1) == 0) || (i == 2 && (c & 1) == 0);
                if (on) ui_fill(bx + c, base - 1 - r, 1, 1, UI_WHITE);
            }
        ui_fill(bx - 1, y + 8, 8, 1, UI_WHITE);
    }
}

void hud_draw_top(void)
{
    ui_fill(0, 0, SCREEN_W, TOP_BAR_H - 1, UI_BLACK);
    ui_hline(0, TOP_BAR_H - 1, SCREEN_W, UI_BLACK);

    char buf[48];
    ui_fmt_money(buf, sizeof(buf), TotalFunds);
    int x = 6;
    x += ui_text(UI_FONT_BOLD, x, 3, buf, FONT_ALIGN_LEFT, UI_WHITE);

    int month = (int)((CityTime % 48) / 4);
    snprintf(buf, sizeof(buf), "%s %d", s_month_names[month], CurrentYear());
    ui_text(UI_FONT_LIGHT, x + 12, 3, buf, FONT_ALIGN_LEFT, UI_WHITE);

    /* Right side: population, demand, speed */
    int speed_icon = SimSpeed == 0 ? ICON_PAUSE : (SimSpeed == 1 ? ICON_SPEED1 : (SimSpeed == 2 ? ICON_SPEED2 : ICON_SPEED3));
    ui_icon(speed_icon, SCREEN_W - 20, 1, true);

    draw_rci(SCREEN_W - 72, 0);

    long pop = CityPop > 0 ? CityPop : (long)(ResPop + (ComPop + IndPop) * 8) * 20;
    ui_fmt_int(buf, sizeof(buf), pop);
    int pw = ui_text_width(UI_FONT_BOLD, buf);
    int px = SCREEN_W - 80 - pw;
    ui_text(UI_FONT_BOLD, px, 3, buf, FONT_ALIGN_LEFT, UI_WHITE);
    ui_icon(ICON_PEOPLE, px - 18, 1, true);
}

static void draw_tool_chip(void)
{
    const tool_def_t *t = &g_tools[G.tool];
    char name[48], cost[24];
    snprintf(name, sizeof(name), "%s", t->name);
    int c = game_tool_cost(G.tool);
    if (c > 0) ui_fmt_money(cost, sizeof(cost), c);
    else snprintf(cost, sizeof(cost), "Free");

    int w = 26 + ui_text_width(UI_FONT_BOLD, name) + 8 + ui_text_width(UI_FONT_LIGHT, cost) + 10;
    int y = SCREEN_H - BOTTOM_BAR_H + 2;
    ui_panel_dark(3, y, w, BOTTOM_BAR_H - 4);
    ui_icon(t->icon, 7, y + 2, true);
    int x = ui_text(UI_FONT_BOLD, 27, y + 5, name, FONT_ALIGN_LEFT, UI_WHITE) + 27 + 8;
    bool cant_afford = c > 0 && TotalFunds < c;
    ui_text(UI_FONT_LIGHT, x, y + 5, cost, FONT_ALIGN_LEFT, UI_WHITE);
    if (cant_afford) ui_hline(x - 1, y + 10, ui_text_width(UI_FONT_LIGHT, cost) + 2, UI_WHITE);

    /* Right side: control hint */
    int hw = k_glyph_advance[GLYPH_B] + 3 + ui_text_width(UI_FONT_BOLD, "Menu") + 12;
    ui_panel_dark(SCREEN_W - 3 - hw, y, hw, BOTTOM_BAR_H - 4);
    ui_hint(SCREEN_W - hw + 2, y + 4, GLYPH_B, "Menu", UI_WHITE);
}

static void draw_ticker(void)
{
    if (s_ticker_t <= 0.0f || !s_ticker[0]) return;
    int tw = ui_text_width(UI_FONT_BOLD, s_ticker);
    int w = tw + 34;
    if (w > SCREEN_W - 12) w = SCREEN_W - 12;
    int x = (SCREEN_W - w) / 2;
    /* Slide in from the top bar */
    float in = 4.5f - s_ticker_t;
    int y = TOP_BAR_H + 3;
    if (in < 0.15f) y -= (int)((0.15f - in) / 0.15f * 22);
    if (s_ticker_t < 0.15f) y -= (int)((0.15f - s_ticker_t) / 0.15f * 22);
    ui_panel(x, y, w, 20);
    ui_icon(s_ticker_icon, x + 4, y + 2, false);
    render_set_clip(vec2i(x + 22, y), vec2i(w - 26, 20));
    ui_text(UI_FONT_BOLD, x + 23, y + 4, s_ticker, FONT_ALIGN_LEFT, UI_BLACK);
    render_set_clip(vec2i(0, 0), vec2i(0, 0));
}

static void draw_cursor(void)
{
    const tool_def_t *t = &g_tools[G.tool];
    int z = G.zoom;
    int sx = G.cur_x * z - CAM_X();
    int sy = G.cur_y * z - CAM_Y();
    int size = t->size * z;
    int phase = (int)(G.time * 20.0f);
    ui_marching_rect(sx - 1, sy - 1, size + 2, size + 2, phase);

    if (s_error_flash > 0.0f && ((int)(s_error_flash * 20) & 1)) {
        ui_invert(sx + 1, sy + 1, size - 2, size - 2);
    }
}

static void draw_floaters(void)
{
    int z = G.zoom;
    for (int i = 0; i < MAX_FLOATERS; i++) {
        floater_t *f = &s_floaters[i];
        if (f->t <= 0.0f) continue;
        int sx = f->tx * z + z / 2 - CAM_X();
        int sy = f->ty * z - CAM_Y() - 14 - (int)((0.9f - f->t) * 24.0f);
        int w = ui_text_width(UI_FONT_BOLD, f->text) + 6;
        ui_fill(sx - w / 2, sy - 1, w, 13, UI_BLACK);
        ui_text(UI_FONT_BOLD, sx, sy + 1, f->text, FONT_ALIGN_CENTER, UI_WHITE);
    }
}

void game_draw_world(void)
{
    mapview_sync();
    int cx = CAM_X(), cy = CAM_Y();
    ui_fill(0, 0, SCREEN_W, SCREEN_H, UI_BLACK);
    mapview_draw(G.zoom, cx, cy, 0, 0, SCREEN_W, SCREEN_H);
    mapview_draw_sprites(G.zoom, cx, cy, 0, 0, SCREEN_W, SCREEN_H);
}

static void game_scene_draw(scene_t *self)
{
    (void)self;

    /* Full-screen pages draw everything themselves */
    if (G.mode == MODE_BUDGET) { budget_draw(); return; }
    if (G.mode == MODE_REPORT) { report_draw(); return; }
    if (G.mode == MODE_MAP)    { citymap_draw(); return; }

    game_draw_world();

    if (G.mode == MODE_PLAY) {
        draw_cursor();
        draw_floaters();
    }

    hud_draw_top();

    switch (G.mode) {
    case MODE_PLAY:
        draw_tool_chip();
        draw_ticker();
        break;
    case MODE_BUILD:  build_draw(); break;
    case MODE_MENU:   menu_draw(); break;
    case MODE_QUERY:  draw_cursor(); query_draw(); break;
    case MODE_NOTICE: notice_draw(); break;
    default: break;
    }
}

static scene_vtab_t s_vtab = {
    .init = game_scene_init,
    .update = game_scene_update,
    .draw = game_scene_draw,
    .cleanup = game_scene_cleanup,
};

static scene_t s_scene = { .vtab = &s_vtab };

scene_t *game_get_scene(void)
{
    return &s_scene;
}
