#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim.h"
#include "sim_render.h"
#include "render.h"
#include "image.h"
#include "font.h"
#include "tileset_atlas.h"

sim_viewport_t g_sim_viewport = {
    .cam_x = 0,
    .cam_y = 0,
    .view_w = 400,
    .view_h = 240,
    .cursor_x = 60,
    .cursor_y = 50,
    .tool_size = 1,
    .show_minimap = false,
    .show_hud = true,
    .show_cursor = true,
    .overlay = OVERLAY_NORMAL
};

static image_t *s_tiles_img = NULL;
static image_t *s_sprites_img = NULL;
static bmfont_t *s_hud_font = NULL;

bool sim_render_init(void)
{
    if (!s_tiles_img) {
        s_tiles_img = image_load("assets/gfx/tiles_16.png");
        if (!s_tiles_img) {
            /* Try alternate monochrome asset */
            s_tiles_img = image_load("assets/gfx/tiles_16_mono.png");
        }
    }

    if (!s_sprites_img) {
        s_sprites_img = image_load("assets/gfx/sprites_16.png");
    }

    if (!s_hud_font) {
        s_hud_font = font_load_bmfont("assets/fonts/prince.bmfnt", NULL);
    }

    return (s_tiles_img != NULL);
}

void sim_render_cleanup(void)
{
    /* Tiny Engine manages image texture lifecycle */
    s_tiles_img = NULL;
    s_sprites_img = NULL;
    s_hud_font = NULL;
}

vec2i_t sim_screen_to_tile(vec2i_t screen_pt, const sim_viewport_t *vp)
{
    int world_px = vp->cam_x + screen_pt.x;
    int world_py = vp->cam_y + screen_pt.y;
    int tx = world_px / SIM_TILE_SIZE;
    int ty = world_py / SIM_TILE_SIZE;

    if (tx < 0) tx = 0;
    if (tx >= SIM_MAP_WIDTH) tx = SIM_MAP_WIDTH - 1;
    if (ty < 0) ty = 0;
    if (ty >= SIM_MAP_HEIGHT) ty = SIM_MAP_HEIGHT - 1;

    return vec2i(tx, ty);
}

vec2i_t sim_tile_to_screen(int tile_x, int tile_y, const sim_viewport_t *vp)
{
    int sx = tile_x * SIM_TILE_SIZE - vp->cam_x;
    int sy = tile_y * SIM_TILE_SIZE - vp->cam_y;
    return vec2i(sx, sy);
}

