/*
 * 1-bit renderer for the Playdate.
 *
 * Opaque fills are converted to an ordered-dither pattern based on luminance,
 * so the game's colour palette maps to grey levels. Strokes, text and points
 * are thresholded so they stay crisp. Translucent fills (map overlays) become
 * sparse black stipple whose density follows alpha.
 */
#include <stdlib.h>

#include "render.h"
#include "pd_shim.h"

#define DITHER_LEVELS 17

static const uint8_t s_bayer4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

/* LCDPattern: 8 rows of bitmap followed by 8 rows of mask */
static LCDPattern s_fill_patterns[DITHER_LEVELS];
static LCDPattern s_stipple_patterns[DITHER_LEVELS];

static int luminance(rgba_t c)
{
    return (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
}

bool render_color_is_light(rgba_t color)
{
    return luminance(color) >= 100;
}

static int dither_level(rgba_t c)
{
    int l = luminance(c);
    if (l < 40) return 0;
    if (l > 215) return DITHER_LEVELS - 1;
    return l * DITHER_LEVELS / 256;
}

void render_init(int width, int height)
{
    (void)width;
    (void)height;

    for (int level = 0; level < DITHER_LEVELS; level++) {
        for (int row = 0; row < 8; row++) {
            uint8_t bits = 0;
            for (int col = 0; col < 8; col++) {
                if (s_bayer4[row & 3][col & 3] < level) bits |= (uint8_t)(0x80 >> col);
            }
            /* fill: white where bayer < level, fully opaque */
            s_fill_patterns[level][row] = bits;
            s_fill_patterns[level][row + 8] = 0xFF;
            /* stipple: black pixels, opaque only where bayer < level */
            s_stipple_patterns[level][row] = 0x00;
            s_stipple_patterns[level][row + 8] = bits;
        }
    }
}

static LCDColor fill_color(rgba_t c)
{
    if (c.a == 255) {
        int level = dither_level(c);
        if (level == 0) return kColorBlack;
        if (level == DITHER_LEVELS - 1) return kColorWhite;
        return (LCDColor)(uintptr_t)s_fill_patterns[level];
    }
    /* Translucent overlay: stipple density ~ 3/4 of alpha */
    int level = (c.a * 3 / 4) * DITHER_LEVELS / 256;
    if (level <= 0) return kColorClear;
    return (LCDColor)(uintptr_t)s_stipple_patterns[level];
}

static LCDColor stroke_color(rgba_t c)
{
    if (c.a == 0) return kColorClear;
    return render_color_is_light(c) ? kColorWhite : kColorBlack;
}

void render_clear(rgba_t color)
{
    g_pd->graphics->clear(fill_color(rgba(color.r, color.g, color.b, 255)));
}

void render_fill_rect(vec2i_t pos, vec2i_t size, rgba_t color)
{
    if (size.x <= 0 || size.y <= 0) return;
    LCDColor c = fill_color(color);
    if (c == kColorClear) return;
    g_pd->graphics->fillRect(pos.x, pos.y, size.x, size.y, c);
}

void render_draw_rect(vec2i_t pos, vec2i_t size, rgba_t color)
{
    if (size.x <= 0 || size.y <= 0) return;
    LCDColor c = stroke_color(color);
    if (c == kColorClear) return;
    g_pd->graphics->drawRect(pos.x, pos.y, size.x, size.y, c);
}

void render_draw_line(vec2i_t a, vec2i_t b, rgba_t color)
{
    LCDColor c = stroke_color(color);
    if (c == kColorClear) return;
    g_pd->graphics->drawLine(a.x, a.y, b.x, b.y, 1, c);
}

void render_draw_point(vec2i_t pos, rgba_t color)
{
    if (pos.x < 0 || pos.y < 0 || pos.x >= LCD_COLUMNS || pos.y >= LCD_ROWS) return;
    if (color.a == 0) return;
    int level = dither_level(color);
    bool white = s_bayer4[pos.y & 3][pos.x & 3] < level;
    g_pd->graphics->setPixel(pos.x, pos.y, white ? kColorWhite : kColorBlack);
}

struct texture_t {
    LCDBitmap *bitmap;
    int w, h;
};

texture_t *texture_create_target(vec2i_t size)
{
    LCDBitmap *b = g_pd->graphics->newBitmap(size.x, size.y, kColorBlack);
    if (!b) return NULL;
    texture_t *t = malloc(sizeof(texture_t));
    if (!t) {
        g_pd->graphics->freeBitmap(b);
        return NULL;
    }
    t->bitmap = b;
    t->w = size.x;
    t->h = size.y;
    return t;
}

void texture_destroy(texture_t *tex)
{
    if (!tex) return;
    g_pd->graphics->freeBitmap(tex->bitmap);
    free(tex);
}

vec2i_t texture_size(texture_t *tex)
{
    return tex ? vec2i(tex->w, tex->h) : vec2i(0, 0);
}

static bool s_target_pushed = false;
static int s_target_w = LCD_COLUMNS, s_target_h = LCD_ROWS;

void shim_target_size(int *w, int *h)
{
    *w = s_target_w;
    *h = s_target_h;
}

void render_set_target(texture_t *target)
{
    if (s_target_pushed) {
        g_pd->graphics->popContext();
        s_target_pushed = false;
    }
    s_target_w = LCD_COLUMNS;
    s_target_h = LCD_ROWS;
    if (target) {
        g_pd->graphics->pushContext(target->bitmap);
        s_target_pushed = true;
        s_target_w = target->w;
        s_target_h = target->h;
    }
}

void render_draw_texture(texture_t *tex, vec2i_t pos)
{
    if (!tex) return;
    g_pd->graphics->drawBitmap(tex->bitmap, pos.x, pos.y, kBitmapUnflipped);
}

void render_draw_texture_scaled(texture_t *tex, vec2i_t pos, vec2i_t size, rgba_t tint)
{
    (void)tint;
    if (!tex) return;
    if (size.x == tex->w && size.y == tex->h) {
        render_draw_texture(tex, pos);
    } else {
        g_pd->graphics->drawScaledBitmap(tex->bitmap, pos.x, pos.y,
                                         (float)size.x / tex->w, (float)size.y / tex->h);
    }
}

static bool s_base_clip = false;
static int s_clip_x, s_clip_y, s_clip_w, s_clip_h;

void render_set_clip(vec2i_t pos, vec2i_t size)
{
    s_base_clip = size.x > 0 && size.y > 0;
    s_clip_x = pos.x;
    s_clip_y = pos.y;
    s_clip_w = size.x;
    s_clip_h = size.y;
    shim_pop_clip();
}

void shim_push_clip(int x, int y, int w, int h)
{
    if (s_base_clip) {
        int x1 = x + w, y1 = y + h;
        if (x < s_clip_x) x = s_clip_x;
        if (y < s_clip_y) y = s_clip_y;
        if (x1 > s_clip_x + s_clip_w) x1 = s_clip_x + s_clip_w;
        if (y1 > s_clip_y + s_clip_h) y1 = s_clip_y + s_clip_h;
        w = x1 - x;
        h = y1 - y;
        if (w <= 0 || h <= 0) {
            /* Fully clipped: a 0-size rect would mean "no clip", so park it off-screen */
            x = -10;
            y = -10;
            w = h = 1;
        }
    }
    g_pd->graphics->setClipRect(x, y, w, h);
}

void shim_pop_clip(void)
{
    if (s_base_clip) g_pd->graphics->setClipRect(s_clip_x, s_clip_y, s_clip_w, s_clip_h);
    else g_pd->graphics->clearClipRect();
}

void render_invert_rect(vec2i_t pos, vec2i_t size)
{
    if (size.x <= 0 || size.y <= 0) return;
    g_pd->graphics->fillRect(pos.x, pos.y, size.x, size.y, kColorXOR);
}
