/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "render.h"
#include "sound.h"
#include "sim.h"


static bmfont_t *s_fonts[UI_FONT_COUNT];
static image_t *s_icons = NULL;
static image_t *s_tiles16 = NULL;
static image_t *s_tiles8 = NULL;
static image_t *s_glyphs = NULL;

bool ui_init(void)
{
    if (!s_fonts[UI_FONT_BOLD]) s_fonts[UI_FONT_BOLD] = font_load_bmfont("assets/fonts/nontendo_bold.bmfnt", NULL);
    if (!s_fonts[UI_FONT_LIGHT]) s_fonts[UI_FONT_LIGHT] = font_load_bmfont("assets/fonts/nontendo_light.bmfnt", NULL);
    if (!s_fonts[UI_FONT_BIG]) s_fonts[UI_FONT_BIG] = font_load_bmfont("assets/fonts/nontendo_bold_2x.bmfnt", NULL);
    if (!s_icons) s_icons = image_load("assets/gfx/icons.png");
    if (!s_tiles16) s_tiles16 = image_load("assets/gfx/tiles_16.png");
    if (!s_tiles8) s_tiles8 = image_load("assets/gfx/tiles_8.png");
    if (!s_glyphs) s_glyphs = image_load("assets/gfx/glyphs.png");
    return s_fonts[UI_FONT_BOLD] && s_icons && s_tiles16;
}

static rgba_t ink_color(ui_ink_t ink)
{
    return ink == UI_WHITE ? rgba_white() : rgba_black();
}

int ui_text(ui_font_t f, int x, int y, const char *s, font_align_t align, ui_ink_t ink)
{
    if (!s || !s_fonts[f]) return 0;
    font_draw_bmfont(s_fonts[f], vec2i(x, y), s, align, ink_color(ink));
    return font_bmfont_get_width(s_fonts[f], s);
}

int ui_text_width(ui_font_t f, const char *s)
{
    return s_fonts[f] ? font_bmfont_get_width(s_fonts[f], s) : 0;
}

int ui_line_height(ui_font_t f)
{
    return s_fonts[f] ? font_bmfont_get_height(s_fonts[f]) : 13;
}

int ui_text_wrap(ui_font_t f, int x, int y, int w, const char *s, ui_ink_t ink, int max_lines)
{
    if (!s) return 0;
    int lh = ui_line_height(f);
    int lines = 0;
    char line[160];
    const char *p = s;

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        /* Greedily take words while they fit */
        int len = 0, last_fit = 0;
        const char *q = p;
        while (*q && *q != '\n') {
            const char *word_end = q;
            while (*word_end && *word_end != ' ' && *word_end != '\n') word_end++;
            int new_len = (int)(word_end - p);
            if (new_len >= (int)sizeof(line)) break;
            memcpy(line, p, (size_t)new_len);
            line[new_len] = '\0';
            if (ui_text_width(f, line) > w && last_fit > 0) break;
            last_fit = new_len;
            len = new_len;
            q = word_end;
            while (*q == ' ') q++;
            if (ui_text_width(f, line) > w) break; /* single overlong word */
        }
        if (len == 0) len = (int)strcspn(p, "\n");
        memcpy(line, p, (size_t)len);
        line[len] = '\0';

        if (max_lines > 0 && lines == max_lines - 1 && p[len] && p[len] != '\n') {
            /* Last allowed line: ellipsize */
            while (len > 0 && ui_text_width(f, line) + ui_text_width(f, "...") > w) line[--len] = '\0';
            strcat(line, "...");
            ui_text(f, x, y + lines * lh, line, FONT_ALIGN_LEFT, ink);
            lines++;
            break;
        }
        ui_text(f, x, y + lines * lh, line, FONT_ALIGN_LEFT, ink);
        lines++;
        p += len;
        if (*p == '\n') p++;
        if (max_lines > 0 && lines >= max_lines) break;
    }
    return lines * lh;
}

