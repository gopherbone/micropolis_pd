/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
/*
 * City map rendering.
 *
 * The whole 120x100 map is kept pre-rendered in two off-screen bitmaps
 * (16px and 8px tiles). Each frame only tiles whose id changed are redrawn,
 * so scrolling is a single clipped blit regardless of zoom.
 */
#ifndef MAP_VIEW_H
#define MAP_VIEW_H

#include <stdbool.h>

#define CITY_W 120
#define CITY_H 100

typedef enum {
    LAYER_CITY = 0,
    LAYER_POWER,
    LAYER_TRAFFIC,
    LAYER_POLLUTION,
    LAYER_CRIME,
    LAYER_LAND_VALUE,
    LAYER_POPULATION,
    LAYER_POLICE,
    LAYER_FIRE,
    LAYER_COUNT
} map_layer_t;

const char *mapview_layer_name(map_layer_t layer);
const char *mapview_layer_legend(map_layer_t layer); /* what dark means */

void mapview_init(void);
void mapview_invalidate(void);  /* force a full redraw (after load / new map) */
void mapview_sync(void);        /* redraw changed tiles into the caches */

/* Draw the map with tile size `zoom` (16 or 8) into the screen rect.
 * cam is the world pixel (at that zoom) shown at the rect's top-left. */
void mapview_draw(int zoom, int cam_x, int cam_y, int x, int y, int w, int h);
void mapview_draw_sprites(int zoom, int cam_x, int cam_y, int x, int y, int w, int h);

/* Whole-city overview at 2px per tile (240x200). Re-rendered when refresh. */
void mapview_draw_overview(int x, int y, map_layer_t layer, bool refresh);

/* Gray level (0 black .. 16 white) used for a tile in the overview's city layer */
int mapview_tile_gray(unsigned short tile);

#endif
