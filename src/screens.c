/*
 * In-game screens: build sheet, budget, city report, city map, menus,
 * inspect card and event notices.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "game_internal.h"
#include "render.h"
#include "ui.h"

#include "sim.h"

static void close_to_play(void)
{
    G.mode = MODE_PLAY;
}

/* Common full-screen page frame: black header with title, white body */
static void page_frame(const char *title, const char *right)
{
    ui_fill(0, 0, SCREEN_W, SCREEN_H, UI_WHITE);
    ui_fill(0, 0, SCREEN_W, 30, UI_BLACK);
    ui_text(UI_FONT_BIG, 10, 2, title, FONT_ALIGN_LEFT, UI_WHITE);
    if (right) ui_text(UI_FONT_BOLD, SCREEN_W - 10, 9, right, FONT_ALIGN_RIGHT, UI_WHITE);
}

static void page_footer(const char *left_hint, const char *a_label, const char *b_label)
{
    ui_hline(8, SCREEN_H - 22, SCREEN_W - 16, UI_BLACK);
    if (left_hint) ui_text(UI_FONT_LIGHT, 10, SCREEN_H - 16, left_hint, FONT_ALIGN_LEFT, UI_BLACK);
    ui_hints(SCREEN_W - 8, SCREEN_H - 17, a_label, b_label, UI_BLACK);
}

/* ======================================================================== */
/* Build sheet                                                               */

#define BUILD_COLS 4
#define BUILD_ROWS 5

typedef enum { CITY_BUDGET = 0, CITY_REPORT, CITY_MAP, CITY_GAME } city_item_t;

static const struct {
    const char *name;
    const char *desc;
    int icon;
} s_city_items[4] = {
    { "Budget", "Set taxes and fund roads, police and fire.", ICON_BUDGET },
    { "City Report", "Population, score, public opinion and history graphs.", ICON_REPORT },
    { "City Map", "See the whole city: power, traffic, pollution, crime and more.", ICON_MAP },
    { "Game", "Speed, save, load, disasters and options.", ICON_GAME },
};

static const int s_build_grid[4][BUILD_COLS] = {
    { TOOL_DOZER,  TOOL_ROAD, TOOL_RAIL,    TOOL_WIRE },
    { TOOL_RES,    TOOL_COM,  TOOL_IND,     TOOL_PARK },
    { TOOL_POLICE, TOOL_FIRE, TOOL_STADIUM, TOOL_QUERY },
    { TOOL_COAL,   TOOL_NUCLEAR, TOOL_SEAPORT, TOOL_AIRPORT },
};

static int s_bsel = 0; /* 0..19 */

void build_open(void)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < BUILD_COLS; c++)
            if (s_build_grid[r][c] == G.tool) s_bsel = r * BUILD_COLS + c;
    G.mode = MODE_BUILD;
    ui_sound(UI_SND_SELECT);
}

static void build_activate(void)
{
    int r = s_bsel / BUILD_COLS, c = s_bsel % BUILD_COLS;
    ui_sound(UI_SND_SELECT);
    if (r < 4) {
        game_select_tool(s_build_grid[r][c]);
        close_to_play();
        return;
    }
    switch (c) {
    case CITY_BUDGET: budget_open(); break;
    case CITY_REPORT: report_open(); break;
    case CITY_MAP: citymap_open(LAYER_CITY); break;
    case CITY_GAME: menu_open_game(); break;
    }
}

void build_update(float dt)
{
    (void)dt;
    int r = s_bsel / BUILD_COLS, c = s_bsel % BUILD_COLS;
    int old = s_bsel;
    if (input_nav(SIM_ACT_LEFT)) c = (c + BUILD_COLS - 1) % BUILD_COLS;
    if (input_nav(SIM_ACT_RIGHT)) c = (c + 1) % BUILD_COLS;
    if (input_nav(SIM_ACT_UP)) r = (r + BUILD_ROWS - 1) % BUILD_ROWS;
    if (input_nav(SIM_ACT_DOWN)) r = (r + 1) % BUILD_ROWS;
    s_bsel = r * BUILD_COLS + c;

    int steps = input_crank_steps(30.0f);
    if (steps) s_bsel = ((s_bsel + steps) % 20 + 20) % 20;
    if (s_bsel != old) ui_sound(UI_SND_MOVE);

    if (sim_action_pressed(SIM_ACT_PRIMARY)) build_activate();
    else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        close_to_play();
    }
}

static void draw_tool_preview(int tool, int bx, int by, int bw, int bh)
{
    const tool_def_t *t = &g_tools[tool];
    if (t->preview == PREVIEW_ICON) {
        ui_icon(t->icon, bx + bw / 2 - 8, by + bh / 2 - 8, false);
        return;
    }
    if (t->preview == PREVIEW_ROW) {
        int x0 = bx + bw / 2 - 24, y0 = by + bh / 2 - 8;
        if (tool == TOOL_PARK) {
            ui_tile16(37, x0, y0);
            ui_tile16(840, x0 + 16, y0);
            ui_tile16(38, x0 + 32, y0);
        } else if (tool == TOOL_DOZER) {
            ui_tile16(RUBBLE, x0, y0);
            ui_tile16(RUBBLE + 1, x0 + 16, y0);
            ui_tile16(DIRT, x0 + 32, y0);
        } else {
            for (int i = 0; i < 3; i++) ui_tile16(t->preview_tile, x0 + i * 16, y0);
        }
        return;
    }
    int n = t->size;
    int ts = n * 16 <= bh - 4 ? 16 : 8;
    int x0 = bx + (bw - n * ts) / 2, y0 = by + (bh - n * ts) / 2;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int tile = t->preview_tile + y * n + x;
            if (tool == TOOL_STADIUM) tile = STADIUMBASE + 16 + y * n + x; /* the full stadium */
            if (ts == 16) ui_tile16(tile, x0 + x * 16, y0 + y * 16);
            else ui_tile8(tile, x0 + x * 8, y0 + y * 8);
        }
}

