#ifndef TINY_FONT_H
#define TINY_FONT_H

#include "types.h"

typedef struct bmfont_t bmfont_t;

typedef enum {
    FONT_ALIGN_LEFT = 0,
    FONT_ALIGN_CENTER,
    FONT_ALIGN_RIGHT
} font_align_t;

bmfont_t *font_load_bmfont(const char *path, void *reserved);
void font_draw_bmfont(bmfont_t *font, vec2i_t pos, const char *text, font_align_t align, rgba_t color);
int font_bmfont_get_width(bmfont_t *font, const char *text);
int font_bmfont_get_height(bmfont_t *font);

#endif
