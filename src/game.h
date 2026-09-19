#ifndef GAME_H
#define GAME_H

#include "scene.h"

scene_t *game_get_scene(void);

void game_start_new_city(int difficulty);
void game_start_scenario(int scenario_id);
void game_start_embedded_city(const char *name);

void game_open_budget(void);
void game_open_eval(void);
void game_open_system_menu(void);

int game_save_slot(int slot);
int game_load_slot(int slot);

#endif // GAME_H