void build_draw(void)
{
    ui_dim(0, TOP_BAR_H, SCREEN_W, SCREEN_H - TOP_BAR_H);

    const int px = 6, py = TOP_BAR_H + 4, pw = SCREEN_W - 12, ph = SCREEN_H - TOP_BAR_H - 8;
    ui_panel(px, py, pw, ph);

    /* Grid */
    const int gx = px + 8, gy = py + 8, cw = 40, ch = 34;
    for (int i = 0; i < 20; i++) {
        int r = i / BUILD_COLS, c = i % BUILD_COLS;
        int x = gx + c * cw;
        int y = gy + r * ch + (r == 4 ? 8 : 0);
        bool sel = i == s_bsel;
        int icon;
        bool active = false, broke = false;
        if (r < 4) {
            int tool = s_build_grid[r][c];
            icon = g_tools[tool].icon;
            active = tool == G.tool;
            int cost = game_tool_cost(tool);
            broke = cost > 0 && TotalFunds < cost;
        } else {
            icon = s_city_items[c].icon;
        }
        if (sel) {
            ui_fill(x + 1, y, cw - 4, ch - 2, UI_BLACK);
            ui_fill(x, y + 1, cw - 2, ch - 4, UI_BLACK);
        } else {
            ui_rect(x + 1, y, cw - 4, 1, UI_BLACK);
            ui_rect(x + 1, y + ch - 3, cw - 4, 1, UI_BLACK);
            ui_rect(x, y + 1, 1, ch - 4, UI_BLACK);
            ui_rect(x + cw - 3, y + 1, 1, ch - 4, UI_BLACK);
        }
        ui_icon(icon, x + (cw - 2) / 2 - 8, y + (ch - 2) / 2 - 8, sel);
        if (active) ui_fill(x + (cw - 2) / 2 - 4, y + ch - 7, 8, 2, sel ? UI_WHITE : UI_BLACK);
        if (broke) {
            /* "can't afford" corner mark */
            for (int k = 0; k < 6; k++) ui_hline(x + cw - 9 + k, y + 2 + k, 6 - k, sel ? UI_WHITE : UI_BLACK);
        }
    }
    ui_hline(gx, gy + 4 * ch + 3, BUILD_COLS * cw - 2, UI_BLACK);

    /* Info pane */
    const int ix = gx + BUILD_COLS * cw + 8, iw = px + pw - ix - 10;
    int r = s_bsel / BUILD_COLS, c = s_bsel % BUILD_COLS;
    int y = py + 8;
    if (r < 4) {
        int tool = s_build_grid[r][c];
        const tool_def_t *t = &g_tools[tool];
        ui_text(UI_FONT_BIG, ix, y, t->name, FONT_ALIGN_LEFT, UI_BLACK);
        y += 28;
        char line[64], cost[24];
        int cv = game_tool_cost(tool);
        if (cv > 0) ui_fmt_money(cost, sizeof(cost), cv);
        else snprintf(cost, sizeof(cost), "Free");
        if (t->size > 1) snprintf(line, sizeof(line), "%s  -  %dx%d tiles", cost, t->size, t->size);
        else if (tool == TOOL_QUERY || tool == TOOL_DOZER) snprintf(line, sizeof(line), "%s", cost);
        else snprintf(line, sizeof(line), "%s per tile", cost);
        ui_text(UI_FONT_BOLD, ix, y, line, FONT_ALIGN_LEFT, UI_BLACK);
        if (cv > 0 && TotalFunds < cv) ui_text(UI_FONT_BOLD, ix + iw, y, "Can't afford", FONT_ALIGN_RIGHT, UI_BLACK);
        y += 18;

        int bh = 70;
        ui_gray(ix, y, iw, bh, 14);
        ui_rect(ix, y, iw, bh, UI_BLACK);
        draw_tool_preview(tool, ix + 1, y + 1, iw - 2, bh - 2);
        y += bh + 8;
        ui_text_wrap(UI_FONT_LIGHT, ix, y, iw, t->desc, UI_BLACK, 4);
    } else {
        ui_text(UI_FONT_BIG, ix, y, s_city_items[c].name, FONT_ALIGN_LEFT, UI_BLACK);
        y += 30;
        ui_text_wrap(UI_FONT_LIGHT, ix, y, iw, s_city_items[c].desc, UI_BLACK, 3);
        y += 46;
        char a[48], b[48];
        switch (c) {
        case CITY_BUDGET:
            ui_fmt_money(b, sizeof(b), TotalFunds);
            snprintf(a, sizeof(a), "Treasury: %s", b);
            ui_text(UI_FONT_BOLD, ix, y, a, FONT_ALIGN_LEFT, UI_BLACK);
            snprintf(a, sizeof(a), "Tax rate: %d%%", CityTax);
            ui_text(UI_FONT_BOLD, ix, y + 16, a, FONT_ALIGN_LEFT, UI_BLACK);
            break;
        case CITY_REPORT:
            snprintf(a, sizeof(a), "Score: %d", CityScore);
            ui_text(UI_FONT_BOLD, ix, y, a, FONT_ALIGN_LEFT, UI_BLACK);
            snprintf(a, sizeof(a), "Approval: %d%%", CityYes);
            ui_text(UI_FONT_BOLD, ix, y + 16, a, FONT_ALIGN_LEFT, UI_BLACK);
            break;
        case CITY_MAP:
            ui_text(UI_FONT_BOLD, ix, y, "9 map layers", FONT_ALIGN_LEFT, UI_BLACK);
            ui_text(UI_FONT_LIGHT, ix, y + 16, "Jump anywhere in the city", FONT_ALIGN_LEFT, UI_BLACK);
            break;
        case CITY_GAME: {
            static const char *spd[] = { "Paused", "Slow", "Normal", "Fast" };
            snprintf(a, sizeof(a), "Speed: %s", spd[SimSpeed & 3]);
            ui_text(UI_FONT_BOLD, ix, y, a, FONT_ALIGN_LEFT, UI_BLACK);
            snprintf(a, sizeof(a), "City: %s", CityName ? CityName : "");
            ui_text(UI_FONT_BOLD, ix, y + 16, a, FONT_ALIGN_LEFT, UI_BLACK);
            break;
        }
        }
    }

    ui_hint(ix, py + ph - 20, GLYPH_CRANK, "Scroll", UI_BLACK);
    ui_hints(px + pw - 6, py + ph - 20, r < 4 ? "Use" : "Open", "Close", UI_BLACK);
}

/* ======================================================================== */
/* Budget                                                                    */

static int s_budget_sel = 0;
static const float s_flevels[3] = { 1.4f, 1.2f, 0.8f };

void budget_open(void)
{
    s_budget_sel = 0;
    G.mode = MODE_BUDGET;
    ui_sound(UI_SND_SELECT);
}

static long tax_income_estimate(void)
{
    int lvl = GameLevel;
    if (lvl < 0 || lvl > 2) lvl = 0;
    return (long)((((long)TotalPop * LVAverage) / 120) * CityTax * s_flevels[lvl]);
}

static float *fund_ptr(int i)
{
    return i == 1 ? &roadPercent : (i == 2 ? &policePercent : &firePercent);
}

static long fund_request(int i)
{
    return i == 1 ? RoadFund : (i == 2 ? PoliceFund : FireFund);
}

