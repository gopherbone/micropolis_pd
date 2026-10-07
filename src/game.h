/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "scene.h"

scene_t *game_get_scene(void);

/* Simulation lifecycle (safe to call from the title scene) */
void game_ensure_init(void);
bool game_has_city(void);                       /* a city/terrain is loaded */
void game_generate_terrain(int seed);           /* fresh terrain, no city yet */
int  game_load_backdrop(int demo);              /* demo city for the title; returns the one used */
bool game_city_bounds(int *x0, int *y0, int *x1, int *y1); /* built-up area, in tiles */

/* Demo cities: classic Micropolis example maps */
int  game_demo_count(void);
const char *game_demo_name(int i);
void game_start_demo(int demo);
void game_start_new_city(int difficulty);       /* uses the current terrain */
void game_start_scenario(int scenario_id);
void game_start_embedded_city(const char *name);
int  game_save_slot(int slot);
int  game_load_slot(int slot);

/* Autosave ("Continue") */
bool game_has_autosave(void);
bool game_autosave_meta(char *name, int name_len, int *year, long *pop);
void game_autosave(void);
bool game_continue(void);

/* Playdate system-menu shortcuts */
void game_open_budget(void);
void game_open_eval(void);
void game_open_system_menu(void);

/* Suggested city name for a new city */
const char *game_random_city_name(int seed);

#endif // GAME_H