void sim_render_map(const sim_viewport_t *vp)
{
    if (!s_tiles_img) return;

    int start_x = vp->cam_x / SIM_TILE_SIZE;
    int end_x   = (vp->cam_x + vp->view_w + SIM_TILE_SIZE - 1) / SIM_TILE_SIZE;
    int start_y = vp->cam_y / SIM_TILE_SIZE;
    int end_y   = (vp->cam_y + vp->view_h + SIM_TILE_SIZE - 1) / SIM_TILE_SIZE;

    if (start_x < 0) start_x = 0;
    if (end_x > SIM_MAP_WIDTH) end_x = SIM_MAP_WIDTH;
    if (start_y < 0) start_y = 0;
    if (end_y > SIM_MAP_HEIGHT) end_y = SIM_MAP_HEIGHT;

    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            unsigned short cell = Map[x][y];
            unsigned short tile_id = cell & LOMASK;

            int dst_x = x * SIM_TILE_SIZE - vp->cam_x;
            int dst_y = y * SIM_TILE_SIZE - vp->cam_y;

            /* Base tile blit */
            image_draw_tile(s_tiles_img, tile_id, vec2i(16, 16),
                            vec2i(dst_x, dst_y), false, false, rgba_white());

            /* Dynamic blinking indicator: unpowered zone centers flash lightning bolt (827) */
            if ((cell & ZONEBIT) && !(cell & PWRBIT) && flagBlink) {
                image_draw_tile(s_tiles_img, 827, vec2i(16, 16),
                                vec2i(dst_x, dst_y), false, false, rgba_white());
            }

            /* Overlay modes */
            if (vp->overlay == OVERLAY_POWER) {
                if (cell & CONDBIT) {
                    if (!(cell & PWRBIT)) {
                        /* Unpowered conductor highlighted in red */
                        render_draw_rect(vec2i(dst_x + 2, dst_y + 2), vec2i(12, 12), rgba(255, 0, 0, 180));
                    } else {
                        /* Powered conductor highlighted in yellow */
                        render_draw_rect(vec2i(dst_x + 4, dst_y + 4), vec2i(8, 8), rgba(255, 255, 0, 160));
                    }
                }
            } else if (vp->overlay == OVERLAY_TRAFFIC) {
                int dens = TrfDensity[x >> 1][y >> 1];
                if (dens > 30) {
                    int alpha = (dens > 200) ? 180 : (dens * 180 / 200);
                    render_fill_rect(vec2i(dst_x, dst_y), vec2i(16, 16), rgba(255, 0, 0, (uint8_t)alpha));
                }
            } else if (vp->overlay == OVERLAY_POLLUTION) {
                int pol = PollutionMem[x >> 1][y >> 1];
                if (pol > 10) {
                    int alpha = (pol > 200) ? 180 : (pol * 180 / 200);
                    render_fill_rect(vec2i(dst_x, dst_y), vec2i(16, 16), rgba(160, 80, 0, (uint8_t)alpha));
                }
            } else if (vp->overlay == OVERLAY_CRIME) {
                int crm = CrimeMem[x >> 1][y >> 1];
                if (crm > 20) {
                    int alpha = (crm > 200) ? 180 : (crm * 180 / 200);
                    render_fill_rect(vec2i(dst_x, dst_y), vec2i(16, 16), rgba(0, 0, 255, (uint8_t)alpha));
                }
            } else if (vp->overlay == OVERLAY_LAND_VALUE) {
                int lv = LandValueMem[x >> 1][y >> 1];
                if (lv > 0) {
                    render_fill_rect(vec2i(dst_x, dst_y), vec2i(16, 16), rgba(0, 255, 0, (uint8_t)(lv * 150 / 250)));
                }
            }
        }
    }
}

void sim_render_sprites(const sim_viewport_t *vp)
{
    if (!sim) return;

    for (SimSprite *s = sim->sprite; s != NULL; s = s->next) {
        if (s->frame == 0) continue;

        int sx, sy;
        if (s->type == TRA) {
            sx = (s->x + 48) - vp->cam_x;
            sy = s->y - vp->cam_y;
        } else {
            sx = (s->x + s->x_hot) - vp->cam_x;
            sy = (s->y + s->y_hot) - vp->cam_y;
        }

        if (sx < -16 || sx >= vp->view_w || sy < -16 || sy >= vp->view_h)
            continue;

        /* Map sprite type to tile id */
        int sprite_id = 0;
        switch (s->type) {
        case TRA: sprite_id = 0; break; /* Train */
        case COP: sprite_id = 1; break; /* Copter */
        case AIR: sprite_id = 2; break; /* Airplane */
        case SHI: sprite_id = 3; break; /* Ship */
        case GOD: sprite_id = 4; break; /* Monster */
        case TOR: sprite_id = 5; break; /* Tornado */
        case EXP:
            /* Explosion drawn from tile atlas */
            image_draw_tile(s_tiles_img, 44 + (s->frame % 4), vec2i(16, 16),
                            vec2i(sx, sy), false, false, rgba_white());
            continue;
        default:  sprite_id = 0; break;
        }

        if (s_sprites_img) {
            image_draw_tile(s_sprites_img, sprite_id, vec2i(16, 16),
                            vec2i(sx, sy), false, false, rgba_white());
        } else if (s_tiles_img) {
            /* Fallback to tile atlas sprite range */
            image_draw_tile(s_tiles_img, 960 + sprite_id, vec2i(16, 16),
                            vec2i(sx, sy), false, false, rgba_white());
        }
    }
}