static void budget_adjust(int delta)
{
    if (delta == 0) return;
    if (s_budget_sel == 0) {
        int t = CityTax + delta;
        if (t < 0) t = 0;
        if (t > 20) t = 20;
        if (t == CityTax) return;
        CityTax = (short)t;
    } else {
        float *p = fund_ptr(s_budget_sel);
        int pct = (int)(*p * 100.0f + 0.5f) + delta * 5;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        if (pct == (int)(*p * 100.0f + 0.5f)) return;
        *p = pct / 100.0f;
    }
    ui_sound(UI_SND_MOVE);
}

void budget_update(float dt)
{
    (void)dt;
    if (input_nav(SIM_ACT_UP)) {
        s_budget_sel = (s_budget_sel + 3) % 4;
        ui_sound(UI_SND_MOVE);
    }
    if (input_nav(SIM_ACT_DOWN)) {
        s_budget_sel = (s_budget_sel + 1) % 4;
        ui_sound(UI_SND_MOVE);
    }
    if (input_nav(SIM_ACT_LEFT)) budget_adjust(-1);
    if (input_nav(SIM_ACT_RIGHT)) budget_adjust(1);
    budget_adjust(input_crank_steps(20.0f));

    if (sim_action_pressed(SIM_ACT_PRIMARY) || sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        close_to_play();
    }
}

void budget_draw(void)
{
    char buf[64], buf2[64];
    ui_fmt_money(buf, sizeof(buf), TotalFunds);
    snprintf(buf2, sizeof(buf2), "Treasury %s", buf);
    page_frame("Budget", buf2);

    static const char *labels[4] = { "Property Tax", "Transportation", "Police", "Fire" };
    long income = tax_income_estimate();
    long spend_total = 0;
    for (int i = 1; i < 4; i++) spend_total += (long)(fund_request(i) * *fund_ptr(i));

    int y = 38;
    for (int i = 0; i < 4; i++) {
        int rh = 36;
        int ry = y + i * rh;
        ui_text(UI_FONT_BOLD, 16, ry + 2, labels[i], FONT_ALIGN_LEFT, UI_BLACK);
        float frac;
        if (i == 0) {
            snprintf(buf, sizeof(buf), "%d%%", CityTax);
            frac = CityTax / 20.0f;
            ui_fmt_money(buf2, sizeof(buf2), income);
            char line[64];
            snprintf(line, sizeof(line), "Brings in about %s a year", buf2);
            ui_text(UI_FONT_LIGHT, 16, ry + 18, line, FONT_ALIGN_LEFT, UI_BLACK);
        } else {
            float p = *fund_ptr(i);
            snprintf(buf, sizeof(buf), "%d%%", (int)(p * 100.0f + 0.5f));
            frac = p;
            long req = fund_request(i);
            char a[24], b[24], line[64];
            ui_fmt_money(a, sizeof(a), (long)(req * p));
            ui_fmt_money(b, sizeof(b), req);
            snprintf(line, sizeof(line), "Spends %s of %s requested", a, b);
            ui_text(UI_FONT_LIGHT, 16, ry + 18, line, FONT_ALIGN_LEFT, UI_BLACK);
        }
        /* Slider */
        int sx = 236, sw = 120;
        ui_text(UI_FONT_BOLD, SCREEN_W - 14, ry + 2, buf, FONT_ALIGN_RIGHT, UI_BLACK);
        ui_bar(sx, ry + 17, sw, 10, frac, UI_BLACK);
        if (i == 0) {
            /* Ticks every 5% so the usual 7% is easy to find */
            for (int k = 1; k < 4; k++) ui_vline(sx + k * sw / 4, ry + 28, 3, UI_BLACK);
        }
        if (i == s_budget_sel) {
            ui_icon(ICON_ARROW_L, sx - 18, ry + 14, false);
            ui_icon(ICON_ARROW_R, sx + sw + 2, ry + 14, false);
            ui_invert(8, ry, 210, rh - 4);
        }
        ui_hline(8, ry + rh - 2, SCREEN_W - 16, UI_BLACK);
    }

    /* Summary */
    long flow = income - spend_total;
    /* Band between the last row's divider and the footer rule: centre the big
     * figure in it and sit the label on the same baseline */
    int sy = y + 4 * 36 + 7;
    ui_text(UI_FONT_BOLD, 16, sy + 9, "Yearly cash flow", FONT_ALIGN_LEFT, UI_BLACK);
    ui_fmt_money(buf, sizeof(buf), flow);
    if (flow > 0) {
        snprintf(buf2, sizeof(buf2), "+%s", buf);
        ui_text(UI_FONT_BIG, SCREEN_W - 14, sy, buf2, FONT_ALIGN_RIGHT, UI_BLACK);
    } else {
        ui_text(UI_FONT_BIG, SCREEN_W - 14, sy, buf, FONT_ALIGN_RIGHT, UI_BLACK);
    }

    page_footer(NULL, "Done", NULL);
    int hx = ui_hint2(10, SCREEN_H - 17, GLYPH_UP, GLYPH_DOWN, "Select", UI_BLACK);
    hx = ui_hint2(hx, SCREEN_H - 17, GLYPH_LEFT, GLYPH_RIGHT, "Adjust", UI_BLACK);
    ui_hint(hx, SCREEN_H - 17, GLYPH_CRANK, "Adjust", UI_BLACK);
}

/* ======================================================================== */
/* City report                                                               */

static int s_report_tab = 0;
static bool s_report_long = false;
#define REPORT_TABS 4
#define TAB_HISTORY 3

void report_open(void)
{
    CityEvaluation();
    s_report_tab = 0;
    G.mode = MODE_REPORT;
    ui_sound(UI_SND_SELECT);
}

void report_update(float dt)
{
    (void)dt;
    int old = s_report_tab;
    if (input_nav(SIM_ACT_LEFT)) s_report_tab = (s_report_tab + REPORT_TABS - 1) % REPORT_TABS;
    if (input_nav(SIM_ACT_RIGHT)) s_report_tab = (s_report_tab + 1) % REPORT_TABS;
    int steps = input_crank_steps(45.0f);
    if (steps) s_report_tab = ((s_report_tab + steps) % REPORT_TABS + REPORT_TABS) % REPORT_TABS;
    if (old != s_report_tab) ui_sound(UI_SND_MOVE);

    if (s_report_tab == TAB_HISTORY && (input_nav(SIM_ACT_UP) || input_nav(SIM_ACT_DOWN))) {
        s_report_long = !s_report_long;
        ui_sound(UI_SND_MOVE);
    }
    if (sim_action_pressed(SIM_ACT_PRIMARY) || sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        close_to_play();
    }
}

static const char *s_class_names[] = { "Village", "Town", "City", "Capital", "Metropolis", "Megalopolis" };
static const char *s_problem_names[] = {
    "Crime", "Pollution", "Housing costs", "Taxes", "Traffic", "Unemployment", "Fires"
};

