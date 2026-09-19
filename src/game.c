#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "platform.h"
#include "render.h"
#include "scene.h"
#include "scene_title.h"
#include "sound.h"
#include "types.h"
#include "sim_input.h"

#include "sim.h"
#include "sim_render.h"

typedef enum {
    MODE_PLAY = 0,
    MODE_QUERY,
    MODE_BUDGET,
    MODE_EVAL,
    MODE_DISASTERS,
    MODE_SCENARIOS,
    MODE_MENU,
    MODE_SAVE_SLOTS,
    MODE_LOAD_SLOTS
} game_mode_t;

static game_mode_t s_game_mode = MODE_PLAY;
static float s_sim_accumulator = 0.0f;
static float s_blink_accumulator = 0.0f;
static int   s_current_tool = roadState;
static bool  s_initialized = false;

/* Tool Definitions */
static const struct {
    int state;
    int size;
    const char *name;
} s_tools[] = {
    { roadState,        1, "Road ($10)" },
    { wireState,        1, "Wire ($5)" },
    { dozeState,        1, "Bulldozer ($1)" },
    { residentialState, 3, "Res Zone ($100)" },
    { commercialState,  3, "Com Zone ($100)" },
    { industrialState,  3, "Ind Zone ($100)" },
    { fireState,        3, "Fire Dept ($500)" },
    { policeState,      3, "Police Dept ($500)" },
    { stadiumState,     4, "Stadium ($5000)" },
    { parkState,        1, "Park ($10)" },
    { seaportState,     4, "Seaport ($3000)" },
    { powerState,       4, "Coal Power ($3000)" },
    { nuclearState,     4, "Nuclear ($5000)" },
    { airportState,     6, "Airport ($10000)" },
    { queryState,       1, "Query" }
};
#define TOOL_COUNT ((int)(sizeof(s_tools) / sizeof(s_tools[0])))
static int s_tool_index = 0;

/* Modals & UI State */
static sim_query_info_t s_query_info;
static sim_budget_modal_t s_budget_modal;
static sim_eval_modal_t s_eval_modal;
static sim_menu_modal_t s_menu_modal;

/* Toast notifications */
static char s_toast_msg[128] = "";
static float s_toast_timer = 0.0f;

/* Navigation & repeat timers */
static float s_key_held_time = 0.0f;
static float s_repeat_timer = 0.0f;
static int s_last_dx = 0, s_last_dy = 0;
static int s_last_drag_tx = -1, s_last_drag_ty = -1;
static float s_palette_timer = 3.0f;

static void show_toast(const char *msg, float duration)
{
    if (!msg) return;
    strncpy(s_toast_msg, msg, sizeof(s_toast_msg) - 1);
    s_toast_msg[sizeof(s_toast_msg) - 1] = '\0';
    s_toast_timer = duration;
}

static void select_tool(int idx)
{
    if (idx < 0) idx = 0;
    if (idx >= TOOL_COUNT) idx = TOOL_COUNT - 1;
    s_tool_index = idx;
    s_current_tool = s_tools[idx].state;
    g_sim_viewport.tool_size = s_tools[idx].size;
    if (sim && sim->editor) {
        setWandState(sim->editor, s_current_tool);
    }
    show_toast(s_tools[idx].name, 2.0f);
    s_palette_timer = 3.0f;
}

static void open_budget_modal(void)
{
    s_budget_modal.tax_rate = CityTax;
    s_budget_modal.road_fund = (int)(roadPercent * 100.0f + 0.5f);
    s_budget_modal.police_fund = (int)(policePercent * 100.0f + 0.5f);
    s_budget_modal.fire_fund = (int)(firePercent * 100.0f + 0.5f);
    s_budget_modal.selected_item = 0;
    s_budget_modal.tax_revenue = (TaxFund > 0) ? TaxFund : (TotalPop * CityTax * 5);
    s_budget_modal.road_cost = (long)(RoadFund * (s_budget_modal.road_fund / 100.0f));
    s_budget_modal.police_cost = (long)(PoliceFund * (s_budget_modal.police_fund / 100.0f));
    s_budget_modal.fire_cost = (long)(FireFund * (s_budget_modal.fire_fund / 100.0f));
    s_budget_modal.previous_funds = TotalFunds;
    s_budget_modal.current_funds = TotalFunds;
    s_game_mode = MODE_BUDGET;
}

static void open_eval_modal(void)
{
    CityEvaluation();
    s_eval_modal.yes_pct = CityYes;
    s_eval_modal.no_pct = CityNo;
    s_eval_modal.score = CityScore;
    s_eval_modal.pop = (int)(TotalPop * 100);
    s_eval_modal.class_id = CityClass;
    s_eval_modal.assessed_val = CityAssValue;
    for (int i = 0; i < 4; i++) {
        s_eval_modal.problems[i] = ProblemOrder[i];
        s_eval_modal.problem_votes[i] = ProblemVotes[ProblemOrder[i]];
    }
    s_game_mode = MODE_EVAL;
}

static const char *s_disaster_names[] = {
    "Fire",
    "Flood",
    "Earthquake",
    "Tornado",
    "Monster Attack",
    "Nuclear Meltdown",
    "Cancel"
};

static void open_disasters_menu(void)
{
    s_menu_modal.title = "TRIGGER DISASTER";
    s_menu_modal.item_count = 7;
    s_menu_modal.selected_index = 0;
    for (int i = 0; i < 7; i++) {
        s_menu_modal.items[i] = s_disaster_names[i];
    }
    s_game_mode = MODE_DISASTERS;
}

static const char *s_scenario_names[] = {
    "1. Dullsville (1900)",
    "2. San Francisco (1906)",
    "3. Hamburg (1944)",
    "4. Bern (1965)",
    "5. Tokyo (1957)",
    "6. Detroit (1972)",
    "7. Boston (2010)",
    "8. Rio de Janeiro (2047)",
    "Cancel"
};

