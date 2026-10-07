/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#include <string.h>
#include <stdio.h>

#include "map_view.h"
#include "ui.h"
#include "render.h"
#include "sim.h"

#define LIGHTNING_TILE 827

static texture_t *s_cache[2];          /* [0] = 16px, [1] = 8px */
static unsigned short s_shadow[CITY_W * CITY_H];
static bool s_valid = false;

static texture_t *s_overview = NULL;
static map_layer_t s_overview_layer = LAYER_COUNT;

static image_t *s_sprites = NULL;

static const char *s_layer_names[LAYER_COUNT] = {
    "City", "Power Grid", "Traffic", "Pollution", "Crime",
    "Land Value", "Population", "Police Cover", "Fire Cover"
};

static const char *s_layer_legend[LAYER_COUNT] = {
    "Black: roads & services\nDark: industry\nLight: homes",
    "Black: powered\nHatched: no power",
    "Darker = heavier traffic",
    "Darker = more pollution",
    "Darker = more crime",
    "Darker = more valuable",
    "Darker = more people",
    "Darker = better coverage",
    "Darker = better coverage",
};

const char *mapview_layer_name(map_layer_t layer)
{
    return (layer >= 0 && layer < LAYER_COUNT) ? s_layer_names[layer] : "";
}

const char *mapview_layer_legend(map_layer_t layer)
{
    return (layer >= 0 && layer < LAYER_COUNT) ? s_layer_legend[layer] : "";
}

void mapview_init(void)
{
    if (!s_cache[0]) s_cache[0] = texture_create_target(vec2i(CITY_W * 16, CITY_H * 16));
    if (!s_cache[1]) s_cache[1] = texture_create_target(vec2i(CITY_W * 8, CITY_H * 8));
    if (!s_overview) s_overview = texture_create_target(vec2i(CITY_W * 2, CITY_H * 2));
    if (!s_sprites) s_sprites = image_load("assets/gfx/sprites_16.png");
    s_valid = false;
}

void mapview_invalidate(void)
{
    s_valid = false;
    s_overview_layer = LAYER_COUNT;
}

void mapview_sync(void)
{
    if (!Map[0]) return;
    for (int c = 0; c < 2; c++) {
        texture_t *cache = s_cache[c];
        if (!cache) continue;
        int ts = c == 0 ? 16 : 8;
        render_set_target(cache);
        for (int y = 0; y < CITY_H; y++) {
            for (int x = 0; x < CITY_W; x++) {
                unsigned short t = Map[x][y] & LOMASK;
                if (s_valid && s_shadow[y * CITY_W + x] == t) continue;
                if (ts == 16) ui_tile16(t, x * 16, y * 16);
                else ui_tile8(t, x * 8, y * 8);
            }
        }
        render_set_target(NULL);
    }
    for (int y = 0; y < CITY_H; y++)
        for (int x = 0; x < CITY_W; x++) s_shadow[y * CITY_W + x] = Map[x][y] & LOMASK;
    s_valid = true;
}

void mapview_draw(int zoom, int cam_x, int cam_y, int x, int y, int w, int h)
{
    if (!Map[0]) return;
    int ci = zoom == 16 ? 0 : 1;

    render_set_clip(vec2i(x, y), vec2i(w, h));
    if (s_cache[ci]) {
        render_draw_texture(s_cache[ci], vec2i(x - cam_x, y - cam_y));
    } else {
        /* Fallback if the big cache bitmap couldn't be allocated */
        int tx0 = cam_x / zoom, ty0 = cam_y / zoom;
        for (int ty = ty0; ty <= ty0 + h / zoom + 1 && ty < CITY_H; ty++)
            for (int tx = tx0; tx <= tx0 + w / zoom + 1 && tx < CITY_W; tx++) {
                if (tx < 0 || ty < 0) continue;
                unsigned short t = Map[tx][ty] & LOMASK;
                int sx = x + tx * zoom - cam_x, sy = y + ty * zoom - cam_y;
                if (zoom == 16) ui_tile16(t, sx, sy);
                else ui_tile8(t, sx, sy);
            }
    }

    /* Blinking lightning bolt on unpowered zone centres */
    if (flagBlink) {
        int tx0 = cam_x / zoom, ty0 = cam_y / zoom;
        for (int ty = ty0; ty <= ty0 + h / zoom + 1 && ty < CITY_H; ty++) {
            for (int tx = tx0; tx <= tx0 + w / zoom + 1 && tx < CITY_W; tx++) {
                if (tx < 0 || ty < 0) continue;
                unsigned short cell = Map[tx][ty];
                if ((cell & ZONEBIT) && !(cell & PWRBIT)) {
                    int sx = x + tx * zoom - cam_x, sy = y + ty * zoom - cam_y;
                    if (zoom == 16) ui_tile16(LIGHTNING_TILE, sx, sy);
                    else ui_tile8(LIGHTNING_TILE, sx, sy);
                }
            }
        }
    }
    render_set_clip(vec2i(0, 0), vec2i(0, 0));
}