static void stat_card(int x, int y, int w, const char *label, const char *value, const char *note)
{
    ui_text(UI_FONT_LIGHT, x, y, label, FONT_ALIGN_LEFT, UI_BLACK);
    ui_text(UI_FONT_BIG, x, y + 17, value, FONT_ALIGN_LEFT, UI_BLACK);
    if (note) ui_text(UI_FONT_BOLD, x + w, y + 25, note, FONT_ALIGN_RIGHT, UI_BLACK);
}

static void report_overview(void)
{
    char v[48], n[48];
    int lx = 16, rx = 208, cw = 176;

    ui_fmt_int(v, sizeof(v), CityPop);
    snprintf(n, sizeof(n), "%s%ld", deltaCityPop >= 0 ? "+" : "", (long)deltaCityPop);
    stat_card(lx, 40, cw, "Population", v, n);

    snprintf(v, sizeof(v), "%d", CityScore);
    snprintf(n, sizeof(n), "%s%d", deltaCityScore >= 0 ? "+" : "", deltaCityScore);
    stat_card(rx, 40, cw, "City score (0-1000)", v, n);

    int cls = (CityClass >= 0 && CityClass < 6) ? CityClass : 0;
    stat_card(lx, 92, cw, "Category", s_class_names[cls], NULL);

    ui_fmt_money(v, sizeof(v), CityAssValue);
    stat_card(rx, 92, cw, "Assessed value", v, NULL);

    ui_fmt_money(v, sizeof(v), TotalFunds);
    stat_card(lx, 144, cw, "Treasury", v, NULL);

    static const char *levels[] = { "Easy", "Medium", "Hard" };
    stat_card(rx, 144, cw, "Difficulty", levels[(GameLevel >= 0 && GameLevel < 3) ? GameLevel : 0], NULL);

    ui_vline(198, 40, 150, UI_BLACK);
}

static void report_opinion(void)
{
    char buf[48];
    ui_text(UI_FONT_BOLD, 16, 40, "Is the mayor doing a good job?", FONT_ALIGN_LEFT, UI_BLACK);
    snprintf(buf, sizeof(buf), "Yes %d%%", CityYes);
    ui_text(UI_FONT_BOLD, 16, 60, buf, FONT_ALIGN_LEFT, UI_BLACK);
    ui_bar(90, 59, 290, 12, CityYes / 100.0f, UI_BLACK);
    snprintf(buf, sizeof(buf), "No %d%%", CityNo);
    ui_text(UI_FONT_BOLD, 16, 78, buf, FONT_ALIGN_LEFT, UI_BLACK);
    ui_rect(90, 77, 290, 12, UI_BLACK);
    int nw = (int)(286 * CityNo / 100.0f);
    if (nw > 0) ui_gray(92, 79, nw, 8, 8);

    ui_text(UI_FONT_BOLD, 16, 104, "What residents worry about most", FONT_ALIGN_LEFT, UI_BLACK);
    int shown = 0;
    for (int i = 0; i < 4; i++) {
        int pid = ProblemOrder[i];
        int votes = ProblemVotes[pid];
        if (pid < 0 || pid >= 7 || votes <= 0) continue;
        int y = 124 + shown * 20;
        ui_text(UI_FONT_LIGHT, 16, y, s_problem_names[pid], FONT_ALIGN_LEFT, UI_BLACK);
        snprintf(buf, sizeof(buf), "%d%%", votes);
        ui_text(UI_FONT_BOLD, 384, y, buf, FONT_ALIGN_RIGHT, UI_BLACK);
        ui_bar(130, y - 1, 210, 12, votes / 100.0f, UI_BLACK);
        shown++;
    }
    if (!shown) ui_text(UI_FONT_LIGHT, 16, 124, "Nothing yet - the city is too small to complain.", FONT_ALIGN_LEFT, UI_BLACK);
}

/* style 0 solid 2px, 1 solid 1px, 2 dotted */
static void plot_series(short *his, int off, int x, int y, int w, int h, int vmax, int style, bool centered)
{
    if (!his) return;
    int n = 120;
    int px = -1, py = -1;
    for (int i = n - 1; i >= 0; i--) {
        int v = his[off + i];
        if (centered) v = v; /* already 0..255 */
        int sx = x + (n - 1 - i) * (w - 1) / (n - 1);
        int sy = y + h - 1 - (vmax > 0 ? v * (h - 1) / vmax : 0);
        if (sy < y) sy = y;
        if (sy > y + h - 1) sy = y + h - 1;
        if (px >= 0) {
            if (style == 2) {
                if ((i & 1) == 0) render_draw_line(vec2i(px, py), vec2i(sx, sy), rgba_black());
            } else {
                render_draw_line(vec2i(px, py), vec2i(sx, sy), rgba_black());
                if (style == 0) render_draw_line(vec2i(px, py - 1), vec2i(sx, sy - 1), rgba_black());
            }
        }
        px = sx;
        py = sy;
    }
}

static void legend(int x, int y, int style, const char *label)
{
    if (style == 0) ui_fill(x, y + 5, 14, 2, UI_BLACK);
    else if (style == 1) ui_fill(x, y + 6, 14, 1, UI_BLACK);
    else for (int i = 0; i < 14; i += 3) ui_fill(x + i, y + 6, 1, 1, UI_BLACK);
    ui_text(UI_FONT_LIGHT, x + 18, y + 1, label, FONT_ALIGN_LEFT, UI_BLACK);
}