static void open_scenarios_menu(void)
{
    s_menu_modal.title = "SELECT SCENARIO";
    s_menu_modal.item_count = 9;
    s_menu_modal.selected_index = 0;
    for (int i = 0; i < 9; i++) {
        s_menu_modal.items[i] = s_scenario_names[i];
    }
    s_game_mode = MODE_SCENARIOS;
}

static const char *s_system_menu_items[] = {
    "Resume Game",
    "City Budget & Taxes",
    "City Evaluation",
    "Disasters Menu",
    "Select Scenario",
    "Save City to Slot",
    "Load City from Slot",
    "Cycle Map Overlay",
    "Toggle Minimap Radar",
    "Simulation Speed",
    "Quit to Main Menu"
};
#define SYSTEM_MENU_COUNT 11

static char s_save_slot_labels[5][48];
static char s_load_slot_labels[5][48];

static void open_system_menu(void)
{
    s_menu_modal.title = "SYSTEM MENU";
    s_menu_modal.item_count = SYSTEM_MENU_COUNT;
    s_menu_modal.selected_index = 0;
    for (int i = 0; i < SYSTEM_MENU_COUNT; i++) {
        s_menu_modal.items[i] = s_system_menu_items[i];
    }
    s_game_mode = MODE_MENU;
}

static void open_save_slots_menu(void)
{
    s_menu_modal.title = "SAVE CITY TO SLOT";
    s_menu_modal.item_count = 5;
    s_menu_modal.selected_index = 0;
    for (int i = 0; i < 4; i++) {
        city_meta_t meta;
        if (city_get_slot_meta(i + 1, &meta) && meta.exists) {
            snprintf(s_save_slot_labels[i], sizeof(s_save_slot_labels[i]),
                     "Slot %d: %.12s (%d, $%ldk)", i + 1, meta.name, meta.year, (long)(meta.funds / 1000));
        } else {
            snprintf(s_save_slot_labels[i], sizeof(s_save_slot_labels[i]),
                     "Slot %d: [Empty Slot]", i + 1);
        }
        s_menu_modal.items[i] = s_save_slot_labels[i];
    }
    snprintf(s_save_slot_labels[4], sizeof(s_save_slot_labels[4]), "Back");
    s_menu_modal.items[4] = s_save_slot_labels[4];
    s_game_mode = MODE_SAVE_SLOTS;
}

static void open_load_slots_menu(void)
{
    s_menu_modal.title = "LOAD CITY FROM SLOT";
    s_menu_modal.item_count = 5;
    s_menu_modal.selected_index = 0;
    for (int i = 0; i < 4; i++) {
        city_meta_t meta;
        if (city_get_slot_meta(i + 1, &meta) && meta.exists) {
            snprintf(s_load_slot_labels[i], sizeof(s_load_slot_labels[i]),
                     "Slot %d: %.12s (%d, $%ldk)", i + 1, meta.name, meta.year, (long)(meta.funds / 1000));
        } else {
            snprintf(s_load_slot_labels[i], sizeof(s_load_slot_labels[i]),
                     "Slot %d: [Empty Slot]", i + 1);
        }
        s_menu_modal.items[i] = s_load_slot_labels[i];
    }
    snprintf(s_load_slot_labels[4], sizeof(s_load_slot_labels[4]), "Back");
    s_menu_modal.items[4] = s_load_slot_labels[4];
    s_game_mode = MODE_LOAD_SLOTS;
}

static void trigger_disaster(int idx)
{
    switch (idx) {
    case 0:
        SetFire();
        sound_play_sound("assets/sounds/fire.wav", 1.0f);
        sound_play_sound("assets/sounds/siren.wav", 1.0f);
        show_toast("FIRE REPORTED!", 3.0f);
        break;
    case 1:
        MakeFlood();
        sound_play_sound("assets/sounds/siren.wav", 1.0f);
        show_toast("FLOODING REPORTED!", 3.0f);
        break;
    case 2:
        MakeEarthquake();
        sound_play_sound("assets/sounds/rumble.wav", 1.0f);
        show_toast("MAJOR EARTHQUAKE!", 3.0f);
        break;
    case 3:
        MakeTornado();
        sound_play_sound("assets/sounds/siren.wav", 1.0f);
        sound_play_sound("assets/sounds/woosh.wav", 1.0f);
        show_toast("TORNADO SIGHTED!", 3.0f);
        break;
    case 4:
        MakeMonster();
        sound_play_sound("assets/sounds/monster.wav", 1.0f);
        sound_play_sound("assets/sounds/siren.wav", 0.8f);
        show_toast("MONSTER ATTACK!", 3.0f);
        break;
    case 5:
        MakeMeltdown();
        sound_play_sound("assets/sounds/explosion-low.wav", 1.0f);
        sound_play_sound("assets/sounds/siren.wav", 1.0f);
        show_toast("NUCLEAR MELTDOWN!", 3.0f);
        break;
    default: break;
    }
}

static void trigger_scenario(int idx)
{
    if (idx >= 0 && idx < 8) {
        LoadScenario(idx + 1);
        char buf[64];
        snprintf(buf, sizeof(buf), "Loaded %s", CityName ? CityName : "Scenario");
        show_toast(buf, 3.0f);
        /* Center camera */
        g_sim_viewport.cam_x = (SIM_MAP_WIDTH * SIM_TILE_SIZE - g_sim_viewport.view_w) / 2;
        g_sim_viewport.cam_y = (SIM_MAP_HEIGHT * SIM_TILE_SIZE - g_sim_viewport.view_h) / 2;
        g_sim_viewport.cursor_x = g_sim_viewport.cam_x / SIM_TILE_SIZE + 12;
        g_sim_viewport.cursor_y = g_sim_viewport.cam_y / SIM_TILE_SIZE + 7;
    }
}

/* UI Hooks from Sim Engine */
static void cb_on_auto_goto(int x, int y)
{
    g_sim_viewport.cam_x = x * SIM_TILE_SIZE - g_sim_viewport.view_w / 2;
    g_sim_viewport.cam_y = y * SIM_TILE_SIZE - g_sim_viewport.view_h / 2;
    g_sim_viewport.cursor_x = x;
    g_sim_viewport.cursor_y = y;
}

