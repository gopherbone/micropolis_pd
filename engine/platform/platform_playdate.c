/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "../core/pd_shim.h"

PlaydateAPI *g_pd = NULL;

static input_state_t s_input;

void platform_playdate_set_api(PlaydateAPI *api)
{
    g_pd = api;
}

void *platform_playdate_get_api(void)
{
    return g_pd;
}

bool platform_init(const char *title, int width, int height)
{
    (void)title;
    (void)width;
    (void)height;
    memset(&s_input, 0, sizeof(s_input));
    /* No pointer on Playdate: park the "mouse" far off-screen */
    s_input.mouse_pos = vec2(-10000.0f, -10000.0f);
    return g_pd != NULL;
}

void platform_cleanup(void)
{
}

bool platform_poll_events(void)
{
    PDButtons current = 0, pushed = 0, released = 0;
    g_pd->system->getButtonState(&current, &pushed, &released);

    static const struct { PDButtons button; int key; } map[] = {
        { kButtonUp, KEY_UP },
        { kButtonDown, KEY_DOWN },
        { kButtonLeft, KEY_LEFT },
        { kButtonRight, KEY_RIGHT },
        { kButtonA, KEY_A },
        { kButtonB, KEY_B },
    };

    memset(s_input.held, 0, sizeof(s_input.held));
    memset(s_input.pressed, 0, sizeof(s_input.pressed));
    memset(s_input.released, 0, sizeof(s_input.released));
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        s_input.held[map[i].key] = (current & map[i].button) != 0;
        s_input.pressed[map[i].key] = (pushed & map[i].button) != 0;
        s_input.released[map[i].key] = (released & map[i].button) != 0;
    }

    s_input.crank_change = g_pd->system->isCrankDocked() ? 0.0f : g_pd->system->getCrankChange();
    return true;
}

const input_state_t *platform_get_input(void)
{
    return &s_input;
}

void platform_present(void)
{
    /* The Playdate flushes the frame buffer when the update callback returns */
}

void platform_show_fps(void)
{
}

double platform_now(void)
{
    unsigned int ms = 0;
    unsigned int sec = g_pd->system->getSecondsSinceEpoch(&ms);
    return (double)sec + ms / 1000.0;
}

vec2i_t platform_screen_size(void)
{
    return vec2i(LCD_COLUMNS, LCD_ROWS);
}

bool platform_save_data(const char *name, const void *data, uint32_t size)
{
    if (!name || !data) return false;
    SDFile *f = g_pd->file->open(name, kFileWrite);
    if (!f) return false;
    int written = g_pd->file->write(f, data, size);
    g_pd->file->close(f);
    return written == (int)size;
}

uint8_t *platform_load_data(const char *name, uint32_t *out_size)
{
    static uint8_t *s_buf = NULL;
    static uint32_t s_cap = 0;

    if (out_size) *out_size = 0;
    if (!name) return NULL;

    FileStat st;
    if (g_pd->file->stat(name, &st) != 0 || st.isdir) return NULL;

    SDFile *f = g_pd->file->open(name, kFileReadData | kFileRead);
    if (!f) return NULL;

    uint32_t size = st.size;
    if (size + 1 > s_cap) {
        uint8_t *nb = realloc(s_buf, size + 1);
        if (!nb) {
            g_pd->file->close(f);
            return NULL;
        }
        s_buf = nb;
        s_cap = size + 1;
    }
    int got = g_pd->file->read(f, s_buf, size);
    g_pd->file->close(f);
    if (got != (int)size) return NULL;

    s_buf[size] = 0;
    if (out_size) *out_size = size;
    return s_buf;
}

#define MAX_MENU_ITEMS 3
static PDMenuItem *s_menu_items[MAX_MENU_ITEMS];
static int s_menu_count = 0;

int platform_menu_add_item(const char *title, platform_menu_cb cb, void *userdata)
{
    if (s_menu_count >= MAX_MENU_ITEMS) return -1;
    PDMenuItem *m = g_pd->system->addMenuItem(title, cb, userdata);
    if (!m) return -1;
    s_menu_items[s_menu_count] = m;
    return s_menu_count++;
}

int platform_menu_add_options(const char *title, const char **options, int count,
                              platform_menu_cb cb, void *userdata)
{
    if (s_menu_count >= MAX_MENU_ITEMS) return -1;
    PDMenuItem *m = g_pd->system->addOptionsMenuItem(title, options, count, cb, userdata);
    if (!m) return -1;
    s_menu_items[s_menu_count] = m;
    return s_menu_count++;
}

int platform_menu_get_value(int handle)
{
    if (handle < 0 || handle >= s_menu_count) return 0;
    return g_pd->system->getMenuItemValue(s_menu_items[handle]);
}

void platform_menu_set_value(int handle, int value)
{
    if (handle < 0 || handle >= s_menu_count) return;
    g_pd->system->setMenuItemValue(s_menu_items[handle], value);
}

void platform_menu_clear(void)
{
    g_pd->system->removeAllMenuItems();
    s_menu_count = 0;
}