static void report_history(void)
{
    int off = s_report_long ? 120 : 0;
    const int cx = 16, cw = 368, ch = 62;

    /* Growth chart: R C I on a shared scale */
    int gy = 52;
    int vmax = 1;
    for (int i = 0; i < 120; i++) {
        if (ResHis && ResHis[off + i] > vmax) vmax = ResHis[off + i];
        if (ComHis && ComHis[off + i] > vmax) vmax = ComHis[off + i];
        if (IndHis && IndHis[off + i] > vmax) vmax = IndHis[off + i];
    }
    ui_text(UI_FONT_BOLD, cx, 38, "Growth", FONT_ALIGN_LEFT, UI_BLACK);
    legend(cx + 70, 37, 0, "Residential");
    legend(cx + 170, 37, 1, "Commercial");
    legend(cx + 268, 37, 2, "Industrial");
    ui_rect(cx, gy, cw, ch, UI_BLACK);
    plot_series(ResHis, off, cx + 2, gy + 2, cw - 4, ch - 4, vmax, 0, false);
    plot_series(ComHis, off, cx + 2, gy + 2, cw - 4, ch - 4, vmax, 1, false);
    plot_series(IndHis, off, cx + 2, gy + 2, cw - 4, ch - 4, vmax, 2, false);

    /* City health: cash flow (centred), crime, pollution on 0..255 */
    int hy = gy + ch + 22;
    ui_text(UI_FONT_BOLD, cx, hy - 14, "Health", FONT_ALIGN_LEFT, UI_BLACK);
    legend(cx + 70, hy - 15, 0, "Cash flow");
    legend(cx + 170, hy - 15, 1, "Crime");
    legend(cx + 268, hy - 15, 2, "Pollution");
    ui_rect(cx, hy, cw, ch, UI_BLACK);
    for (int x = cx + 2; x < cx + cw - 2; x += 4) ui_fill(x, hy + ch / 2, 1, 1, UI_BLACK);
    plot_series(MoneyHis, off, cx + 2, hy + 2, cw - 4, ch - 4, 255, 0, true);
    plot_series(CrimeHis, off, cx + 2, hy + 2, cw - 4, ch - 4, 255, 1, false);
    plot_series(PollutionHis, off, cx + 2, hy + 2, cw - 4, ch - 4, 255, 2, false);

    ui_text(UI_FONT_LIGHT, cx, hy + ch + 1, s_report_long ? "120 years ago" : "10 years ago", FONT_ALIGN_LEFT, UI_BLACK);
    ui_text(UI_FONT_LIGHT, cx + cw, hy + ch + 1, "Now", FONT_ALIGN_RIGHT, UI_BLACK);
}

/* Demand: one centred bar per zone type, surplus left of zero, demand right */
static void report_demand(void)
{
    static const char *names[3] = { "Residential", "Commercial", "Industrial" };
    int vals[3] = { RValve, CValve, IValve };
    int lim[3] = { 2000, 1500, 1500 };
    ui_text(UI_FONT_BOLD, 16, 40, "Demand for new zones", FONT_ALIGN_LEFT, UI_BLACK);
    ui_text(UI_FONT_LIGHT, 150, 40, "Surplus", FONT_ALIGN_LEFT, UI_BLACK);
    ui_text(UI_FONT_LIGHT, 380, 40, "Demand", FONT_ALIGN_RIGHT, UI_BLACK);
    for (int i = 0; i < 3; i++) {
        int y = 64 + i * 40;
        float f = (float)vals[i] / lim[i];
        if (f > 1) f = 1;
        if (f < -1) f = -1;
        const char *word = f > 0.5f ? "Strong demand" : f > 0.1f ? "Some demand" : f < -0.5f ? "Oversupplied" : f < -0.1f ? "Slight surplus" : "Balanced";
        ui_text(UI_FONT_BOLD, 16, y, names[i], FONT_ALIGN_LEFT, UI_BLACK);
        ui_text(UI_FONT_LIGHT, 16, y + 15, word, FONT_ALIGN_LEFT, UI_BLACK);
        int bx = 150, bw = 230, mid = bx + bw / 2;
        ui_rect(bx, y + 2, bw, 22, UI_BLACK);
        ui_vline(mid, y, 26, UI_BLACK);
        int len = (int)(f * (bw / 2 - 3));
        if (len > 0) ui_fill(mid + 1, y + 5, len, 16, UI_BLACK);
        else if (len < 0) ui_gray(mid + len, y + 5, -len, 16, 8);
    }
}

void report_draw(void)
{
    page_frame("City Report", CityName);

    /* Tabs */
    static const char *tabs[REPORT_TABS] = { "Overview", "Demand", "Opinion", "History" };
    int tx = 220;
    (void)tx;

    switch (s_report_tab) {
    case 0: report_overview(); break;
    case 1: report_demand(); break;
    case 2: report_opinion(); break;
    case TAB_HISTORY: report_history(); break;
    }

    /* Tab strip in the footer: dots + name */
    ui_hline(8, SCREEN_H - 22, SCREEN_W - 16, UI_BLACK);
    int x = 10;
    for (int i = 0; i < REPORT_TABS; i++) {
        int w = ui_text_width(UI_FONT_BOLD, tabs[i]) + 10;
        if (i == s_report_tab) {
            ui_fill(x - 4, SCREEN_H - 19, w, 16, UI_BLACK);
            ui_text(UI_FONT_BOLD, x + 1, SCREEN_H - 16, tabs[i], FONT_ALIGN_LEFT, UI_WHITE);
        } else {
            ui_text(UI_FONT_LIGHT, x + 1, SCREEN_H - 16, tabs[i], FONT_ALIGN_LEFT, UI_BLACK);
        }
        x += w + 4;
    }
    if (s_report_tab == TAB_HISTORY) ui_hint2(x + 2, SCREEN_H - 17, GLYPH_UP, GLYPH_DOWN, s_report_long ? "10y" : "120y", UI_BLACK);
    else ui_hint2(x + 2, SCREEN_H - 17, GLYPH_LEFT, GLYPH_RIGHT, "Tab", UI_BLACK);
    ui_hints(SCREEN_W - 8, SCREEN_H - 17, NULL, "Back", UI_BLACK);
}

/* ======================================================================== */
/* City map                                                                  */

static map_layer_t s_layer = LAYER_CITY;
static bool s_map_jump = false;
static int s_jump_x, s_jump_y; /* centre tile of the frame */
static bool s_map_refresh = true;

void citymap_open(map_layer_t layer)
{
    s_layer = layer;
    s_map_jump = false;
    s_map_refresh = true;
    int z = G.zoom;
    s_jump_x = ((int)G.cam_x + SCREEN_W / 2) / z;
    s_jump_y = ((int)G.cam_y + SCREEN_H / 2) / z;
    G.mode = MODE_MAP;
    ui_sound(UI_SND_SELECT);
}

void citymap_update(float dt)
{
    (void)dt;
    if (s_map_jump) {
        int step = 2;
        if (input_nav(SIM_ACT_LEFT)) s_jump_x -= step;
        if (input_nav(SIM_ACT_RIGHT)) s_jump_x += step;
        if (input_nav(SIM_ACT_UP)) s_jump_y -= step;
        if (input_nav(SIM_ACT_DOWN)) s_jump_y += step;
        if (s_jump_x < 0) s_jump_x = 0;
        if (s_jump_y < 0) s_jump_y = 0;
        if (s_jump_x >= CITY_W) s_jump_x = CITY_W - 1;
        if (s_jump_y >= CITY_H) s_jump_y = CITY_H - 1;
        if (sim_action_pressed(SIM_ACT_PRIMARY)) {
            ui_sound(UI_SND_SELECT);
            game_center_on(s_jump_x, s_jump_y);
            close_to_play();
        } else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
            ui_sound(UI_SND_BACK);
            s_map_jump = false;
        }
        return;
    }

    int old = s_layer;
    if (input_nav(SIM_ACT_UP)) s_layer = (s_layer + LAYER_COUNT - 1) % LAYER_COUNT;
    if (input_nav(SIM_ACT_DOWN)) s_layer = (s_layer + 1) % LAYER_COUNT;
    int steps = input_crank_steps(30.0f);
    if (steps) s_layer = (map_layer_t)(((s_layer + steps) % LAYER_COUNT + LAYER_COUNT) % LAYER_COUNT);
    if ((int)s_layer != old) ui_sound(UI_SND_MOVE);

    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        ui_sound(UI_SND_SELECT);
        s_map_jump = true;
    } else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        close_to_play();
    }
}