static void cb_on_show_notice(int id)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "Notice #%d", id);
    show_toast(buf, 3.0f);
}

static void cb_on_budget_modal(void)
{
    open_budget_modal();
}

static void cb_on_show_zone_status(char *str, char *s0, char *s1, char *s2, char *s3, char *s4, int x, int y)
{
    s_query_info.active = true;
    strncpy(s_query_info.name, str ? str : "Zone", sizeof(s_query_info.name) - 1);
    s_query_info.name[sizeof(s_query_info.name) - 1] = '\0';
    strncpy(s_query_info.density, s0 ? s0 : "", sizeof(s_query_info.density) - 1);
    s_query_info.density[sizeof(s_query_info.density) - 1] = '\0';
    strncpy(s_query_info.value, s1 ? s1 : "", sizeof(s_query_info.value) - 1);
    s_query_info.value[sizeof(s_query_info.value) - 1] = '\0';
    strncpy(s_query_info.crime, s2 ? s2 : "", sizeof(s_query_info.crime) - 1);
    s_query_info.crime[sizeof(s_query_info.crime) - 1] = '\0';
    strncpy(s_query_info.pollution, s3 ? s3 : "", sizeof(s_query_info.pollution) - 1);
    s_query_info.pollution[sizeof(s_query_info.pollution) - 1] = '\0';
    strncpy(s_query_info.growth, s4 ? s4 : "", sizeof(s_query_info.growth) - 1);
    s_query_info.growth[sizeof(s_query_info.growth) - 1] = '\0';
    s_query_info.tile_x = x;
    s_query_info.tile_y = y;
    s_query_info.powered = (x >= 0 && x < SIM_MAP_WIDTH && y >= 0 && y < SIM_MAP_HEIGHT) ? ((Map[x][y] & PWRBIT) != 0) : false;
    s_game_mode = MODE_QUERY;
}

static void cb_on_did_tool(const char *name, int x, int y)
{
    (void)x; (void)y;
    if (!name) return;

    if (strcmp(name, "Road") == 0)      sound_play_sound("assets/sounds/road.wav", 1.0f);
    else if (strcmp(name, "Wire") == 0) sound_play_sound("assets/sounds/wire.wav", 1.0f);
    else if (strcmp(name, "Park") == 0) sound_play_sound("assets/sounds/park.wav", 1.0f);
    else if (strcmp(name, "Res") == 0)  sound_play_sound("assets/sounds/res.wav", 1.0f);
    else if (strcmp(name, "Com") == 0)  sound_play_sound("assets/sounds/com.wav", 1.0f);
    else if (strcmp(name, "Ind") == 0)  sound_play_sound("assets/sounds/ind.wav", 1.0f);
    else if (strcmp(name, "Fire") == 0) sound_play_sound("assets/sounds/fire.wav", 1.0f);
    else if (strcmp(name, "Pol") == 0)  sound_play_sound("assets/sounds/police.wav", 1.0f);
    else if (strcmp(name, "Stad") == 0) sound_play_sound("assets/sounds/stadium.wav", 1.0f);
    else if (strcmp(name, "Coal") == 0) sound_play_sound("assets/sounds/coal.wav", 1.0f);
    else if (strcmp(name, "Nuc") == 0)  sound_play_sound("assets/sounds/nuclear.wav", 1.0f);
    else if (strcmp(name, "Seap") == 0) sound_play_sound("assets/sounds/seaport.wav", 1.0f);
    else if (strcmp(name, "Airp") == 0) sound_play_sound("assets/sounds/airport.wav", 1.0f);
    else if (strcmp(name, "Dozr") == 0) sound_play_sound("assets/sounds/bulldozer.wav", 0.9f);
    else if (strcmp(name, "Qry") == 0)  sound_play_sound("assets/sounds/query.wav", 1.0f);
    else                                sound_play_sound("assets/sounds/build.wav", 1.0f);
}

static void micropolis_ensure_init(void)
{
    if (s_initialized) return;

    /* 1. Setup Sim UI Callbacks */
    g_sim_ui_callbacks.on_auto_goto = cb_on_auto_goto;
    g_sim_ui_callbacks.on_show_notice = cb_on_show_notice;
    g_sim_ui_callbacks.on_budget_modal = cb_on_budget_modal;
    g_sim_ui_callbacks.on_show_zone_status = cb_on_show_zone_status;
    g_sim_ui_callbacks.on_did_tool = cb_on_did_tool;

    /* 2. Initialize simulation memory and globals */
    initMapArrays();
    sim = MakeNewSim();
    if (sim) {
        sim->editor = MakeNewView();
        if (sim->editor) {
            sim->editor->w_width = 400;
            sim->editor->w_height = 240;
            sim->editor->tool_state = roadState;
        }
    }
    InitWillStuff();
    InitGame();

    /* 3. Initialize graphical renderer */
    sim_render_init();

    /* 4. Default viewport position */
    g_sim_viewport.cam_x = (SIM_MAP_WIDTH * SIM_TILE_SIZE - g_sim_viewport.view_w) / 2;
    g_sim_viewport.cam_y = (SIM_MAP_HEIGHT * SIM_TILE_SIZE - g_sim_viewport.view_h) / 2;
    g_sim_viewport.cursor_x = g_sim_viewport.cam_x / SIM_TILE_SIZE + 12;
    g_sim_viewport.cursor_y = g_sim_viewport.cam_y / SIM_TILE_SIZE + 7;
    g_sim_viewport.tool_size = s_tools[s_tool_index].size;

    SimSpeed = 2;
    s_initialized = true;
}