void sim_render_cursor(const sim_viewport_t *vp)
{
    if (!vp->show_cursor) return;

    int sx = vp->cursor_x * SIM_TILE_SIZE - vp->cam_x;
    int sy = vp->cursor_y * SIM_TILE_SIZE - vp->cam_y;
    int size_px = vp->tool_size * SIM_TILE_SIZE;

    /* Highlight box around active cursor position */
    render_draw_rect(vec2i(sx - 1, sy - 1), vec2i(size_px + 2, size_px + 2), rgba(0, 0, 0, 255));
    render_draw_rect(vec2i(sx, sy), vec2i(size_px, size_px), rgba(255, 255, 0, 255));
}

void sim_render_minimap(vec2i_t pos, vec2i_t size, const sim_viewport_t *vp)
{
    /* Background frame: solid black with white border */
    render_fill_rect(vec2i(pos.x - 2, pos.y - 2), vec2i(size.x + 4, size.y + 4), rgba_black());
    render_draw_rect(vec2i(pos.x - 1, pos.y - 1), vec2i(size.x + 2, size.y + 2), rgba_white());

    /* 60x50 sampling: 2x2 map tiles per minimap pixel */
    for (int my = 0; my < size.y; my++) {
        int ty = my * SIM_MAP_HEIGHT / size.y;
        for (int mx = 0; mx < size.x; mx++) {
            int tx = mx * SIM_MAP_WIDTH / size.x;
            unsigned short tile = Map[tx][ty] & LOMASK;

            rgba_t color;
            if (tile == DIRT) {
                color = rgba(180, 160, 120, 255);
            } else if (tile >= RIVER && tile <= LASTRIVEDGE) {
                color = rgba(40, 100, 200, 255);
            } else if (tile >= TREEBASE && tile <= WOODS5) {
                color = rgba(30, 140, 40, 255);
            } else if (tile >= RUBBLE && tile <= LASTFIRE) {
                color = rgba(180, 40, 20, 255);
            } else if (tile >= ROADBASE && tile < RESBASE) {
                color = rgba(130, 130, 130, 255);
            } else if (tile >= RESBASE && tile < COMBASE) {
                color = rgba(60, 180, 60, 255);
            } else if (tile >= COMBASE && tile < INDBASE) {
                color = rgba(50, 80, 220, 255);
            } else if (tile >= INDBASE && tile < PORTBASE) {
                color = rgba(220, 180, 30, 255);
            } else if (tile >= PORTBASE && tile <= NUCLEARBASE + 15) {
                color = rgba(160, 90, 40, 255);
            } else {
                color = rgba(100, 100, 100, 255);
            }

            render_draw_point(vec2i(pos.x + mx, pos.y + my), color);
        }
    }

    /* Camera viewport outline on minimap */
    int cx = pos.x + (vp->cam_x * size.x) / (SIM_MAP_WIDTH * SIM_TILE_SIZE);
    int cy = pos.y + (vp->cam_y * size.y) / (SIM_MAP_HEIGHT * SIM_TILE_SIZE);
    int cw = (vp->view_w * size.x) / (SIM_MAP_WIDTH * SIM_TILE_SIZE);
    int ch = (vp->view_h * size.y) / (SIM_MAP_HEIGHT * SIM_TILE_SIZE);
    if (cw < 3) cw = 3;
    if (ch < 3) ch = 3;

    render_draw_rect(vec2i(cx, cy), vec2i(cw, ch), rgba_white());
}