void mapview_draw_sprites(int zoom, int cam_x, int cam_y, int x, int y, int w, int h)
{
    if (!sim) return;
    render_set_clip(vec2i(x, y), vec2i(w, h));
    for (SimSprite *s = sim->sprite; s != NULL; s = s->next) {
        if (s->frame == 0) continue;

        int wx, wy; /* world pixel at 16px scale, top-left of the sprite */
        if (s->type == TRA) {
            wx = s->x + 48;
            wy = s->y;
        } else {
            wx = s->x + s->x_hot;
            wy = s->y + s->y_hot;
        }
        int sx = x + wx * zoom / 16 - cam_x;
        int sy = y + wy * zoom / 16 - cam_y;
        if (zoom == 8) {
            sx -= 4;
            sy -= 4;
        }
        if (sx < x - 16 || sx >= x + w || sy < y - 16 || sy >= y + h) continue;

        int id;
        switch (s->type) {
        case TRA: id = 0; break;
        case COP: id = 1; break;
        case AIR: id = 2; break;
        case SHI: id = 3; break;
        case GOD: id = 4; break;
        case TOR: id = 5; break;
        case EXP:
            ui_tile16(44 + (s->frame % 4), sx, sy);
            continue;
        default: id = 0; break;
        }
        if (s_sprites) {
            image_draw_tile(s_sprites, id, vec2i(16, 16), vec2i(sx, sy), false, false, rgba_white());
        }
    }
    render_set_clip(vec2i(0, 0), vec2i(0, 0));
}

int mapview_tile_gray(unsigned short t)
{
    if (t == DIRT) return 16;
    if (t >= RIVER && t <= LASTRIVEDGE) return 2;        /* water */
    if (t >= TREEBASE && t <= WOODS5) return 9;          /* trees */
    if (t >= RUBBLE && t <= LASTRUBBLE) return 12;
    if (t >= FLOOD && t <= LASTFLOOD) return 3;
    if (t >= FIREBASE && t <= LASTFIRE) return 0;
    if (t >= ROADBASE && t <= LASTROAD) return 0;
    if (t >= POWERBASE && t <= LASTPOWER) return 11;
    if (t >= RAILBASE && t <= LASTRAIL) return 0;
    if (t >= RESBASE && t < COMBASE) return 13;
    if (t >= COMBASE && t < INDBASE) return 7;
    if (t >= INDBASE && t < PORTBASE) return 4;
    if (t >= LASTZONE && t < 960) return 9;              /* parks / misc animated */
    return 0;                                             /* civic & power plants */
}

static int data_gray(map_layer_t layer, int x, int y, int *out_is_data)
{
    int v = 0;
    *out_is_data = 1;
    int hx = x >> 1, hy = y >> 1;
    int sx = x >> 3, sy = y >> 3;
    switch (layer) {
    case LAYER_TRAFFIC: v = TrfDensity[hx][hy]; break;
    case LAYER_POLLUTION: v = PollutionMem[hx][hy]; break;
    case LAYER_CRIME: v = CrimeMem[hx][hy]; break;
    case LAYER_LAND_VALUE: v = LandValueMem[hx][hy]; break;
    case LAYER_POPULATION: v = PopDensity[hx][hy] * 2; break;
    case LAYER_POLICE: v = PoliceMapEffect[sx][sy] / 4; break;
    case LAYER_FIRE: v = FireRate[sx][sy] / 4; break;
    default: *out_is_data = 0; return 16;
    }
    if (v > 255) v = 255;
    if (v < 0) v = 0;
    if (v < 8) return 16;
    return 14 - v * 14 / 255;
}

void mapview_draw_overview(int x, int y, map_layer_t layer, bool refresh)
{
    if (!Map[0]) return;
    if (s_overview && (refresh || layer != s_overview_layer)) {
        render_set_target(s_overview);
        for (int ty = 0; ty < CITY_H; ty++) {
            for (int tx = 0; tx < CITY_W; tx++) {
                unsigned short cell = Map[tx][ty];
                unsigned short t = cell & LOMASK;
                bool water = t >= RIVER && t <= LASTRIVEDGE;
                int g;
                if (layer == LAYER_CITY) {
                    g = mapview_tile_gray(t);
                } else if (water) {
                    g = 3;
                } else if (layer == LAYER_POWER) {
                    if (cell & CONDBIT) g = (cell & PWRBIT) ? 0 : 7;
                    else g = (t >= ROADBASE && t <= LASTROAD) ? 13 : 16;
                } else {
                    int is_data;
                    g = data_gray(layer, tx, ty, &is_data);
                    if (g == 16 && t >= ROADBASE && t <= LASTRAIL) g = 13;
                }
                ui_gray(tx * 2, ty * 2, 2, 2, g);
            }
        }
        render_set_target(NULL);
        s_overview_layer = layer;
    }
    if (s_overview) render_draw_texture(s_overview, vec2i(x, y));
}