void citymap_draw(void)
{
    ui_fill(0, 0, SCREEN_W, SCREEN_H, UI_WHITE);
    const int mx = 6, my = 22;
    ui_fill(0, 0, SCREEN_W, 18, UI_BLACK);
    ui_text(UI_FONT_BOLD, 8, 3, "City Map", FONT_ALIGN_LEFT, UI_WHITE);
    ui_text(UI_FONT_LIGHT, 74, 3, CityName ? CityName : "", FONT_ALIGN_LEFT, UI_WHITE);

    ui_rect(mx - 2, my - 2, CITY_W * 2 + 4, CITY_H * 2 + 4, UI_BLACK);
    mapview_draw_overview(mx, my, s_layer, s_map_refresh);
    s_map_refresh = false;

    /* Current (or proposed) view frame */
    int z = G.zoom;
    int vw = SCREEN_W / z, vh = SCREEN_H / z;
    int cx, cy;
    if (s_map_jump) {
        cx = s_jump_x;
        cy = s_jump_y;
    } else {
        cx = ((int)G.cam_x + SCREEN_W / 2) / z;
        cy = ((int)G.cam_y + SCREEN_H / 2) / z;
    }
    int fx = cx - vw / 2, fy = cy - vh / 2;
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > CITY_W - vw) fx = CITY_W - vw;
    if (fy > CITY_H - vh) fy = CITY_H - vh;
    ui_marching_rect(mx + fx * 2 - 1, my + fy * 2 - 1, vw * 2 + 2, vh * 2 + 2, (int)(G.time * 20.0f));

    /* Layer list */
    const int lx = mx + CITY_W * 2 + 10, lw = SCREEN_W - lx - 6;
    for (int i = 0; i < LAYER_COUNT; i++) {
        int y = my - 2 + i * 15;
        bool sel = i == (int)s_layer;
        if (sel) ui_fill(lx, y, lw, 15, UI_BLACK);
        ui_text(sel ? UI_FONT_BOLD : UI_FONT_LIGHT, lx + 5, y + 2, mapview_layer_name((map_layer_t)i),
                FONT_ALIGN_LEFT, sel ? UI_WHITE : UI_BLACK);
    }
    int ly = my + LAYER_COUNT * 15 + 4;
    ui_hline(lx, ly, lw, UI_BLACK);
    ui_text_wrap(UI_FONT_LIGHT, lx + 2, ly + 5, lw - 2, mapview_layer_legend(s_layer), UI_BLACK, 3);

    if (s_map_jump) {
        ui_fill(0, SCREEN_H - 18, SCREEN_W, 18, UI_BLACK);
        ui_hint(8, SCREEN_H - 15, GLYPH_DPAD, "Move view", UI_WHITE);
        ui_hints(SCREEN_W - 6, SCREEN_H - 15, "Go here", "Cancel", UI_WHITE);
    } else {
        ui_fill(0, SCREEN_H - 18, SCREEN_W, 18, UI_BLACK);
        int hx = 8 + ui_glyph(GLYPH_UP, 8, SCREEN_H - 15, UI_WHITE) + 1;
        hx += ui_glyph(GLYPH_DOWN, hx, SCREEN_H - 15, UI_WHITE) + 1;
        ui_hint(hx, SCREEN_H - 15, GLYPH_CRANK, "Layer", UI_WHITE);
        ui_hints(SCREEN_W - 6, SCREEN_H - 15, "Jump to...", "Back", UI_WHITE);
    }
}

/* ======================================================================== */
/* Menus                                                                     */

typedef enum {
    MENU_GAME = 0,
    MENU_DISASTERS,
    MENU_SAVE,
    MENU_LOAD,
    MENU_QUIT,
    MENU_LAYERS
} menu_kind_t;

#define MAX_ITEMS 10
typedef struct {
    char label[48];
    char value[24];
    bool enabled;
} menu_item_t;

static menu_kind_t s_menu = MENU_GAME;
static int s_menu_sel = 0;
static menu_item_t s_items[MAX_ITEMS];
static int s_item_count = 0;
static const char *s_menu_title = "";

static const char *s_speed_names[] = { "Paused", "Slow", "Normal", "Fast" };

static void add_item(const char *label, const char *value, bool enabled)
{
    if (s_item_count >= MAX_ITEMS) return;
    menu_item_t *it = &s_items[s_item_count++];
    snprintf(it->label, sizeof(it->label), "%s", label);
    snprintf(it->value, sizeof(it->value), "%s", value ? value : "");
    it->enabled = enabled;
}

static void slot_label(int slot, char *label, int n, bool *exists)
{
    city_meta_t meta;
    if (city_get_slot_meta(slot, &meta) && meta.exists) {
        snprintf(label, (size_t)n, "%d. %.16s (%d)", slot, meta.name, meta.year);
        *exists = true;
    } else {
        snprintf(label, (size_t)n, "%d. Empty", slot);
        *exists = false;
    }
}