void game_start_new_city(int difficulty)
{
    micropolis_ensure_init();
    GenerateSomeCity((int)Rand16());
    setCityName("Micropolis");
    StartupGameLevel = (difficulty >= 0 && difficulty <= 2) ? difficulty : 0;
    SetGameLevelFunds(StartupGameLevel);
    CityTime = 0;
    setSpeed(2);
    SimSpeed = 2;

    g_sim_viewport.cam_x = (SIM_MAP_WIDTH * SIM_TILE_SIZE - g_sim_viewport.view_w) / 2;
    g_sim_viewport.cam_y = (SIM_MAP_HEIGHT * SIM_TILE_SIZE - g_sim_viewport.view_h) / 2;
    g_sim_viewport.cursor_x = g_sim_viewport.cam_x / SIM_TILE_SIZE + 12;
    g_sim_viewport.cursor_y = g_sim_viewport.cam_y / SIM_TILE_SIZE + 7;
    s_game_mode = MODE_PLAY;
    show_toast("Welcome Mayor! Build your city.", 3.5f);
}

void game_start_scenario(int scenario_id)
{
    micropolis_ensure_init();
    LoadScenario(scenario_id);

    g_sim_viewport.cam_x = (SIM_MAP_WIDTH * SIM_TILE_SIZE - g_sim_viewport.view_w) / 2;
    g_sim_viewport.cam_y = (SIM_MAP_HEIGHT * SIM_TILE_SIZE - g_sim_viewport.view_h) / 2;
    g_sim_viewport.cursor_x = g_sim_viewport.cam_x / SIM_TILE_SIZE + 12;
    g_sim_viewport.cursor_y = g_sim_viewport.cam_y / SIM_TILE_SIZE + 7;
    s_game_mode = MODE_PLAY;

    char buf[64];
    snprintf(buf, sizeof(buf), "Scenario: %s", CityName ? CityName : "City");
    show_toast(buf, 3.5f);
}

void game_start_embedded_city(const char *name)
{
    micropolis_ensure_init();
    if (!LoadEmbeddedCity((char*)name)) {
        GenerateNewCity();
    }

    g_sim_viewport.cam_x = (SIM_MAP_WIDTH * SIM_TILE_SIZE - g_sim_viewport.view_w) / 2;
    g_sim_viewport.cam_y = (SIM_MAP_HEIGHT * SIM_TILE_SIZE - g_sim_viewport.view_h) / 2;
    g_sim_viewport.cursor_x = g_sim_viewport.cam_x / SIM_TILE_SIZE + 12;
    g_sim_viewport.cursor_y = g_sim_viewport.cam_y / SIM_TILE_SIZE + 7;
    s_game_mode = MODE_PLAY;
    show_toast("Loaded City", 3.0f);
}

int game_save_slot(int slot)
{
    micropolis_ensure_init();
    if (!city_save_slot(slot)) {
        show_toast("Failed to Save City!", 2.5f);
        return 0;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Saved Slot %d: %s", slot, CityName ? CityName : "City");
    show_toast(buf, 2.5f);
    return 1;
}

int game_load_slot(int slot)
{
    micropolis_ensure_init();
    if (!city_load_slot(slot)) {
        show_toast("Failed to Load City!", 2.5f);
        return 0;
    }
    g_sim_viewport.cam_x = (SIM_MAP_WIDTH * SIM_TILE_SIZE - g_sim_viewport.view_w) / 2;
    g_sim_viewport.cam_y = (SIM_MAP_HEIGHT * SIM_TILE_SIZE - g_sim_viewport.view_h) / 2;
    g_sim_viewport.cursor_x = g_sim_viewport.cam_x / SIM_TILE_SIZE + 12;
    g_sim_viewport.cursor_y = g_sim_viewport.cam_y / SIM_TILE_SIZE + 7;
    s_game_mode = MODE_PLAY;
    char buf[64];
    snprintf(buf, sizeof(buf), "Loaded Slot %d: %s", slot, CityName ? CityName : "City");
    show_toast(buf, 3.0f);
    return 1;
}

void game_open_budget(void)
{
    micropolis_ensure_init();
    open_budget_modal();
}

void game_open_eval(void)
{
    micropolis_ensure_init();
    open_eval_modal();
}

void game_open_system_menu(void)
{
    micropolis_ensure_init();
    open_system_menu();
}

static void micropolis_scene_init(scene_t *self)
{
    (void)self;
    micropolis_ensure_init();
    sim_input_init();

    /* If no city loaded yet, default to example city */
    if (TotalFunds == 0 && CityTime == 0) {
        if (!LoadEmbeddedCity("about.cty")) {
            GenerateNewCity();
        }
    }
}

static void update_modal_budget(void)
{
    if (sim_action_pressed(SIM_ACT_UP)) {
        s_budget_modal.selected_item = (s_budget_modal.selected_item + 4) % 5;
    }
    if (sim_action_pressed(SIM_ACT_DOWN)) {
        s_budget_modal.selected_item = (s_budget_modal.selected_item + 1) % 5;
    }

    int change = 0;
    if (sim_action_pressed(SIM_ACT_LEFT)) change -= 1;
    if (sim_action_pressed(SIM_ACT_RIGHT)) change += 1;

    if (change != 0) {
        switch (s_budget_modal.selected_item) {
        case 0: /* Tax */
            s_budget_modal.tax_rate += change;
            if (s_budget_modal.tax_rate < 0) s_budget_modal.tax_rate = 0;
            if (s_budget_modal.tax_rate > 20) s_budget_modal.tax_rate = 20;
            CityTax = s_budget_modal.tax_rate;
            break;
        case 1: /* Road */
            s_budget_modal.road_fund += change * 10;
            if (s_budget_modal.road_fund < 0) s_budget_modal.road_fund = 0;
            if (s_budget_modal.road_fund > 100) s_budget_modal.road_fund = 100;
            roadPercent = s_budget_modal.road_fund / 100.0f;
            break;
        case 2: /* Police */
            s_budget_modal.police_fund += change * 10;
            if (s_budget_modal.police_fund < 0) s_budget_modal.police_fund = 0;
            if (s_budget_modal.police_fund > 100) s_budget_modal.police_fund = 100;
            policePercent = s_budget_modal.police_fund / 100.0f;
            break;
        case 3: /* Fire */
            s_budget_modal.fire_fund += change * 10;
            if (s_budget_modal.fire_fund < 0) s_budget_modal.fire_fund = 0;
            if (s_budget_modal.fire_fund > 100) s_budget_modal.fire_fund = 100;
            firePercent = s_budget_modal.fire_fund / 100.0f;
            break;
        default: break;
        }

        /* Update costs */
        s_budget_modal.tax_revenue = (TotalPop > 0) ? (TotalPop * CityTax * 5) : 0;
        s_budget_modal.road_cost = (long)(RoadFund * (s_budget_modal.road_fund / 100.0f));
        s_budget_modal.police_cost = (long)(PoliceFund * (s_budget_modal.police_fund / 100.0f));
        s_budget_modal.fire_cost = (long)(FireFund * (s_budget_modal.fire_fund / 100.0f));
    }

    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        if (s_budget_modal.selected_item == 4) {
            s_game_mode = MODE_PLAY;
            show_toast("Budget Confirmed", 2.0f);
        }
    }
    if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        s_game_mode = MODE_PLAY;
    }
}

