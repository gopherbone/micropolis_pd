/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#include <string.h>
#include "sim_input.h"
#include "platform.h"

#if !defined(PLAYDATE)
#include <SDL2/SDL.h>
#endif

static bool s_action_held[SIM_ACT_COUNT];
static bool s_action_prev[SIM_ACT_COUNT];
static bool s_action_pressed[SIM_ACT_COUNT];
static bool s_action_released[SIM_ACT_COUNT];
static bool s_action_triggered[SIM_ACT_COUNT];

static sim_mouse_t s_mouse;
static bool s_prev_mouse_left = false;
static bool s_prev_mouse_right = false;
static float s_crank_change = 0.0f;
static float s_crank_accum = 0.0f;
/* After a scene switch, buttons still held from the previous scene must not
 * count as new presses (e.g. the A that confirmed "Quit to title"). */
static bool s_wait_release = false;

void sim_input_init(void)
{
    memset(s_action_held, 0, sizeof(s_action_held));
    memset(s_action_prev, 0, sizeof(s_action_prev));
    memset(s_action_pressed, 0, sizeof(s_action_pressed));
    memset(s_action_released, 0, sizeof(s_action_released));
    memset(s_action_triggered, 0, sizeof(s_action_triggered));
    memset(&s_mouse, 0, sizeof(s_mouse));
    s_prev_mouse_left = false;
    s_prev_mouse_right = false;
    s_crank_change = 0.0f;
    s_crank_accum = 0.0f;
    s_wait_release = true;
}

