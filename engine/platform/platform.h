/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#ifndef TINY_PLATFORM_H
#define TINY_PLATFORM_H

#include "../core/types.h"

typedef enum {
    KEY_UP = 0,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_A,
    KEY_B,
    KEY_START,
    KEY_SELECT,
    KEY_PAGEUP,
    KEY_PAGEDOWN,
    KEY_COUNT
} key_t_;

typedef struct {
    bool held[KEY_COUNT];
    bool pressed[KEY_COUNT];
    bool released[KEY_COUNT];
    float crank_change;
    vec2_t mouse_pos;
    bool mouse_down;
} input_state_t;

bool platform_init(const char *title, int width, int height);
void platform_cleanup(void);
bool platform_poll_events(void);
void platform_present(void);
void platform_show_fps(void);
double platform_now(void);
vec2i_t platform_screen_size(void);
const input_state_t *platform_get_input(void);

/* Persistent storage in the game's Data folder. Returned buffer is owned by
 * the platform layer and stays valid until the next load call. */
bool platform_save_data(const char *name, const void *data, uint32_t size);
uint8_t *platform_load_data(const char *name, uint32_t *out_size);

void *platform_playdate_get_api(void);

/* System menu (Playdate pause menu). Handles are small ints, -1 on failure. */
typedef void (*platform_menu_cb)(void *userdata);
int platform_menu_add_item(const char *title, platform_menu_cb cb, void *userdata);
int platform_menu_add_options(const char *title, const char **options, int count,
                              platform_menu_cb cb, void *userdata);
int platform_menu_get_value(int handle);
void platform_menu_set_value(int handle, int value);
void platform_menu_clear(void);

#endif
