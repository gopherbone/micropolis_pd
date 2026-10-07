/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#ifndef TINY_RENDER_H
#define TINY_RENDER_H

#include "types.h"

typedef struct texture_t texture_t;

void render_init(int width, int height);
void render_clear(rgba_t color);
void render_fill_rect(vec2i_t pos, vec2i_t size, rgba_t color);
void render_draw_rect(vec2i_t pos, vec2i_t size, rgba_t color);
void render_draw_line(vec2i_t a, vec2i_t b, rgba_t color);
void render_draw_point(vec2i_t pos, rgba_t color);

/* Off-screen render targets */
texture_t *texture_create_target(vec2i_t size);
void texture_destroy(texture_t *tex);
vec2i_t texture_size(texture_t *tex);
void render_set_target(texture_t *target);
void render_draw_texture_scaled(texture_t *tex, vec2i_t pos, vec2i_t size, rgba_t tint);
void render_draw_texture(texture_t *tex, vec2i_t pos);

/* Clip subsequent drawing to a rectangle (size <= 0 clears the clip) */
void render_set_clip(vec2i_t pos, vec2i_t size);

/* Invert every pixel in the rectangle (selection highlights) */
void render_invert_rect(vec2i_t pos, vec2i_t size);

/* Shim internals shared with image/font: luminance-threshold a color to 1-bit */
bool render_color_is_light(rgba_t color);

#endif