void sim_input_poll(float dt)
{
    (void)dt;

    /* 1. Save previous action states to calculate pressed / released edges */
    memcpy(s_action_prev, s_action_held, sizeof(s_action_held));
    memset(s_action_held, 0, sizeof(s_action_held));

    /* Apply external triggered actions (e.g. from system menu callbacks) */
    for (int i = 0; i < SIM_ACT_COUNT; i++) {
        if (s_action_triggered[i]) {
            s_action_held[i] = true;
            s_action_triggered[i] = false;
        }
    }


    /* 2. Poll Engine Platform Input (Playdate buttons & Crank) */
    const input_state_t *plat_in = platform_get_input();
    if (plat_in) {
        if (plat_in->held[KEY_UP])    s_action_held[SIM_ACT_UP] = true;
        if (plat_in->held[KEY_DOWN])  s_action_held[SIM_ACT_DOWN] = true;
        if (plat_in->held[KEY_LEFT])  s_action_held[SIM_ACT_LEFT] = true;
        if (plat_in->held[KEY_RIGHT]) s_action_held[SIM_ACT_RIGHT] = true;

        if (plat_in->held[KEY_A])     s_action_held[SIM_ACT_PRIMARY] = true;
        if (plat_in->held[KEY_B])     s_action_held[SIM_ACT_SECONDARY] = true;

        if (plat_in->held[KEY_START])  s_action_held[SIM_ACT_MENU] = true;
        if (plat_in->held[KEY_SELECT]) s_action_held[SIM_ACT_MINIMAP] = true;

        if (plat_in->held[KEY_PAGEUP])   s_action_held[SIM_ACT_PREV_TOOL] = true;
        if (plat_in->held[KEY_PAGEDOWN]) s_action_held[SIM_ACT_NEXT_TOOL] = true;

        s_crank_change = plat_in->crank_change;
        if (s_crank_change != 0.0f) {
            s_crank_accum += s_crank_change;
            while (s_crank_accum >= 24.0f) {
                s_action_held[SIM_ACT_NEXT_TOOL] = true;
                s_crank_accum -= 24.0f;
            }
            while (s_crank_accum <= -24.0f) {
                s_action_held[SIM_ACT_PREV_TOOL] = true;
                s_crank_accum += 24.0f;
            }
        }

        /* Mouse Position (scale down 2x from 800x480 desktop window to 400x240 virtual canvas) */
        s_mouse.pos.x = (int)(plat_in->mouse_pos.x * 0.5f);
        s_mouse.pos.y = (int)(plat_in->mouse_pos.y * 0.5f);
        s_mouse.left_down = plat_in->mouse_down;
    }

#if !defined(PLAYDATE)
    /* 3. Direct SDL2 Polling on Desktop (Full keyboard & multi-button mouse) */
    int numkeys = 0;
    const Uint8 *keys = SDL_GetKeyboardState(&numkeys);
    if (keys) {
        /* Arrow Keys & Numpad for navigation */
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_KP_8])    s_action_held[SIM_ACT_UP] = true;
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_KP_2])  s_action_held[SIM_ACT_DOWN] = true;
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_KP_4])  s_action_held[SIM_ACT_LEFT] = true;
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_KP_6]) s_action_held[SIM_ACT_RIGHT] = true;

        /* Confirm / Primary */
        if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER] || keys[SDL_SCANCODE_SPACE]) {
            s_action_held[SIM_ACT_PRIMARY] = true;
        }

        /* Cancel / Secondary */
        if (keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE]) {
            s_action_held[SIM_ACT_SECONDARY] = true;
        }

        /* Menu & Views */
        if (keys[SDL_SCANCODE_ESCAPE]) s_action_held[SIM_ACT_MENU] = true;
        if (keys[SDL_SCANCODE_M])      s_action_held[SIM_ACT_MINIMAP] = true;
        if (keys[SDL_SCANCODE_TAB])    s_action_held[SIM_ACT_OVERLAY] = true;
        if (keys[SDL_SCANCODE_U])      s_action_held[SIM_ACT_BUDGET] = true;
        if (keys[SDL_SCANCODE_V])      s_action_held[SIM_ACT_EVAL] = true;
        if (keys[SDL_SCANCODE_X])      s_action_held[SIM_ACT_DISASTERS] = true;
        if (keys[SDL_SCANCODE_G])      s_action_held[SIM_ACT_SCENARIOS] = true;

        /* Speeds */
        if (keys[SDL_SCANCODE_1]) s_action_held[SIM_ACT_SPEED_PAUSE] = true;
        if (keys[SDL_SCANCODE_2]) s_action_held[SIM_ACT_SPEED_SLOW] = true;
        if (keys[SDL_SCANCODE_3]) s_action_held[SIM_ACT_SPEED_NORM] = true;
        if (keys[SDL_SCANCODE_4]) s_action_held[SIM_ACT_SPEED_FAST] = true;

        /* Tool Cycling */
        if (keys[SDL_SCANCODE_LEFTBRACKET] || keys[SDL_SCANCODE_PAGEUP])   s_action_held[SIM_ACT_PREV_TOOL] = true;
        if (keys[SDL_SCANCODE_RIGHTBRACKET] || keys[SDL_SCANCODE_PAGEDOWN]) s_action_held[SIM_ACT_NEXT_TOOL] = true;

        /* Classic direct tool hotkeys */
        if (keys[SDL_SCANCODE_R]) s_action_held[SIM_ACT_TOOL_ROAD] = true;
        if (keys[SDL_SCANCODE_W]) s_action_held[SIM_ACT_TOOL_WIRE] = true;
        if (keys[SDL_SCANCODE_B] || keys[SDL_SCANCODE_D]) s_action_held[SIM_ACT_TOOL_DOZER] = true;
        if (keys[SDL_SCANCODE_Z]) s_action_held[SIM_ACT_TOOL_RES] = true;
        if (keys[SDL_SCANCODE_C]) s_action_held[SIM_ACT_TOOL_COM] = true;
        if (keys[SDL_SCANCODE_I]) s_action_held[SIM_ACT_TOOL_IND] = true;
        if (keys[SDL_SCANCODE_F]) s_action_held[SIM_ACT_TOOL_FIRE] = true;
        if (keys[SDL_SCANCODE_O]) s_action_held[SIM_ACT_TOOL_POLICE] = true;
        if (keys[SDL_SCANCODE_S]) s_action_held[SIM_ACT_TOOL_STAD] = true;
        if (keys[SDL_SCANCODE_P]) s_action_held[SIM_ACT_TOOL_PARK] = true;
        if (keys[SDL_SCANCODE_T]) s_action_held[SIM_ACT_TOOL_PORT] = true;
        if (keys[SDL_SCANCODE_L]) s_action_held[SIM_ACT_TOOL_COAL] = true;
        if (keys[SDL_SCANCODE_N]) s_action_held[SIM_ACT_TOOL_NUKE] = true;
        if (keys[SDL_SCANCODE_A]) s_action_held[SIM_ACT_TOOL_AIR] = true;
        if (keys[SDL_SCANCODE_Q]) s_action_held[SIM_ACT_TOOL_QUERY] = true;
    }

    /* Check SDL Mouse Buttons for Right and Middle click */
    Uint32 mstate = SDL_GetMouseState(NULL, NULL);
    s_mouse.right_down = (mstate & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    s_mouse.middle_down = (mstate & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
#endif

    /* 4. Mouse Edge Detection */
    s_mouse.left_pressed = (s_mouse.left_down && !s_prev_mouse_left);
    s_mouse.left_released = (!s_mouse.left_down && s_prev_mouse_left);
    s_prev_mouse_left = s_mouse.left_down;

    s_mouse.right_pressed = (s_mouse.right_down && !s_prev_mouse_right);
    s_mouse.right_released = (!s_mouse.right_down && s_prev_mouse_right);
    s_prev_mouse_right = s_mouse.right_down;

    /* Swallow buttons carried over from the previous scene until released */
    if (s_wait_release) {
        bool any = false;
        for (int i = 0; i < SIM_ACT_COUNT; i++) any = any || s_action_held[i];
        if (any) {
            memset(s_action_held, 0, sizeof(s_action_held));
        } else {
            s_wait_release = false;
        }
    }

    /* 5. Compute Pressed & Released Edges for Actions */
    for (int i = 0; i < SIM_ACT_COUNT; i++) {
        s_action_pressed[i] = (s_action_held[i] && !s_action_prev[i]);
        s_action_released[i] = (!s_action_held[i] && s_action_prev[i]);
    }
}

bool sim_action_held(sim_action_t act)
{
    if (act < 0 || act >= SIM_ACT_COUNT) return false;
    return s_action_held[act];
}

bool sim_action_pressed(sim_action_t act)
{
    if (act < 0 || act >= SIM_ACT_COUNT) return false;
    return s_action_pressed[act];
}

bool sim_action_released(sim_action_t act)
{
    if (act < 0 || act >= SIM_ACT_COUNT) return false;
    return s_action_released[act];
}

const sim_mouse_t *sim_input_get_mouse(void)
{
    return &s_mouse;
}

float sim_input_get_crank_change(void)
{
    return s_crank_change;
}

void sim_input_trigger_action(sim_action_t act)
{
    if (act >= 0 && act < SIM_ACT_COUNT) {
        s_action_triggered[act] = true;
    }
}

