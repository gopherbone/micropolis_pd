/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
/*
 * State shared between the in-game map view (game.c) and its screens
 * (screens.c). Not part of the public game API.
 */
#ifndef GAME_INTERNAL_H
#define GAME_INTERNAL_H

#include <stdbool.h>
#include "sim_input.h"
#include "map_view.h"

#define TOP_BAR_H 18
#define BOTTOM_BAR_H 24

typedef enum {
    TOOL_DOZER = 0, TOOL_ROAD, TOOL_RAIL, TOOL_WIRE,
    TOOL_RES, TOOL_COM, TOOL_IND, TOOL_PARK,
    TOOL_POLICE, TOOL_FIRE, TOOL_STADIUM, TOOL_QUERY,
    TOOL_COAL, TOOL_NUCLEAR, TOOL_SEAPORT, TOOL_AIRPORT,
    TOOL_COUNT
} tool_id_t;

typedef enum {
    PREVIEW_BLOCK = 0, /* size x size block of consecutive tiles from base */
    PREVIEW_ROW,       /* three copies of one tile in a row */
    PREVIEW_ICON       /* just the icon */
} preview_kind_t;

typedef struct {
    int state;          /* Micropolis tool state */
    int size;           /* footprint in tiles */
    int icon;
    const char *name;
    const char *desc;
    int preview_tile;
    preview_kind_t preview;
} tool_def_t;

extern const tool_def_t g_tools[TOOL_COUNT];

typedef enum {
    MODE_PLAY = 0,
    MODE_BUILD,
    MODE_BUDGET,
    MODE_REPORT,
    MODE_MAP,
    MODE_MENU,
    MODE_QUERY,
    MODE_NOTICE
} game_mode_t;

typedef struct {
    game_mode_t mode;
    int tool;
    int zoom;            /* 16 or 8 */
    float cam_x, cam_y;  /* world pixels at current zoom (top-left of screen) */
    int cur_x, cur_y;    /* cursor footprint top-left, in tiles */
    float time;          /* seconds since scene start (animations) */
} game_state_t;

extern game_state_t G;

/* Camera in whole screen pixels, snapped to even values. The tile art's
 * dithers repeat every 2px, so moving in 2px steps keeps them stable on
 * screen instead of flipping phase (shimmering) on odd-pixel moves. */
static inline int cam_px(float v)
{
    int i = (int)(v < 0 ? v - 1.0f : v);
    return i & ~1;
}
#define CAM_X() cam_px(G.cam_x)
#define CAM_Y() cam_px(G.cam_y)

/* --- game.c services used by screens ------------------------------------ */
int  game_tool_cost(int tool);
void game_select_tool(int tool);
void game_set_speed(int speed);
void game_set_zoom(int zoom);
void game_center_on(int tx, int ty);
void game_show_message(const char *msg, int icon);
void game_quit_to_title(void);
bool input_nav(sim_action_t act);   /* pressed, or auto-repeat while held */
float input_crank(void);            /* raw crank degrees this frame */
int  input_crank_steps(float detent); /* crank detents crossed this frame (+/-) */

/* Shared HUD pieces */
void hud_draw_top(void);
void game_draw_world(void);  /* map + sprites at the current camera */

/* --- screens.c ----------------------------------------------------------- */
void build_open(void);
void build_update(float dt);
void build_draw(void);

void budget_open(void);
void budget_update(float dt);
void budget_draw(void);

void report_open(void);
void report_update(float dt);
void report_draw(void);

void citymap_open(map_layer_t layer);
void citymap_update(float dt);
void citymap_draw(void);

void menu_open_game(void);
void menu_open_layers(void);
void menu_update(float dt);
void menu_draw(void);

void query_set(const char *title, const char *density, const char *value,
               const char *crime, const char *pollution, const char *growth,
               int x, int y, bool powered);
void query_update(float dt);
void query_draw(void);

void notice_open(int id, const char *msg, int x, int y);
void notice_update(float dt);
void notice_draw(void);

#endif
