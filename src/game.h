#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include "scene.h"

scene_t *game_get_scene(void);

/* Simulation lifecycle (safe to call from the title scene) */
void game_ensure_init(void);
bool game_has_city(void);                       /* a city/terrain is loaded */
void game_generate_terrain(int seed);           /* fresh terrain, no city yet */
void game_load_backdrop(void);                  /* example city for the title screen */
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