static void build_items(void)
{
    s_item_count = 0;
    char buf[48];
    bool ex;
    switch (s_menu) {
    case MENU_GAME:
        s_menu_title = "Game";
        add_item("Speed", s_speed_names[SimSpeed & 3], true);
        add_item("Save City...", NULL, true);
        add_item("Load City...", NULL, true);
        add_item("Disasters...", NULL, true);
        add_item("Auto-Bulldoze", autoBulldoze ? "On" : "Off", true);
        add_item("Auto-Budget", autoBudget ? "On" : "Off", true);
        add_item("Sound", UserSoundOn ? "On" : "Off", true);
        add_item("Quit to Title", NULL, true);
        break;
    case MENU_DISASTERS:
        s_menu_title = "Disasters";
        add_item("Random Disasters", NoDisasters ? "Off" : "On", true);
        add_item("Start a Fire", NULL, true);
        add_item("Flood", NULL, true);
        add_item("Tornado", NULL, true);
        add_item("Earthquake", NULL, true);
        add_item("Monster", NULL, true);
        add_item("Nuclear Meltdown", NULL, true);
        break;
    case MENU_SAVE:
        s_menu_title = "Save City";
        for (int i = 1; i <= 4; i++) {
            slot_label(i, buf, sizeof(buf), &ex);
            add_item(buf, ex ? "Replace" : "Save", true);
        }
        break;
    case MENU_LOAD:
        s_menu_title = "Load City";
        for (int i = 1; i <= 4; i++) {
            slot_label(i, buf, sizeof(buf), &ex);
            add_item(buf, NULL, ex);
        }
        add_item("Example: Micropolis", NULL, true);
        break;
    case MENU_QUIT:
        s_menu_title = "Quit to title?";
        add_item("Yes, quit", NULL, true);
        add_item("Keep playing", NULL, true);
        break;
    case MENU_LAYERS:
        s_menu_title = "City Map";
        for (int i = 0; i < LAYER_COUNT; i++) add_item(mapview_layer_name((map_layer_t)i), NULL, true);
        break;
    }
    if (s_menu_sel >= s_item_count) s_menu_sel = s_item_count - 1;
    if (s_menu_sel < 0) s_menu_sel = 0;
}

static void open_menu(menu_kind_t kind, int sel)
{
    s_menu = kind;
    s_menu_sel = sel;
    G.mode = MODE_MENU;
    build_items();
}

void menu_open_game(void)
{
    open_menu(MENU_GAME, 0);
    ui_sound(UI_SND_SELECT);
}

void menu_open_layers(void)
{
    open_menu(MENU_LAYERS, 0);
}

static void menu_back(void)
{
    ui_sound(UI_SND_BACK);
    switch (s_menu) {
    case MENU_DISASTERS: open_menu(MENU_GAME, 3); break;
    case MENU_SAVE: open_menu(MENU_GAME, 1); break;
    case MENU_LOAD: open_menu(MENU_GAME, 2); break;
    case MENU_QUIT: open_menu(MENU_GAME, 7); break;
    default: close_to_play(); break;
    }
}

static void menu_change_value(int dir)
{
    if (s_menu != MENU_GAME) return;
    switch (s_menu_sel) {
    case 0: game_set_speed(((SimSpeed + dir) % 4 + 4) % 4); break;
    case 4: autoBulldoze = !autoBulldoze; break;
    case 5: autoBudget = !autoBudget; break;
    case 6: UserSoundOn = !UserSoundOn; break;
    default: return;
    }
    ui_sound(UI_SND_MOVE);
}

static void menu_activate(void)
{
    menu_item_t *it = &s_items[s_menu_sel];
    if (!it->enabled) {
        ui_sound(UI_SND_ERROR);
        return;
    }
    ui_sound(UI_SND_SELECT);
    int sel = s_menu_sel;
    switch (s_menu) {
    case MENU_GAME:
        switch (sel) {
        case 0: case 4: case 5: case 6: menu_change_value(1); break;
        case 1: open_menu(MENU_SAVE, 0); break;
        case 2: open_menu(MENU_LOAD, 0); break;
        case 3: open_menu(MENU_DISASTERS, 0); break;
        case 7: open_menu(MENU_QUIT, 1); break;
        }
        break;
    case MENU_DISASTERS:
        if (sel == 0) {
            NoDisasters = !NoDisasters;
            break;
        }
        close_to_play();
        switch (sel) {
        case 1: SetFire(); break;
        case 2: MakeFlood(); break;
        case 3: MakeTornado(); break;
        case 4: MakeEarthquake(); break;
        case 5: MakeMonster(); break;
        case 6: MakeMeltdown(); break;
        }
        break;
    case MENU_SAVE:
        game_save_slot(sel + 1);
        close_to_play();
        break;
    case MENU_LOAD:
        if (sel < 4) {
            if (!game_load_slot(sel + 1)) game_show_message("Couldn't load that city.", ICON_ALERT);
        } else {
            game_start_embedded_city("about.cty");
        }
        close_to_play();
        break;
    case MENU_QUIT:
        if (sel == 0) game_quit_to_title();
        else close_to_play();
        break;
    case MENU_LAYERS:
        citymap_open((map_layer_t)sel);
        break;
    }
    if (G.mode == MODE_MENU) build_items();
}

void menu_update(float dt)
{
    (void)dt;
    int old = s_menu_sel;
    if (input_nav(SIM_ACT_UP)) s_menu_sel = (s_menu_sel + s_item_count - 1) % s_item_count;
    if (input_nav(SIM_ACT_DOWN)) s_menu_sel = (s_menu_sel + 1) % s_item_count;
    int steps = input_crank_steps(30.0f);
    if (steps) s_menu_sel = ((s_menu_sel + steps) % s_item_count + s_item_count) % s_item_count;
    if (old != s_menu_sel) ui_sound(UI_SND_MOVE);

    if (input_nav(SIM_ACT_LEFT)) menu_change_value(-1);
    if (input_nav(SIM_ACT_RIGHT)) menu_change_value(1);

    if (sim_action_pressed(SIM_ACT_PRIMARY)) menu_activate();
    else if (sim_action_pressed(SIM_ACT_SECONDARY)) menu_back();
    if (G.mode == MODE_MENU) build_items();
}

void menu_draw(void)
{
    ui_dim(0, TOP_BAR_H, SCREEN_W, SCREEN_H - TOP_BAR_H);
    const int rh = 18;
    int w = 250;
    int h = 30 + s_item_count * rh + 26;
    int x = (SCREEN_W - w) / 2;
    int y = TOP_BAR_H + (SCREEN_H - TOP_BAR_H - h) / 2;
    ui_panel(x, y, w, h);
    ui_text(UI_FONT_BOLD, x + 12, y + 8, s_menu_title, FONT_ALIGN_LEFT, UI_BLACK);
    ui_hline(x + 6, y + 24, w - 12, UI_BLACK);

    for (int i = 0; i < s_item_count; i++) {
        menu_item_t *it = &s_items[i];
        int ry = y + 28 + i * rh;
        bool sel = i == s_menu_sel;
        ui_ink_t ink = sel ? UI_WHITE : UI_BLACK;
        if (sel) ui_fill(x + 5, ry, w - 10, rh, UI_BLACK);
        ui_text(UI_FONT_BOLD, x + 12, ry + 3, it->label, FONT_ALIGN_LEFT, ink);
        if (it->value[0]) {
            bool cyclable = s_menu == MENU_GAME && (i == 0 || (i >= 4 && i <= 6));
            int vx = x + w - 12;
            if (cyclable && sel) {
                ui_icon(ICON_ARROW_R, vx - 12, ry + 1, true);
                vx -= 14;
                int vw = ui_text_width(UI_FONT_LIGHT, it->value);
                ui_icon(ICON_ARROW_L, vx - vw - 14, ry + 1, true);
            }
            ui_text(UI_FONT_LIGHT, vx, ry + 3, it->value, FONT_ALIGN_RIGHT, ink);
        }
        if (!it->enabled) {
            /* Disabled: stipple over the row */
            for (int k = x + 10; k < x + w - 10; k += 2) ui_fill(k, ry + 9, 1, 1, ink);
        }
    }
    ui_hints(x + w - 6, y + h - 20, "Select", "Back", UI_BLACK);
}

