#ifndef SIM_RENDER_H
#define SIM_RENDER_H

#include <stdbool.h>
#include "types.h"
#include "image.h"
#include "font.h"

#define SIM_MAP_WIDTH  120
#define SIM_MAP_HEIGHT 100
#define SIM_TILE_SIZE  16

typedef enum {
    OVERLAY_NORMAL = 0,
    OVERLAY_POWER,
    OVERLAY_TRAFFIC,
    OVERLAY_POLLUTION,
    OVERLAY_CRIME,
    OVERLAY_LAND_VALUE,
    OVERLAY_COUNT
} sim_overlay_t;

typedef struct sim_viewport_t {
    int cam_x;           /* Camera pixel X (0 .. 120*16 - view_w) */
    int cam_y;           /* Camera pixel Y (0 .. 100*16 - view_h) */
    int view_w;          /* Viewport width in pixels (e.g. 400) */
    int view_h;          /* Viewport height in pixels (e.g. 240) */
    int cursor_x;        /* Selected tile X (0 .. 119) */
    int cursor_y;        /* Selected tile Y (0 .. 99) */
    int tool_size;       /* Active tool dimension in tiles (1, 3, 4, 6) */
    bool show_minimap;   /* Minimap visibility flag */
    bool show_hud;       /* HUD bar visibility flag */
    bool show_cursor;    /* Cursor placement box flag */
    sim_overlay_t overlay;
} sim_viewport_t;

extern sim_viewport_t g_sim_viewport;

bool sim_render_init(void);
void sim_render_cleanup(void);

/* Viewport & layers */
void sim_render_map(const sim_viewport_t *vp);
void sim_render_sprites(const sim_viewport_t *vp);
void sim_render_cursor(const sim_viewport_t *vp);
void sim_render_minimap(vec2i_t pos, vec2i_t size, const sim_viewport_t *vp);
void sim_render_hud(const sim_viewport_t *vp, const char *city_name, long funds,
                    int pop, int year, int month, const char *tool_name, int speed);

/* Coordinate conversions */
vec2i_t sim_screen_to_tile(vec2i_t screen_pt, const sim_viewport_t *vp);
vec2i_t sim_tile_to_screen(int tile_x, int tile_y, const sim_viewport_t *vp);

/* UI & Modal Types */
typedef struct {
    bool active;
    char name[64];
    char density[32];
    char value[32];
    char crime[32];
    char pollution[32];
    char growth[32];
    int tile_x;
    int tile_y;
    bool powered;
} sim_query_info_t;

typedef struct {
    int tax_rate;       /* 0 .. 20 % */
    int road_fund;      /* 0 .. 100 % */
    int police_fund;    /* 0 .. 100 % */
    int fire_fund;      /* 0 .. 100 % */
    int selected_item;  /* 0: Tax, 1: Road, 2: Police, 3: Fire, 4: Continue */
    long tax_revenue;
    long road_cost;
    long police_cost;
    long fire_cost;
    long previous_funds;
    long current_funds;
} sim_budget_modal_t;

typedef struct {
    int yes_pct;
    int no_pct;
    int score;
    int pop;
    int class_id;       /* 0: Village .. 5: Megalopolis */
    long assessed_val;
    int problems[4];    /* Top 4 problem IDs */
    int problem_votes[4];
} sim_eval_modal_t;

typedef struct {
    const char *title;
    const char *items[12];
    int item_count;
    int selected_index;
} sim_menu_modal_t;

/* Additional UI Rendering Functions */
void sim_render_rci(vec2i_t pos, short r_valve, short c_valve, short i_valve);
void sim_render_tool_palette(const sim_viewport_t *vp, int current_tool_idx, bool expanded);
void sim_render_query_card(const sim_query_info_t *info, vec2i_t screen_center);
void sim_render_budget_modal(const sim_budget_modal_t *budget, vec2i_t screen_center);
void sim_render_eval_modal(const sim_eval_modal_t *eval_data, vec2i_t screen_center);
void sim_render_menu_modal(const sim_menu_modal_t *menu, vec2i_t screen_center);
void sim_render_toast(const char *msg, float alpha, vec2i_t screen_size);

/* Interactive Click Targets */
bmfont_t *sim_render_get_font(void);
bool sim_render_is_point_in_tool_tray(vec2i_t pt, const sim_viewport_t *vp, int *out_tool_idx);
bool sim_render_is_point_in_minimap(vec2i_t pt, const sim_viewport_t *vp, vec2i_t *out_tile);

#endif /* SIM_RENDER_H */