static void update_modal_menu(int menu_type)
{
    /* Mouse Hover / Click for modal items */
    const sim_mouse_t *m = sim_input_get_mouse();
    int item_h = 16;
    int w = 240;
    int h = 40 + s_menu_modal.item_count * item_h;
    int cx = g_sim_viewport.view_w / 2;
    int cy = g_sim_viewport.view_h / 2;
    int pos_x = cx - w / 2;
    int pos_y = cy - h / 2;
    for (int i = 0; i < s_menu_modal.item_count; i++) {
        int ry = pos_y + 26 + i * item_h;
        if (m->pos.x >= pos_x + 6 && m->pos.x <= pos_x + w - 6 &&
            m->pos.y >= ry - 2 && m->pos.y < ry + item_h - 2) {
            if (s_menu_modal.selected_index != i) {
                s_menu_modal.selected_index = i;
            }
            if (m->left_pressed) {
                sim_input_trigger_action(SIM_ACT_PRIMARY);
            }
        }
    }

    if (sim_action_pressed(SIM_ACT_UP)) {
        s_menu_modal.selected_index = (s_menu_modal.selected_index + s_menu_modal.item_count - 1) % s_menu_modal.item_count;
        sound_play_sound("assets/sounds/button.wav", 0.7f);
    }
    if (sim_action_pressed(SIM_ACT_DOWN)) {
        s_menu_modal.selected_index = (s_menu_modal.selected_index + 1) % s_menu_modal.item_count;
        sound_play_sound("assets/sounds/button.wav", 0.7f);
    }
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        int sel = s_menu_modal.selected_index;
        sound_play_sound("assets/sounds/button.wav", 0.9f);

        if (menu_type == 0) { /* Disasters */
            s_game_mode = MODE_PLAY;
            if (sel < 6) trigger_disaster(sel);
        } else if (menu_type == 1) { /* Scenarios */
            s_game_mode = MODE_PLAY;
            if (sel < 8) trigger_scenario(sel);
        } else if (menu_type == 2) { /* System Menu */
            switch (sel) {
            case 0: /* Resume */
                s_game_mode = MODE_PLAY;
                break;
            case 1: /* Budget */
                open_budget_modal();
                break;
            case 2: /* Evaluation */
                open_eval_modal();
                break;
            case 3: /* Disasters */
                open_disasters_menu();
                break;
            case 4: /* Scenarios */
                open_scenarios_menu();
                break;
            case 5: /* Save City to Slot */
                open_save_slots_menu();
                break;
            case 6: /* Load City from Slot */
                open_load_slots_menu();
                break;
            case 7: /* Cycle Overlay */
                g_sim_viewport.overlay = (g_sim_viewport.overlay + 1) % OVERLAY_COUNT;
                s_game_mode = MODE_PLAY;
                show_toast("Overlay Changed", 1.5f);
                break;
            case 8: /* Toggle Minimap */
                g_sim_viewport.show_minimap = !g_sim_viewport.show_minimap;
                s_game_mode = MODE_PLAY;
                show_toast(g_sim_viewport.show_minimap ? "Minimap ON" : "Minimap OFF", 1.5f);
                break;
            case 9: /* Speed */
                SimSpeed = (SimSpeed + 1) % 4;
                s_game_mode = MODE_PLAY;
                {
                    const char *spd_names[] = { "PAUSED", "SLOW", "NORMAL", "FAST" };
                    show_toast(spd_names[SimSpeed], 1.5f);
                }
                break;
            case 10: /* Quit to Main Menu */
                scene_set(scene_title_create());
                break;
            default:
                s_game_mode = MODE_PLAY;
                break;
            }
        } else if (menu_type == 3) { /* Save Slots */
            if (sel < 4) {
                game_save_slot(sel + 1);
                s_game_mode = MODE_PLAY;
            } else {
                open_system_menu();
            }
        } else if (menu_type == 4) { /* Load Slots */
            if (sel < 4) {
                city_meta_t meta;
                if (city_get_slot_meta(sel + 1, &meta) && meta.exists) {
                    game_load_slot(sel + 1);
                    s_game_mode = MODE_PLAY;
                } else {
                    sound_play_sound("assets/sounds/uh-uh.wav", 1.0f);
                    show_toast("Slot is Empty!", 1.5f);
                }
            } else {
                open_system_menu();
            }
        }
    }
    if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        if (menu_type == 3 || menu_type == 4) {
            open_system_menu();
        } else {
            s_game_mode = MODE_PLAY;
        }
    }
}

static void apply_current_tool(int tx, int ty)
{
    if (!sim || !sim->editor) return;
    int place_x = tx;
    int place_y = ty;
    if (g_sim_viewport.tool_size > 1) {
        place_x += 1;
        place_y += 1;
    }
    DoTool(sim->editor, s_current_tool, place_x, place_y);
}

