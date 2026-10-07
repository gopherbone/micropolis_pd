#include <stdlib.h>
#include <string.h>

#include "image.h"
#include "pd_shim.h"

struct image_t {
    char path[128];
    LCDBitmap *bitmap;
    int width;
    int height;
};

#define MAX_IMAGES 16
static image_t s_images[MAX_IMAGES];
static int s_image_count = 0;

/* "assets/gfx/foo.png" -> "assets/gfx/foo" (pdc compiles PNGs to .pdi) */
static void strip_extension(const char *path, char *out, size_t out_len)
{
    strncpy(out, path, out_len - 1);
    out[out_len - 1] = '\0';
    char *dot = strrchr(out, '.');
    char *slash = strrchr(out, '/');
    if (dot && (!slash || dot > slash)) *dot = '\0';
}

image_t *image_load(const char *path)
{
    if (!path) return NULL;

    /* Cached: the game drops its pointers on scene cleanup and reloads later */
    for (int i = 0; i < s_image_count; i++) {
        if (strcmp(s_images[i].path, path) == 0) return &s_images[i];
    }
    if (s_image_count >= MAX_IMAGES) return NULL;

    char base[128];
    strip_extension(path, base, sizeof(base));

    const char *err = NULL;
    LCDBitmap *bmp = g_pd->graphics->loadBitmap(base, &err);
    if (!bmp) {
        g_pd->system->logToConsole("image_load: %s failed: %s", base, err ? err : "?");
        return NULL;
    }

    image_t *img = &s_images[s_image_count++];
    strncpy(img->path, path, sizeof(img->path) - 1);
    img->bitmap = bmp;
    g_pd->graphics->getBitmapData(bmp, &img->width, &img->height, NULL, NULL, NULL);
    return img;
}

void image_draw_tile(image_t *img, int tile, vec2i_t tile_size, vec2i_t pos,
                     bool flip_x, bool flip_y, rgba_t tint)
{
    (void)tint;
    if (!img || tile < 0 || tile_size.x <= 0 || tile_size.y <= 0) return;

    int cols = img->width / tile_size.x;
    if (cols <= 0) return;
    int rows = img->height / tile_size.y;
    if (tile >= cols * rows) return;

    /* Cull fully off-screen tiles before touching the clip rect */
    int tw, th;
    shim_target_size(&tw, &th);
    if (pos.x >= tw || pos.y >= th ||
        pos.x + tile_size.x <= 0 || pos.y + tile_size.y <= 0) return;

    int sx = (tile % cols) * tile_size.x;
    int sy = (tile / cols) * tile_size.y;

    LCDBitmapFlip flip = kBitmapUnflipped;
    int ox = pos.x - sx;
    int oy = pos.y - sy;
    if (flip_x && flip_y) {
        flip = kBitmapFlippedXY;
        ox = pos.x - (img->width - sx - tile_size.x);
        oy = pos.y - (img->height - sy - tile_size.y);
    } else if (flip_x) {
        flip = kBitmapFlippedX;
        ox = pos.x - (img->width - sx - tile_size.x);
    } else if (flip_y) {
        flip = kBitmapFlippedY;
        oy = pos.y - (img->height - sy - tile_size.y);
    }

    shim_push_clip(pos.x, pos.y, tile_size.x, tile_size.y);
    g_pd->graphics->drawBitmap(img->bitmap, ox, oy, flip);
    shim_pop_clip();
}