void sim_render_hud(const sim_viewport_t *vp, const char *city_name, long funds,
                    int pop, int year, int month, const char *tool_name, int speed)
{
    if (!vp->show_hud) return;

    /* Top status bar: 16px high, solid black with white underline */
    render_fill_rect(vec2i(0, 0), vec2i(vp->view_w, 16), rgba_black());
    render_draw_line(vec2i(0, 16), vec2i(vp->view_w, 16), rgba_white());

    static const char *month_names[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const char *m_str = (month >= 0 && month < 12) ? month_names[month] : "Jan";

    char text[256];
    snprintf(text, sizeof(text), "%s | %s %d | $%ld | Pop:%d | Spd:%d | %s",
             city_name ? city_name : "Metropolis",
             m_str, year, funds, pop, speed,
             tool_name ? tool_name : "Pan");

    if (s_hud_font) {
        font_draw_bmfont(s_hud_font, vec2i(6, 2), text, FONT_ALIGN_LEFT, rgba_white());
    }

    /* Active overlay indicator */
    if (vp->overlay != OVERLAY_NORMAL) {
        const char *ov_names[] = { "", "POWER", "TRAFFIC", "POLLUTION", "CRIME", "VALUE" };
        char ov_txt[32];
        snprintf(ov_txt, sizeof(ov_txt), "[%s]", ov_names[vp->overlay]);
        if (s_hud_font) {
            font_draw_bmfont(s_hud_font, vec2i(vp->view_w - 6, 2), ov_txt, FONT_ALIGN_RIGHT, rgba(255, 200, 0, 255));
        }
    }
}

void sim_render_rci(vec2i_t pos, short r_valve, short c_valve, short i_valve)
{
    /* Background box: 24x12, solid black with white border */
    render_fill_rect(pos, vec2i(24, 12), rgba_black());
    render_draw_rect(pos, vec2i(24, 12), rgba_white());

    int base_y = pos.y + 6;
    render_draw_line(vec2i(pos.x + 1, base_y), vec2i(pos.x + 22, base_y), rgba(120, 120, 120, 255));

    /* R: Residential (Green) */
    int r_h = (abs(r_valve) * 5) / 2000;
    if (r_h > 5) r_h = 5;
    if (r_valve >= 0) {
        if (r_h > 0) render_fill_rect(vec2i(pos.x + 3, base_y - r_h), vec2i(4, r_h), rgba(40, 220, 60, 255));
    } else {
        if (r_h > 0) render_fill_rect(vec2i(pos.x + 3, base_y), vec2i(4, r_h), rgba(220, 50, 50, 255));
    }

    /* C: Commercial (Blue) */
    int c_h = (abs(c_valve) * 5) / 1500;
    if (c_h > 5) c_h = 5;
    if (c_valve >= 0) {
        if (c_h > 0) render_fill_rect(vec2i(pos.x + 10, base_y - c_h), vec2i(4, c_h), rgba(60, 140, 255, 255));
    } else {
        if (c_h > 0) render_fill_rect(vec2i(pos.x + 10, base_y), vec2i(4, c_h), rgba(220, 50, 50, 255));
    }

    /* I: Industrial (Yellow) */
    int i_h = (abs(i_valve) * 5) / 1500;
    if (i_h > 5) i_h = 5;
    if (i_valve >= 0) {
        if (i_h > 0) render_fill_rect(vec2i(pos.x + 17, base_y - i_h), vec2i(4, i_h), rgba(240, 210, 40, 255));
    } else {
        if (i_h > 0) render_fill_rect(vec2i(pos.x + 17, base_y), vec2i(4, i_h), rgba(220, 50, 50, 255));
    }
}

static const int s_tool_tile_icons[15] = {
    56,  /* road */
    208, /* wire */
    966, /* bulldozer */
    240, /* res */
    423, /* com */
    612, /* ind */
    709, /* fire */
    718, /* police */
    745, /* stadium */
    44,  /* park */
    756, /* seaport */
    765, /* coal */
    774, /* nuclear */
    783, /* airport */
    967  /* query */
};

void sim_render_tool_palette(const sim_viewport_t *vp, int current_tool_idx, bool expanded)
{
    (void)expanded;
    int tool_count = 15;
    int icon_size = 16;
    int pad = 2;
    int total_w = tool_count * (icon_size + pad) + pad;
    int start_x = (vp->view_w - total_w) / 2;
    int start_y = vp->view_h - 20;

    /* Background tray: solid black with white border */
    render_fill_rect(vec2i(start_x - 2, start_y - 2), vec2i(total_w + 4, icon_size + 4), rgba_black());
    render_draw_rect(vec2i(start_x - 2, start_y - 2), vec2i(total_w + 4, icon_size + 4), rgba_white());

    for (int i = 0; i < tool_count; i++) {
        int x = start_x + i * (icon_size + pad);
        int y = start_y;

        if (s_tiles_img) {
            int tile_id = s_tool_tile_icons[i];
            image_draw_tile(s_tiles_img, tile_id, vec2i(16, 16), vec2i(x, y), false, false, rgba_white());
        }

        if (i == current_tool_idx) {
            /* Highlight selection */
            render_draw_rect(vec2i(x - 1, y - 1), vec2i(icon_size + 2, icon_size + 2), rgba(255, 220, 40, 255));
        }
    }
}

void sim_render_query_card(const sim_query_info_t *info, vec2i_t screen_center)
{
    if (!info || !info->active) return;

    int w = 220;
    int h = 120;
    vec2i_t pos = vec2i(screen_center.x - w / 2, screen_center.y - h / 2);

    /* Background panel: solid black with white double border */
    render_fill_rect(pos, vec2i(w, h), rgba_black());
    render_draw_rect(pos, vec2i(w, h), rgba_white());
    render_draw_rect(vec2i(pos.x + 2, pos.y + 2), vec2i(w - 4, h - 4), rgba_white());
    render_draw_line(vec2i(pos.x + 2, pos.y + 20), vec2i(pos.x + w - 3, pos.y + 20), rgba_white());

    if (s_hud_font) {
        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 5), "ZONE QUERY", FONT_ALIGN_CENTER, rgba(255, 215, 60, 255));

        /* Details */
        char buf[128];
        snprintf(buf, sizeof(buf), "Zone: %s", info->name);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, pos.y + 24), buf, FONT_ALIGN_LEFT, rgba_white());

        snprintf(buf, sizeof(buf), "Density: %s", info->density[0] ? info->density : "Low");
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, pos.y + 38), buf, FONT_ALIGN_LEFT, rgba(200, 200, 200, 255));

        snprintf(buf, sizeof(buf), "Power: %s", info->powered ? "YES" : "NO");
        rgba_t pwr_col = info->powered ? rgba(60, 240, 60, 255) : rgba(240, 60, 60, 255);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 120, pos.y + 38), buf, FONT_ALIGN_LEFT, pwr_col);

        snprintf(buf, sizeof(buf), "Value: %s", info->value[0] ? info->value : "Medium");
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, pos.y + 52), buf, FONT_ALIGN_LEFT, rgba(200, 200, 200, 255));

        snprintf(buf, sizeof(buf), "Crime: %s", info->crime[0] ? info->crime : "None");
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 120, pos.y + 52), buf, FONT_ALIGN_LEFT, rgba(200, 200, 200, 255));

        snprintf(buf, sizeof(buf), "Pollution: %s", info->pollution[0] ? info->pollution : "None");
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, pos.y + 66), buf, FONT_ALIGN_LEFT, rgba(200, 200, 200, 255));

        snprintf(buf, sizeof(buf), "Growth: %s", info->growth[0] ? info->growth : "Stable");
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, pos.y + 80), buf, FONT_ALIGN_LEFT, rgba(200, 200, 200, 255));

        /* Footer hint */
        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 100), "[ Press (B) to close ]", FONT_ALIGN_CENTER, rgba(160, 170, 190, 255));
    }
}