static void micropolis_scene_update(scene_t *self, float dt)
{
    (void)self;

    /* Update sim input manager */
    sim_input_poll(dt);

    /* Update toast timer */
    if (s_toast_timer > 0.0f) {
        s_toast_timer -= dt;
    }
    if (s_palette_timer > 0.0f) {
        s_palette_timer -= dt;
    }

    /* Simulation ticker message check */
    if (HaveLastMessage) {
        show_toast(LastMessage, 3.5f);
        HaveLastMessage = 0;
    }

    /* Handle Modal Dialogs */
    if (s_game_mode == MODE_BUDGET) {
        update_modal_budget();
        return;
    }
    if (s_game_mode == MODE_DISASTERS) {
        update_modal_menu(0);
        return;
    }
    if (s_game_mode == MODE_SCENARIOS) {
        update_modal_menu(1);
        return;
    }
    if (s_game_mode == MODE_MENU) {
        update_modal_menu(2);
        return;
    }
    if (s_game_mode == MODE_SAVE_SLOTS) {
        update_modal_menu(3);
        return;
    }
    if (s_game_mode == MODE_LOAD_SLOTS) {
        update_modal_menu(4);
        return;
    }
    if (s_game_mode == MODE_QUERY || s_game_mode == MODE_EVAL) {
        if (sim_action_pressed(SIM_ACT_PRIMARY) || sim_action_pressed(SIM_ACT_SECONDARY)) {
            s_game_mode = MODE_PLAY;
            s_query_info.active = false;
        }
        return;
    }

    /* ---- MODE_PLAY: Standard Controls & Navigation ---- */

    /* 1. System Menu Trigger */
    if (sim_action_pressed(SIM_ACT_MENU)) {
        open_system_menu();
        return;
    }

    /* 2. Tool Cycling (Crank / Shoulder buttons / PageUp / PageDown / Brackets) */
    if (sim_action_pressed(SIM_ACT_NEXT_TOOL)) {
        select_tool((s_tool_index + 1) % TOOL_COUNT);
    }
    if (sim_action_pressed(SIM_ACT_PREV_TOOL)) {
        select_tool((s_tool_index + TOOL_COUNT - 1) % TOOL_COUNT);
    }

    /* 3. Direct Tool Hotkeys */
    if (sim_action_pressed(SIM_ACT_TOOL_ROAD))   select_tool(0);
    if (sim_action_pressed(SIM_ACT_TOOL_WIRE))   select_tool(1);
    if (sim_action_pressed(SIM_ACT_TOOL_DOZER))  select_tool(2);
    if (sim_action_pressed(SIM_ACT_TOOL_RES))    select_tool(3);
    if (sim_action_pressed(SIM_ACT_TOOL_COM))    select_tool(4);
    if (sim_action_pressed(SIM_ACT_TOOL_IND))    select_tool(5);
    if (sim_action_pressed(SIM_ACT_TOOL_FIRE))   select_tool(6);
    if (sim_action_pressed(SIM_ACT_TOOL_POLICE)) select_tool(7);
    if (sim_action_pressed(SIM_ACT_TOOL_STAD))   select_tool(8);
    if (sim_action_pressed(SIM_ACT_TOOL_PARK))   select_tool(9);
    if (sim_action_pressed(SIM_ACT_TOOL_PORT))   select_tool(10);
    if (sim_action_pressed(SIM_ACT_TOOL_COAL))   select_tool(11);
    if (sim_action_pressed(SIM_ACT_TOOL_NUKE))   select_tool(12);
    if (sim_action_pressed(SIM_ACT_TOOL_AIR))    select_tool(13);
    if (sim_action_pressed(SIM_ACT_TOOL_QUERY))  select_tool(14);

    /* 4. Modal Menu Hotkeys */
    if (sim_action_pressed(SIM_ACT_BUDGET))    open_budget_modal();
    if (sim_action_pressed(SIM_ACT_EVAL))      open_eval_modal();
    if (sim_action_pressed(SIM_ACT_DISASTERS)) open_disasters_menu();
    if (sim_action_pressed(SIM_ACT_SCENARIOS)) open_scenarios_menu();

    /* 5. Speed Controls */
    if (sim_action_pressed(SIM_ACT_SPEED_PAUSE)) { SimSpeed = 0; setSpeed(0); show_toast("PAUSED", 1.5f); }
    if (sim_action_pressed(SIM_ACT_SPEED_SLOW))  { SimSpeed = 1; setSpeed(1); show_toast("SLOW", 1.5f); }
    if (sim_action_pressed(SIM_ACT_SPEED_NORM))  { SimSpeed = 2; setSpeed(2); show_toast("NORMAL", 1.5f); }
    if (sim_action_pressed(SIM_ACT_SPEED_FAST))  { SimSpeed = 3; setSpeed(3); show_toast("FAST", 1.5f); }
    if (sim_action_pressed(SIM_ACT_SPEED)) {
        SimSpeed = (SimSpeed + 1) % 4;
        setSpeed(SimSpeed);
        const char *spd_names[] = { "PAUSED", "SLOW", "NORMAL", "FAST" };
        show_toast(spd_names[SimSpeed], 1.5f);
    }

    /* 6. Toggles: Minimap & Overlay */
    if (sim_action_pressed(SIM_ACT_MINIMAP)) {
        g_sim_viewport.show_minimap = !g_sim_viewport.show_minimap;
    }
    if (sim_action_pressed(SIM_ACT_OVERLAY)) {
        g_sim_viewport.overlay = (g_sim_viewport.overlay + 1) % OVERLAY_COUNT;
    }

    /* 7. D-Pad Cursor Navigation with Hold Acceleration */
    int dx = 0, dy = 0;
    if (sim_action_held(SIM_ACT_LEFT))  dx -= 1;
    if (sim_action_held(SIM_ACT_RIGHT)) dx += 1;
    if (sim_action_held(SIM_ACT_UP))    dy -= 1;
    if (sim_action_held(SIM_ACT_DOWN))  dy += 1;

    bool moved = false;
    if (dx != 0 || dy != 0) {
        if (sim_action_pressed(SIM_ACT_LEFT) || sim_action_pressed(SIM_ACT_RIGHT) ||
            sim_action_pressed(SIM_ACT_UP) || sim_action_pressed(SIM_ACT_DOWN) ||
            dx != s_last_dx || dy != s_last_dy) {
            moved = true;
            s_key_held_time = 0.0f;
            s_repeat_timer = 0.22f;
        } else {
            s_key_held_time += dt;
            s_repeat_timer -= dt;
            if (s_repeat_timer <= 0.0f) {
                moved = true;
                s_repeat_timer = (s_key_held_time > 1.2f) ? 0.04f : 0.09f;
            }
        }
    } else {
        s_key_held_time = 0.0f;
        s_repeat_timer = 0.0f;
    }
    s_last_dx = dx;
    s_last_dy = dy;

    if (moved) {
        g_sim_viewport.cursor_x += dx;
        g_sim_viewport.cursor_y += dy;

        /* Clamp cursor */
        int max_tx = SIM_MAP_WIDTH - g_sim_viewport.tool_size;
        int max_ty = SIM_MAP_HEIGHT - g_sim_viewport.tool_size;
        if (g_sim_viewport.cursor_x < 0) g_sim_viewport.cursor_x = 0;
        if (g_sim_viewport.cursor_y < 0) g_sim_viewport.cursor_y = 0;
        if (g_sim_viewport.cursor_x > max_tx) g_sim_viewport.cursor_x = max_tx;
        if (g_sim_viewport.cursor_y > max_ty) g_sim_viewport.cursor_y = max_ty;

        /* Smooth camera edge following */
        int cur_px = g_sim_viewport.cursor_x * SIM_TILE_SIZE;
        int cur_py = g_sim_viewport.cursor_y * SIM_TILE_SIZE;
        int margin = 32;

        if (cur_px < g_sim_viewport.cam_x + margin) {
            g_sim_viewport.cam_x = cur_px - margin;
        }
        if (cur_px + g_sim_viewport.tool_size * SIM_TILE_SIZE > g_sim_viewport.cam_x + g_sim_viewport.view_w - margin) {
            g_sim_viewport.cam_x = cur_px + g_sim_viewport.tool_size * SIM_TILE_SIZE - g_sim_viewport.view_w + margin;
        }
        if (cur_py < g_sim_viewport.cam_y + margin) {
            g_sim_viewport.cam_y = cur_py - margin;
        }
        if (cur_py + g_sim_viewport.tool_size * SIM_TILE_SIZE > g_sim_viewport.cam_y + g_sim_viewport.view_h - margin) {
            g_sim_viewport.cam_y = cur_py + g_sim_viewport.tool_size * SIM_TILE_SIZE - g_sim_viewport.view_h + margin;
        }

        /* Continuous drag placement with A button */
        if (sim_action_held(SIM_ACT_PRIMARY)) {
            apply_current_tool(g_sim_viewport.cursor_x, g_sim_viewport.cursor_y);
        }
    }

    /* 8. Mouse Support & Click Targets */
    const sim_mouse_t *m = sim_input_get_mouse();
    if (m) {
        /* Right Click: inspect tile under mouse */
        if (m->right_pressed) {
            int tx = (g_sim_viewport.cam_x + m->pos.x) / SIM_TILE_SIZE;
            int ty = (g_sim_viewport.cam_y + m->pos.y) / SIM_TILE_SIZE;
            if (tx >= 0 && tx < SIM_MAP_WIDTH && ty >= 0 && ty < SIM_MAP_HEIGHT) {
                if (sim && sim->editor) {
                    DoTool(sim->editor, queryState, tx, ty);
                }
            }
        }

        /* Left Click on Interactive UI Targets */
        if (m->left_pressed) {
            int tray_tool = -1;
            vec2i_t target_tile = {0, 0};

            /* Tool Tray Click */
            if (sim_render_is_point_in_tool_tray(m->pos, &g_sim_viewport, &tray_tool)) {
                select_tool(tray_tool);
            }
            /* Minimap Click */
            else if (sim_render_is_point_in_minimap(m->pos, &g_sim_viewport, &target_tile)) {
                g_sim_viewport.cam_x = target_tile.x * SIM_TILE_SIZE - g_sim_viewport.view_w / 2;
                g_sim_viewport.cam_y = target_tile.y * SIM_TILE_SIZE - g_sim_viewport.view_h / 2;
                g_sim_viewport.cursor_x = target_tile.x;
                g_sim_viewport.cursor_y = target_tile.y;
            }
            /* Status Bar Click */
            else if (m->pos.y < 16) {
                if (m->pos.x >= g_sim_viewport.view_w - 70) {
                    g_sim_viewport.overlay = (g_sim_viewport.overlay + 1) % OVERLAY_COUNT;
                } else if (m->pos.x >= 140 && m->pos.x <= 230) {
                    open_budget_modal();
                } else if (m->pos.x >= 280 && m->pos.x <= 340) {
                    SimSpeed = (SimSpeed + 1) % 4;
                    setSpeed(SimSpeed);
                }
            }
        }

        /* Mouse Move & Continuous Drag Building on Map Canvas */
        if (m->pos.y >= 16 && m->pos.y < 220 && m->pos.x >= 0 && m->pos.x < g_sim_viewport.view_w) {
            int m_tx = (g_sim_viewport.cam_x + m->pos.x) / SIM_TILE_SIZE;
            int m_ty = (g_sim_viewport.cam_y + m->pos.y) / SIM_TILE_SIZE;

            if (m_tx >= 0 && m_tx < SIM_MAP_WIDTH && m_ty >= 0 && m_ty < SIM_MAP_HEIGHT) {
                g_sim_viewport.cursor_x = m_tx;
                g_sim_viewport.cursor_y = m_ty;

                if (m->left_down) {
                    if (m_tx != s_last_drag_tx || m_ty != s_last_drag_ty) {
                        apply_current_tool(m_tx, m_ty);
                        s_last_drag_tx = m_tx;
                        s_last_drag_ty = m_ty;
                    }
                } else {
                    s_last_drag_tx = -1;
                    s_last_drag_ty = -1;
                }
            }
        }
    }

    /* 9. Action Button Single Presses */
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        apply_current_tool(g_sim_viewport.cursor_x, g_sim_viewport.cursor_y);
    }
    if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        if (sim && sim->editor) {
            DoTool(sim->editor, queryState, g_sim_viewport.cursor_x, g_sim_viewport.cursor_y);
        }
    }

    /* Clamp camera to world boundary */
    int max_cam_x = SIM_MAP_WIDTH * SIM_TILE_SIZE - g_sim_viewport.view_w;
    int max_cam_y = SIM_MAP_HEIGHT * SIM_TILE_SIZE - g_sim_viewport.view_h;
    if (g_sim_viewport.cam_x < 0) g_sim_viewport.cam_x = 0;
    if (g_sim_viewport.cam_y < 0) g_sim_viewport.cam_y = 0;
    if (g_sim_viewport.cam_x > max_cam_x) g_sim_viewport.cam_x = max_cam_x;
    if (g_sim_viewport.cam_y > max_cam_y) g_sim_viewport.cam_y = max_cam_y;

    /* 10. Blink Animation (~2 Hz) */
    s_blink_accumulator += dt;
    if (s_blink_accumulator >= 0.25f) {
        s_blink_accumulator = 0.0f;
        flagBlink = !flagBlink;
        animateTiles();
    }

    /* 11. Simulation Timestep Execution (20 Hz fixed tick) */
    s_sim_accumulator += dt;
    float sim_interval = 0.15f;
    if (SimSpeed == 1) sim_interval = 0.30f;
    if (SimSpeed == 2) sim_interval = 0.15f;
    if (SimSpeed == 3) sim_interval = 0.05f;

    if (SimSpeed > 0 && s_sim_accumulator >= sim_interval) {
        s_sim_accumulator = 0.0f;
        SimFrame();
        MoveObjects();
        DoUpdateHeads();
        scoreDoer();
    }
}