void ui_icon(int icon, int x, int y, bool inverted)
{
    if (!s_icons || icon < 0) return;
    image_draw_tile(s_icons, icon + (inverted ? ICON_INVERT_OFFSET : 0), vec2i(16, 16),
                    vec2i(x, y), false, false, rgba_white());
}

void ui_tile16(int tile, int x, int y)
{
    image_draw_tile(s_tiles16, tile, vec2i(16, 16), vec2i(x, y), false, false, rgba_white());
}

void ui_tile8(int tile, int x, int y)
{
    image_draw_tile(s_tiles8, tile, vec2i(8, 8), vec2i(x, y), false, false, rgba_white());
}

image_t *ui_tiles16(void) { return s_tiles16; }
image_t *ui_tiles8(void) { return s_tiles8; }

void ui_fill(int x, int y, int w, int h, ui_ink_t ink)
{
    render_fill_rect(vec2i(x, y), vec2i(w, h), ink_color(ink));
}

void ui_rect(int x, int y, int w, int h, ui_ink_t ink)
{
    render_draw_rect(vec2i(x, y), vec2i(w, h), ink_color(ink));
}

void ui_hline(int x, int y, int w, ui_ink_t ink)
{
    if (w > 0) render_fill_rect(vec2i(x, y), vec2i(w, 1), ink_color(ink));
}

void ui_vline(int x, int y, int h, ui_ink_t ink)
{
    if (h > 0) render_fill_rect(vec2i(x, y), vec2i(1, h), ink_color(ink));
}

void ui_gray(int x, int y, int w, int h, int level)
{
    if (level <= 0) {
        ui_fill(x, y, w, h, UI_BLACK);
    } else if (level >= 16) {
        ui_fill(x, y, w, h, UI_WHITE);
    } else {
        /* The renderer maps opaque luminance to 17 Bayer levels */
        uint8_t l = (uint8_t)(level * 16);
        render_fill_rect(vec2i(x, y), vec2i(w, h), rgba(l, l, l, 255));
    }
}

void ui_dim(int x, int y, int w, int h)
{
    /* Translucent fills become black stipple at ~3/4 alpha density */
    render_fill_rect(vec2i(x, y), vec2i(w, h), rgba(0, 0, 0, 170));
}

void ui_invert(int x, int y, int w, int h)
{
    render_invert_rect(vec2i(x, y), vec2i(w, h));
}

static void panel_shape(int x, int y, int w, int h, ui_ink_t fill, ui_ink_t border)
{
    /* Body */
    ui_fill(x + 2, y, w - 4, h, border);
    ui_fill(x, y + 2, w, h - 4, border);
    ui_fill(x + 1, y + 1, w - 2, h - 2, border);
    /* Inner fill */
    ui_fill(x + 3, y + 2, w - 6, h - 4, fill);
    ui_fill(x + 2, y + 3, w - 4, h - 6, fill);
}

void ui_panel(int x, int y, int w, int h)
{
    panel_shape(x, y, w, h, UI_WHITE, UI_BLACK);
}

void ui_panel_dark(int x, int y, int w, int h)
{
    panel_shape(x, y, w, h, UI_BLACK, UI_WHITE);
    ui_fill(x + 3, y + 2, w - 6, h - 4, UI_BLACK);
    ui_fill(x + 2, y + 3, w - 4, h - 6, UI_BLACK);
}

static void ant(int x, int y, int i, int phase)
{
    bool white = ((i + phase) / 3) & 1;
    render_draw_point(vec2i(x, y), white ? rgba_white() : rgba_black());
}

void ui_marching_rect(int x, int y, int w, int h, int phase)
{
    if (w < 2 || h < 2) return;
    int i = 0;
    /* Two pixels thick so it reads on both busy and flat tiles */
    for (int t = 0; t < 2; t++) {
        int x0 = x + t, y0 = y + t, x1 = x + w - 1 - t, y1 = y + h - 1 - t;
        i = 0;
        for (int px = x0; px <= x1; px++) ant(px, y0, i++, phase);
        for (int py = y0 + 1; py <= y1; py++) ant(x1, py, i++, phase);
        for (int px = x1 - 1; px >= x0; px--) ant(px, y1, i++, phase);
        for (int py = y1 - 1; py > y0; py--) ant(x0, py, i++, phase);
    }
}