void sim_render_budget_modal(const sim_budget_modal_t *budget, vec2i_t screen_center)
{
    if (!budget) return;

    int w = 270;
    int h = 180;
    vec2i_t pos = vec2i(screen_center.x - w / 2, screen_center.y - h / 2);

    /* Background panel: solid black with white double border */
    render_fill_rect(pos, vec2i(w, h), rgba_black());
    render_draw_rect(pos, vec2i(w, h), rgba_white());
    render_draw_rect(vec2i(pos.x + 2, pos.y + 2), vec2i(w - 4, h - 4), rgba_white());
    render_draw_line(vec2i(pos.x + 2, pos.y + 22), vec2i(pos.x + w - 3, pos.y + 22), rgba_white());

    if (s_hud_font) {
        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 5), "CITY BUDGET & TAXES", FONT_ALIGN_CENTER, rgba(255, 215, 60, 255));

        /* Budget Items */
        const char *labels[4] = { "Tax Rate:", "Road Fund:", "Police Fund:", "Fire Fund:" };
        int values[4] = { budget->tax_rate, budget->road_fund, budget->police_fund, budget->fire_fund };
        long costs[4] = { budget->tax_revenue, budget->road_cost, budget->police_cost, budget->fire_cost };

        for (int i = 0; i < 4; i++) {
            int row_y = pos.y + 30 + i * 20;
            bool sel = (budget->selected_item == i);

            if (sel) {
                render_draw_rect(vec2i(pos.x + 6, row_y - 2), vec2i(w - 12, 16), rgba_white());
            }

            char item_buf[64];
            snprintf(item_buf, sizeof(item_buf), "%s%s", sel ? "> " : "  ", labels[i]);
            font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, row_y), item_buf, FONT_ALIGN_LEFT, sel ? rgba(255, 230, 100, 255) : rgba_white());

            char val_buf[32];
            snprintf(val_buf, sizeof(val_buf), "< %3d%% >", values[i]);
            font_draw_bmfont(s_hud_font, vec2i(pos.x + 115, row_y), val_buf, FONT_ALIGN_LEFT, sel ? rgba(255, 255, 100, 255) : rgba(200, 200, 220, 255));

            char cost_buf[32];
            if (i == 0) {
                snprintf(cost_buf, sizeof(cost_buf), "+$%ld", costs[i]);
                font_draw_bmfont(s_hud_font, vec2i(pos.x + w - 12, row_y), cost_buf, FONT_ALIGN_RIGHT, rgba(60, 220, 60, 255));
            } else {
                snprintf(cost_buf, sizeof(cost_buf), "-$%ld", costs[i]);
                font_draw_bmfont(s_hud_font, vec2i(pos.x + w - 12, row_y), cost_buf, FONT_ALIGN_RIGHT, rgba(240, 100, 80, 255));
            }
        }

        /* Divider */
        render_draw_line(vec2i(pos.x + 10, pos.y + 115), vec2i(pos.x + w - 10, pos.y + 115), rgba_white());

        /* Summary Stats */
        long total_expense = budget->road_cost + budget->police_cost + budget->fire_cost;
        long cash_flow = budget->tax_revenue - total_expense;

        char flow_buf[64];
        snprintf(flow_buf, sizeof(flow_buf), "Cash Flow: %s$%ld", cash_flow >= 0 ? "+" : "", cash_flow);
        rgba_t flow_col = cash_flow >= 0 ? rgba(60, 220, 60, 255) : rgba(240, 60, 60, 255);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, pos.y + 122), flow_buf, FONT_ALIGN_LEFT, flow_col);

        char fund_buf[64];
        snprintf(fund_buf, sizeof(fund_buf), "Funds: $%ld", budget->current_funds);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + w - 12, pos.y + 122), fund_buf, FONT_ALIGN_RIGHT, rgba(255, 215, 60, 255));

        /* Continue Button (Item 4) */
        bool btn_sel = (budget->selected_item == 4);
        render_fill_rect(vec2i(pos.x + 35, pos.y + 144), vec2i(w - 70, 18), btn_sel ? rgba_white() : rgba_black());
        render_draw_rect(vec2i(pos.x + 35, pos.y + 144), vec2i(w - 70, 18), rgba_white());
        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 147), "[ CONTINUE ]", FONT_ALIGN_CENTER, btn_sel ? rgba_black() : rgba_white());

        /* Controls Hint */
        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 167), "Up/Down: Select | Left/Right: Adjust | (A): Done", FONT_ALIGN_CENTER, rgba(180, 190, 200, 255));
    }
}

