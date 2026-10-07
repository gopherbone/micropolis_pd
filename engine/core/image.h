#ifndef TINY_IMAGE_H
#define TINY_IMAGE_H

#include "types.h"

typedef struct image_t image_t;

image_t *image_load(const char *path);
void image_draw_tile(image_t *img, int tile, vec2i_t tile_size, vec2i_t pos,
                     bool flip_x, bool flip_y, rgba_t tint);

#endif
