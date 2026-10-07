/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
/*
 * AngelCode BMFont (XML) loader + renderer.
 * The page PNG is preprocessed to white glyphs with alpha, so glyphs can be
 * drawn in either colour with kDrawModeFillWhite / kDrawModeFillBlack.
 */
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "render.h"
#include "pd_shim.h"

typedef struct {
    int16_t x, y, w, h;
    int16_t xoff, yoff, xadv;
    bool valid;
} glyph_t;

struct bmfont_t {
    char path[128];
    LCDBitmap *page;
    int line_height;
    glyph_t glyphs[256];
};

#define MAX_FONTS 4
static bmfont_t s_fonts[MAX_FONTS];
static int s_font_count = 0;

static int attr_int(const char *line, const char *name, int def)
{
    char key[32];
    size_t n = strlen(name);
    if (n + 3 > sizeof(key)) return def;
    key[0] = ' ';
    memcpy(key + 1, name, n);
    key[n + 1] = '=';
    key[n + 2] = '\0';
    const char *p = strstr(line, key);
    if (!p) return def;
    p += n + 2;
    if (*p == '"') p++;
    return atoi(p);
}

static bool attr_str(const char *line, const char *name, char *out, size_t out_len)
{
    char key[32];
    size_t n = strlen(name);
    if (n + 4 > sizeof(key)) return false;
    key[0] = ' ';
    memcpy(key + 1, name, n);
    memcpy(key + n + 1, "=\"", 3);
    const char *p = strstr(line, key);
    if (!p) return false;
    p += n + 3;
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t len = (size_t)(end - p);
    if (len >= out_len) len = out_len - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static char *read_file(const char *path, int *out_len)
{
    SDFile *f = g_pd->file->open(path, kFileRead);
    if (!f) return NULL;
    g_pd->file->seek(f, 0, SEEK_END);
    int len = g_pd->file->tell(f);
    g_pd->file->seek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        g_pd->file->close(f);
        return NULL;
    }
    int got = g_pd->file->read(f, buf, (unsigned)len);
    g_pd->file->close(f);
    if (got < 0) got = 0;
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

bmfont_t *font_load_bmfont(const char *path, void *reserved)
{
    (void)reserved;
    if (!path) return NULL;

    for (int i = 0; i < s_font_count; i++) {
        if (strcmp(s_fonts[i].path, path) == 0) return &s_fonts[i];
    }
    if (s_font_count >= MAX_FONTS) return NULL;

    char *xml = read_file(path, NULL);
    if (!xml) {
        g_pd->system->logToConsole("font_load_bmfont: cannot read %s", path);
        return NULL;
    }

    bmfont_t *font = &s_fonts[s_font_count];
    memset(font, 0, sizeof(*font));
    strncpy(font->path, path, sizeof(font->path) - 1);

    char page_file[64] = "";
    char *line = xml;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) *next++ = '\0';

        if (strstr(line, "<common ")) {
            font->line_height = attr_int(line, "lineHeight", 12);
        } else if (strstr(line, "<page ")) {
            attr_str(line, "file", page_file, sizeof(page_file));
        } else if (strstr(line, "<char ")) {
            int id = attr_int(line, "id", -1);
            if (id >= 0 && id < 256) {
                glyph_t *g = &font->glyphs[id];
                g->x = (int16_t)attr_int(line, "x", 0);
                g->y = (int16_t)attr_int(line, "y", 0);
                g->w = (int16_t)attr_int(line, "width", 0);
                g->h = (int16_t)attr_int(line, "height", 0);
                g->xoff = (int16_t)attr_int(line, "xoffset", 0);
                g->yoff = (int16_t)attr_int(line, "yoffset", 0);
                g->xadv = (int16_t)attr_int(line, "xadvance", 0);
                g->valid = true;
            }
        }
        line = next;
    }
    free(xml);

    /* Page lives next to the .bmfnt; strip ".png" for pdc's compiled image */
    char page_path[192];
    strncpy(page_path, path, sizeof(page_path) - 1);
    page_path[sizeof(page_path) - 1] = '\0';
    char *slash = strrchr(page_path, '/');
    size_t dir_len = slash ? (size_t)(slash - page_path + 1) : 0;
    page_path[dir_len] = '\0';
    strncat(page_path, page_file, sizeof(page_path) - dir_len - 1);
    char *dot = strrchr(page_path, '.');
    if (dot && dot > page_path + dir_len) *dot = '\0';

    const char *err = NULL;
    font->page = g_pd->graphics->loadBitmap(page_path, &err);
    if (!font->page) {
        g_pd->system->logToConsole("font_load_bmfont: page %s failed: %s", page_path, err ? err : "?");
        return NULL;
    }

    s_font_count++;
    return font;
}

int font_bmfont_get_width(bmfont_t *font, const char *text)
{
    if (!font || !text) return 0;
    int w = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        const glyph_t *g = &font->glyphs[*p];
        if (g->valid) w += g->xadv;
    }
    return w;
}

void font_draw_bmfont(bmfont_t *font, vec2i_t pos, const char *text, font_align_t align, rgba_t color)
{
    if (!font || !text || color.a == 0) return;

    int x = pos.x;
    if (align == FONT_ALIGN_CENTER) x -= font_bmfont_get_width(font, text) / 2;
    else if (align == FONT_ALIGN_RIGHT) x -= font_bmfont_get_width(font, text);

    g_pd->graphics->setDrawMode(render_color_is_light(color) ? kDrawModeFillWhite : kDrawModeFillBlack);

    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        const glyph_t *g = &font->glyphs[*p];
        if (!g->valid) continue;
        if (g->w > 0 && g->h > 0 && *p != ' ') {
            int gx = x + g->xoff;
            int gy = pos.y + g->yoff;
            shim_push_clip(gx, gy, g->w, g->h);
            g_pd->graphics->drawBitmap(font->page, gx - g->x, gy - g->y, kBitmapUnflipped);
        }
        x += g->xadv;
    }

    shim_pop_clip();
    g_pd->graphics->setDrawMode(kDrawModeCopy);
}

int font_bmfont_get_height(bmfont_t *font)
{
    return font ? font->line_height : 0;
}