void sim_render_eval_modal(const sim_eval_modal_t *eval_data, vec2i_t screen_center)
{
    if (!eval_data) return;

    int w = 270;
    int h = 180;
    vec2i_t pos = vec2i(screen_center.x - w / 2, screen_center.y - h / 2);

    /* Background panel: solid black with white double border */
    render_fill_rect(pos, vec2i(w, h), rgba_black());
    render_draw_rect(pos, vec2i(w, h), rgba_white());
    render_draw_rect(vec2i(pos.x + 2, pos.y + 2), vec2i(w - 4, h - 4), rgba_white());
    render_draw_line(vec2i(pos.x + 2, pos.y + 22), vec2i(pos.x + w - 3, pos.y + 22), rgba_white());

    if (s_hud_font) {
        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 5), "CITY EVALUATION", FONT_ALIGN_CENTER, rgba(255, 215, 60, 255));

        /* Ratings */
        char buf[128];
        snprintf(buf, sizeof(buf), "Mayor Approval: %d%% Yes / %d%% No", eval_data->yes_pct, eval_data->no_pct);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 12, pos.y + 28), buf, FONT_ALIGN_LEFT, rgba(240, 240, 240, 255));

        snprintf(buf, sizeof(buf), "Overall City Score: %d / 1000", eval_data->score);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 12, pos.y + 44), buf, FONT_ALIGN_LEFT, rgba(255, 215, 60, 255));

        static const char *class_names[] = { "Village", "Town", "City", "Capital", "Metropolis", "Megalopolis" };
        const char *c_name = (eval_data->class_id >= 0 && eval_data->class_id < 6) ? class_names[eval_data->class_id] : "City";
        snprintf(buf, sizeof(buf), "Class: %s (Pop: %d)", c_name, eval_data->pop);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 12, pos.y + 60), buf, FONT_ALIGN_LEFT, rgba(200, 200, 220, 255));

        snprintf(buf, sizeof(buf), "Assessed Value: $%ld", eval_data->assessed_val);
        font_draw_bmfont(s_hud_font, vec2i(pos.x + 12, pos.y + 76), buf, FONT_ALIGN_LEFT, rgba(200, 200, 220, 255));

        /* Divider */
        render_draw_line(vec2i(pos.x + 10, pos.y + 94), vec2i(pos.x + w - 10, pos.y + 94), rgba_white());

        font_draw_bmfont(s_hud_font, vec2i(pos.x + 12, pos.y + 100), "Top Public Complaints:", FONT_ALIGN_LEFT, rgba(255, 180, 60, 255));

        static const char *prob_names[] = {
            "Crime", "Pollution", "Housing Costs", "Taxes",
            "Traffic", "Unemployment", "Fire Protection", "None"
        };

        for (int i = 0; i < 4; i++) {
            int pid = eval_data->problems[i];
            const char *p_name = (pid >= 0 && pid < 8) ? prob_names[pid] : "None";
            snprintf(buf, sizeof(buf), "%d. %s (%d%%)", i + 1, p_name, eval_data->problem_votes[i]);
            int col_x = (i % 2 == 0) ? pos.x + 16 : pos.x + 140;
            int row_y = pos.y + 118 + (i / 2) * 16;
            font_draw_bmfont(s_hud_font, vec2i(col_x, row_y), buf, FONT_ALIGN_LEFT, rgba(210, 210, 220, 255));
        }

        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 158), "[ Press (B) to close ]", FONT_ALIGN_CENTER, rgba(160, 170, 190, 255));
    }
}

