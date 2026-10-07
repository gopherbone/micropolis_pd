/*
 * Small immediate-mode UI kit for the 1-bit Playdate interface:
 * fonts, icons, panels, button hints, bars, text wrapping, sounds.
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "types.h"
#include "image.h"
#include "font.h"
#include "ui_icons.h"
#include "ui_glyphs.h"

#define SCREEN_W 400
#define SCREEN_H 240

typedef enum {
    UI_FONT_BOLD = 0,  /* Nontendo Bold, 13px line: labels, headings */
    UI_FONT_LIGHT,     /* Nontendo Light, 13px line: body copy */
    UI_FONT_BIG,       /* Nontendo Bold 2x, 26px line: titles */
    UI_FONT_COUNT
} ui_font_t;

typedef enum {
    UI_BLACK = 0,
    UI_WHITE = 1
} ui_ink_t;

typedef enum {
    UI_SND_MOVE = 0,
    UI_SND_SELECT,
    UI_SND_BACK,
    UI_SND_ERROR,
    UI_SND_ZOOM
} ui_sound_t;

bool ui_init(void);

/* Text */
int  ui_text(ui_font_t f, int x, int y, const char *s, font_align_t align, ui_ink_t ink);
int  ui_text_width(ui_font_t f, const char *s);
int  ui_line_height(ui_font_t f);
/* Word-wrapped text; returns total height used. max_lines <= 0 = unlimited */
int  ui_text_wrap(ui_font_t f, int x, int y, int w, const char *s, ui_ink_t ink, int max_lines);

/* Icons (16x16) */
void ui_icon(int icon, int x, int y, bool inverted);

/* Tiles from the city tileset (16px or 8px variants) */
void ui_tile16(int tile, int x, int y);
void ui_tile8(int tile, int x, int y);
image_t *ui_tiles16(void);
image_t *ui_tiles8(void);

/* Shapes */
void ui_fill(int x, int y, int w, int h, ui_ink_t ink);
void ui_rect(int x, int y, int w, int h, ui_ink_t ink);
void ui_hline(int x, int y, int w, ui_ink_t ink);
void ui_vline(int x, int y, int h, ui_ink_t ink);
void ui_gray(int x, int y, int w, int h, int level); /* level 0 (black) .. 16 (white) */
void ui_dim(int x, int y, int w, int h);             /* 50% black stipple over content */
void ui_invert(int x, int y, int w, int h);
/* White card with 2px black border and rounded corners */
void ui_panel(int x, int y, int w, int h);
/* Black card with white border (used for HUD chips / toasts) */
void ui_panel_dark(int x, int y, int w, int h);
/* Animated dashed outline visible on any background */
void ui_marching_rect(int x, int y, int w, int h, int phase);
/* Horizontal bar (outline + fill fraction 0..1) */
void ui_bar(int x, int y, int w, int h, float frac, ui_ink_t ink);

/* Native Playdate button glyph (GLYPH_*), drawn so it centres on a text line
 * whose top is y. Returns its advance width. */
int  ui_glyph(int glyph, int x, int y, ui_ink_t ink);

/* Button hints: "Ⓐ Label  Ⓑ Label" right-aligned at (right, y). Pass NULL to skip. */
void ui_hints(int right, int y, const char *a_label, const char *b_label, ui_ink_t ink);
/* Single hint: glyph + label; returns x after drawing */
int  ui_hint(int x, int y, int glyph, const char *label, ui_ink_t ink);
/* Two glyphs + label, e.g. left/right "Adjust" */
int  ui_hint2(int x, int y, int glyph1, int glyph2, const char *label, ui_ink_t ink);

/* Formatting */
void ui_fmt_money(char *out, int n, long value);  /* "$12,345" / "-$1,200" */
void ui_fmt_int(char *out, int n, long value);    /* "12,345" */

/* Sound */
void ui_sound(ui_sound_t s);

#endif