void ui_bar(int x, int y, int w, int h, float frac, ui_ink_t ink)
{
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    ui_rect(x, y, w, h, ink);
    int fw = (int)((w - 4) * frac + 0.5f);
    if (fw > 0) ui_fill(x + 2, y + 2, fw, h - 4, ink);
}

int ui_glyph(int glyph, int x, int y, ui_ink_t ink)
{
    if (!s_glyphs || glyph < 0 || glyph >= GLYPH_COUNT) return 0;
    /* Glyph sheet: row 0 black ink (for light backgrounds), row 1 inverted */
    int tile = glyph + (ink == UI_WHITE ? GLYPH_COUNT : 0);
    image_draw_tile(s_glyphs, tile, vec2i(GLYPH_W, GLYPH_H), vec2i(x, y - 4), false, false, rgba_white());
    return k_glyph_advance[glyph];
}

int ui_hint(int x, int y, int glyph, const char *label, ui_ink_t ink)
{
    x += ui_glyph(glyph, x, y, ink) + 3;
    x += ui_text(UI_FONT_BOLD, x, y + 1, label, FONT_ALIGN_LEFT, ink);
    return x + 10;
}

int ui_hint2(int x, int y, int glyph1, int glyph2, const char *label, ui_ink_t ink)
{
    x += ui_glyph(glyph1, x, y, ink) + 1;
    x += ui_glyph(glyph2, x, y, ink) + 3;
    x += ui_text(UI_FONT_BOLD, x, y + 1, label, FONT_ALIGN_LEFT, ink);
    return x + 10;
}

static int hint_width(int glyph, const char *label)
{
    return k_glyph_advance[glyph] + 3 + ui_text_width(UI_FONT_BOLD, label) + 10;
}

void ui_hints(int right, int y, const char *a_label, const char *b_label, ui_ink_t ink)
{
    int w = 0;
    if (a_label) w += hint_width(GLYPH_A, a_label);
    if (b_label) w += hint_width(GLYPH_B, b_label);
    int x = right - w + 10;
    if (a_label) x = ui_hint(x, y, GLYPH_A, a_label, ink);
    if (b_label) ui_hint(x, y, GLYPH_B, b_label, ink);
}

void ui_fmt_int(char *out, int n, long value)
{
    char digits[32];
    bool neg = value < 0;
    unsigned long v = neg ? (unsigned long)(-value) : (unsigned long)value;
    snprintf(digits, sizeof(digits), "%lu", v);
    int len = (int)strlen(digits);
    char buf[48];
    int o = 0;
    if (neg) buf[o++] = '-';
    for (int i = 0; i < len; i++) {
        buf[o++] = digits[i];
        int rem = len - 1 - i;
        if (rem > 0 && rem % 3 == 0) buf[o++] = ',';
    }
    buf[o] = '\0';
    snprintf(out, (size_t)n, "%s", buf);
}

void ui_fmt_money(char *out, int n, long value)
{
    char num[40];
    ui_fmt_int(num, sizeof(num), value < 0 ? -value : value);
    snprintf(out, (size_t)n, "%s$%s", value < 0 ? "-" : "", num);
}

void ui_sound(ui_sound_t s)
{
    if (!UserSoundOn) return;
    static const char *paths[] = {
        "assets/sounds/ui/move.wav",
        "assets/sounds/ui/select.wav",
        "assets/sounds/ui/back.wav",
        "assets/sounds/ui/error.wav",
        "assets/sounds/ui/zoom.wav",
    };
    static const float vols[] = { 0.5f, 0.7f, 0.7f, 0.8f, 0.6f };
    sound_play_sound(paths[s], vols[s]);
}