void sim_render_menu_modal(const sim_menu_modal_t *menu, vec2i_t screen_center)
{
    if (!menu) return;

    int item_h = 16;
    int w = 240;
    int h = 40 + menu->item_count * item_h;
    vec2i_t pos = vec2i(screen_center.x - w / 2, screen_center.y - h / 2);

    /* Background panel: solid black with white double border */
    render_fill_rect(pos, vec2i(w, h), rgba_black());
    render_draw_rect(pos, vec2i(w, h), rgba_white());
    render_draw_rect(vec2i(pos.x + 2, pos.y + 2), vec2i(w - 4, h - 4), rgba_white());
    render_draw_line(vec2i(pos.x + 2, pos.y + 22), vec2i(pos.x + w - 3, pos.y + 22), rgba_white());

    if (s_hud_font) {
        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + 5), menu->title ? menu->title : "MENU", FONT_ALIGN_CENTER, rgba(255, 215, 60, 255));

        for (int i = 0; i < menu->item_count; i++) {
            int row_y = pos.y + 26 + i * item_h;
            bool sel = (menu->selected_index == i);

            if (sel) {
                render_draw_rect(vec2i(pos.x + 6, row_y - 1), vec2i(w - 12, item_h), rgba_white());
            }

            char buf[64];
            snprintf(buf, sizeof(buf), "%s%s", sel ? "> " : "  ", menu->items[i] ? menu->items[i] : "");
            font_draw_bmfont(s_hud_font, vec2i(pos.x + 10, row_y), buf, FONT_ALIGN_LEFT, sel ? rgba(255, 240, 100, 255) : rgba_white());
        }

        font_draw_bmfont(s_hud_font, vec2i(screen_center.x, pos.y + h - 14), "Up/Down: Move | (A): Select | (B): Cancel", FONT_ALIGN_CENTER, rgba(160, 170, 190, 255));
    }
}