static void micropolis_scene_draw(scene_t *self)
{
    (void)self;

    /* 1. Clear frame */
    render_clear(rgba(20, 20, 30, 255));

    /* 2. Render visible map tiles */
    sim_render_map(&g_sim_viewport);

    /* 3. Render dynamic sprites (train, helicopter, ship, monster, etc.) */
    sim_render_sprites(&g_sim_viewport);

    /* 4. Render tool placement cursor in play mode */
    if (s_game_mode == MODE_PLAY) {
        sim_render_cursor(&g_sim_viewport);
    }

    /* 5. Render minimap overlay if enabled */
    if (g_sim_viewport.show_minimap) {
        vec2i_t m_pos = vec2i(g_sim_viewport.view_w - 68, 22);
        vec2i_t m_size = vec2i(60, 50);
        sim_render_minimap(m_pos, m_size, &g_sim_viewport);
    }

    /* 6. Render top HUD / status bar */
    int current_year = CurrentYear();
    int month = (int)(CityTime % 12);
    sim_render_hud(&g_sim_viewport, CityName, TotalFunds,
                   TotalPop * 100, current_year, month,
                   s_tools[s_tool_index].name, SimSpeed);

    /* 7. Render R/C/I Demand Valves */
    sim_render_rci(vec2i(g_sim_viewport.view_w - 82, 2), RValve, CValve, IValve);

    /* 8. Render bottom tool palette */
    sim_render_tool_palette(&g_sim_viewport, s_tool_index, s_palette_timer > 0.0f);

    /* 9. Render active modal dialogs */
    vec2i_t center = vec2i(g_sim_viewport.view_w / 2, g_sim_viewport.view_h / 2);
    if (s_game_mode == MODE_QUERY) {
        sim_render_query_card(&s_query_info, center);
    } else if (s_game_mode == MODE_BUDGET) {
        sim_render_budget_modal(&s_budget_modal, center);
    } else if (s_game_mode == MODE_EVAL) {
        sim_render_eval_modal(&s_eval_modal, center);
    } else if (s_game_mode == MODE_DISASTERS || s_game_mode == MODE_SCENARIOS ||
               s_game_mode == MODE_MENU || s_game_mode == MODE_SAVE_SLOTS ||
               s_game_mode == MODE_LOAD_SLOTS) {
        sim_render_menu_modal(&s_menu_modal, center);
    }

    /* 10. Render floating toast notification */
    if (s_toast_timer > 0.0f) {
        float alpha = (s_toast_timer < 0.5f) ? (s_toast_timer / 0.5f) : 1.0f;
        sim_render_toast(s_toast_msg, alpha, vec2i(g_sim_viewport.view_w, g_sim_viewport.view_h));
    }
}