/* ======================================================================== */
/* Inspect card                                                              */

static struct {
    char title[48], density[32], value[32], crime[32], pollution[32], growth[32];
    int x, y;
    bool powered;
} s_query;

void query_set(const char *title, const char *density, const char *value,
               const char *crime, const char *pollution, const char *growth,
               int x, int y, bool powered)
{
    snprintf(s_query.title, sizeof(s_query.title), "%s", title ? title : "");
    snprintf(s_query.density, sizeof(s_query.density), "%s", density ? density : "");
    snprintf(s_query.value, sizeof(s_query.value), "%s", value ? value : "");
    snprintf(s_query.crime, sizeof(s_query.crime), "%s", crime ? crime : "");
    snprintf(s_query.pollution, sizeof(s_query.pollution), "%s", pollution ? pollution : "");
    snprintf(s_query.growth, sizeof(s_query.growth), "%s", growth ? growth : "");
    s_query.x = x;
    s_query.y = y;
    s_query.powered = powered;
    G.mode = MODE_QUERY;
}

void query_update(float dt)
{
    (void)dt;
    if (sim_action_pressed(SIM_ACT_PRIMARY) || sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        close_to_play();
    }
}

void query_draw(void)
{
    const int w = 176, h = 150;
    /* Opposite side of the screen from the cursor */
    int cursor_sx = G.cur_x * G.zoom - (int)G.cam_x;
    int x = cursor_sx < SCREEN_W / 2 ? SCREEN_W - w - 8 : 8;
    int y = TOP_BAR_H + 8;
    ui_panel(x, y, w, h);
    ui_icon(ICON_QUERY, x + 8, y + 6, false);
    render_set_clip(vec2i(x + 4, y), vec2i(w - 8, h));
    ui_text(UI_FONT_BOLD, x + 28, y + 9, s_query.title, FONT_ALIGN_LEFT, UI_BLACK);
    render_set_clip(vec2i(0, 0), vec2i(0, 0));
    ui_hline(x + 6, y + 26, w - 12, UI_BLACK);

    const char *labels[6] = { "Density", "Value", "Crime", "Pollution", "Growth", "Power" };
    const char *vals[6] = { s_query.density, s_query.value, s_query.crime, s_query.pollution,
                            s_query.growth, s_query.powered ? "Yes" : "No" };
    for (int i = 0; i < 6; i++) {
        int ry = y + 32 + i * 15;
        ui_text(UI_FONT_LIGHT, x + 10, ry, labels[i], FONT_ALIGN_LEFT, UI_BLACK);
        ui_text(UI_FONT_BOLD, x + w - 10, ry, vals[i][0] ? vals[i] : "-", FONT_ALIGN_RIGHT, UI_BLACK);
    }
    ui_hints(x + w - 6, y + h - 20, NULL, "Close", UI_BLACK);
}

/* ======================================================================== */
/* Event notice                                                              */

static struct {
    char title[32];
    char body[128];
    int icon;
    int x, y;
} s_notice;

void notice_open(int id, const char *msg, int x, int y)
{
    const char *title = "News";
    int icon = ICON_ALERT;
    switch (id) {
    case 20: case 30: title = "Fire!"; icon = ICON_FIRE; break;
    case 21: title = "Monster!"; break;
    case 22: title = "Tornado!"; break;
    case 23: title = "Earthquake!"; break;
    case 24: title = "Plane Crash"; icon = ICON_AIRPORT; break;
    case 25: title = "Shipwreck"; icon = ICON_SEAPORT; break;
    case 26: title = "Train Crash"; icon = ICON_RAIL; break;
    case 27: title = "Copter Crash"; break;
    case 32: title = "Explosion!"; break;
    case 35: title = "Now a Town!"; icon = ICON_PEOPLE; break;
    case 36: title = "Now a City!"; icon = ICON_PEOPLE; break;
    case 37: title = "A Capital!"; icon = ICON_PEOPLE; break;
    case 38: title = "Metropolis!"; icon = ICON_PEOPLE; break;
    case 39: title = "Megalopolis!"; icon = ICON_PEOPLE; break;
    case 40: title = "Brownouts"; icon = ICON_POWER_OFF; break;
    case 42: title = "Flood!"; break;
    case 43: title = "Meltdown!"; icon = ICON_NUCLEAR; break;
    case 44: title = "Riots!"; break;
    default: break;
    }
    snprintf(s_notice.title, sizeof(s_notice.title), "%s", title);
    snprintf(s_notice.body, sizeof(s_notice.body), "%s", msg ? msg : "");
    s_notice.icon = icon;
    s_notice.x = x;
    s_notice.y = y;
    G.mode = MODE_NOTICE;
    ui_sound(UI_SND_ERROR);
}

void notice_update(float dt)
{
    (void)dt;
    bool has_loc = s_notice.x >= 0 && s_notice.y >= 0;
    if (sim_action_pressed(SIM_ACT_PRIMARY)) {
        ui_sound(UI_SND_SELECT);
        if (has_loc) game_center_on(s_notice.x, s_notice.y);
        close_to_play();
    } else if (sim_action_pressed(SIM_ACT_SECONDARY)) {
        ui_sound(UI_SND_BACK);
        close_to_play();
    }
}

void notice_draw(void)
{
    ui_dim(0, TOP_BAR_H, SCREEN_W, SCREEN_H - TOP_BAR_H);
    const int w = 300, h = 112;
    int x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + 8;
    ui_panel(x, y, w, h);
    ui_fill(x + 2, y + 2, w - 4, 34, UI_BLACK);
    ui_icon(s_notice.icon, x + 12, y + 10, true);
    ui_text(UI_FONT_BIG, x + 36, y + 6, s_notice.title, FONT_ALIGN_LEFT, UI_WHITE);
    ui_text_wrap(UI_FONT_BOLD, x + 14, y + 46, w - 28, s_notice.body, UI_BLACK, 3);
    bool has_loc = s_notice.x >= 0 && s_notice.y >= 0;
    ui_hints(x + w - 8, y + h - 22, has_loc ? "Go there" : "OK", has_loc ? "Dismiss" : NULL, UI_BLACK);
}