void sim_render_toast(const char *msg, float alpha, vec2i_t screen_size)
{
    if (!msg || !s_hud_font || alpha <= 0.0f) return;

    int text_w = font_bmfont_get_width(s_hud_font, msg);
    int pad_x = 10;
    int pad_y = 4;
    int box_w = text_w + pad_x * 2;
    int box_h = 16 + pad_y * 2;
    int x = (screen_size.x - box_w) / 2;
    int y = screen_size.y - 42;

    render_fill_rect(vec2i(x, y), vec2i(box_w, box_h), rgba_black());
    render_draw_rect(vec2i(x, y), vec2i(box_w, box_h), rgba_white());
    font_draw_bmfont(s_hud_font, vec2i(x + pad_x, y + pad_y), msg, FONT_ALIGN_LEFT, rgba(255, 230, 100, (uint8_t)(alpha * 255.0f)));
}

bmfont_t *sim_render_get_font(void)
{
    return s_hud_font;
}


bool sim_render_is_point_in_tool_tray(vec2i_t pt, const sim_viewport_t *vp, int *out_tool_idx)
{
    if (!vp) return false;
    int tool_count = 15;
    int icon_size = 16;
    int pad = 2;
    int total_w = tool_count * (icon_size + pad) + pad;
    int start_x = (vp->view_w - total_w) / 2;
    int start_y = vp->view_h - 20;

    if (pt.x >= start_x && pt.x < start_x + total_w &&
        pt.y >= start_y && pt.y < start_y + icon_size + 4) {
        int idx = (pt.x - start_x) / (icon_size + pad);
        if (idx >= 0 && idx < tool_count) {
            if (out_tool_idx) *out_tool_idx = idx;
            return true;
        }
    }
    return false;
}

bool sim_render_is_point_in_minimap(vec2i_t pt, const sim_viewport_t *vp, vec2i_t *out_tile)
{
    if (!vp || !vp->show_minimap) return false;
    vec2i_t pos = vec2i(vp->view_w - 68, 22);
    vec2i_t size = vec2i(60, 50);

    if (pt.x >= pos.x && pt.x < pos.x + size.x &&
        pt.y >= pos.y && pt.y < pos.y + size.y) {
        int mx = pt.x - pos.x;
        int my = pt.y - pos.y;
        if (out_tile) {
            out_tile->x = (mx * SIM_MAP_WIDTH) / size.x;
            out_tile->y = (my * SIM_MAP_HEIGHT) / size.y;
            if (out_tile->x < 0) out_tile->x = 0;
            if (out_tile->y < 0) out_tile->y = 0;
            if (out_tile->x >= SIM_MAP_WIDTH) out_tile->x = SIM_MAP_WIDTH - 1;
            if (out_tile->y >= SIM_MAP_HEIGHT) out_tile->y = SIM_MAP_HEIGHT - 1;
        }
        return true;
    }
    return false;
}