static void micropolis_scene_cleanup(scene_t *self)
{
    (void)self;
    sim_render_cleanup();
    s_initialized = false;
}

static scene_vtab_t micropolis_scene_vtab = {
    .init = micropolis_scene_init,
    .update = micropolis_scene_update,
    .draw = micropolis_scene_draw,
    .cleanup = micropolis_scene_cleanup
};

static scene_t micropolis_scene = {
    .vtab = &micropolis_scene_vtab
};

scene_t *game_get_scene(void)
{
    return &micropolis_scene;
}

#if !defined(PLAYDATE)
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!platform_init("micropolis", 800, 480)) {
        return 1;
    }

    render_init(800, 480);
    sound_init(44100);
    scene_manager_init();
    scene_set(scene_title_create());

    texture_t *backing_buffer = texture_create_target(vec2i(400, 240));
    double last_time = platform_now();

    while (platform_poll_events()) {
        double now = platform_now();
        float dt = (float)(now - last_time);
        if (dt > 0.1f) dt = 0.1f;
        last_time = now;

        render_set_target(backing_buffer);
        scene_update(dt);
        scene_draw();

        render_set_target(NULL);
        render_clear(rgba(0, 0, 0, 255));

        vec2i_t screen = platform_screen_size();
        render_draw_texture_scaled(backing_buffer, vec2i(0, 0), screen, rgba_white());

        platform_show_fps();
        platform_present();
    }

    sound_cleanup();
    platform_cleanup();
    return 0;
}
#endif
