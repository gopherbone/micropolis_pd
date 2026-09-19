#ifndef SIM_INPUT_H
#define SIM_INPUT_H

#include <stdbool.h>
#include "types.h"

typedef enum {
    SIM_ACT_NONE = 0,
    SIM_ACT_UP,
    SIM_ACT_DOWN,
    SIM_ACT_LEFT,
    SIM_ACT_RIGHT,
    SIM_ACT_PRIMARY,    /* A Button / Space / Left Click (Confirm / Place) */
    SIM_ACT_SECONDARY,  /* B Button / Esc / Right Click (Cancel / Query) */
    SIM_ACT_MINIMAP,    /* Toggle Minimap ('M', Select) */
    SIM_ACT_OVERLAY,    /* Cycle Overlay ('Tab') */
    SIM_ACT_SPEED,      /* Cycle Speed ('1'..'4', Space) */
    SIM_ACT_MENU,       /* System / Pause Menu (Start, Esc, Enter) */
    SIM_ACT_PREV_TOOL,  /* Cycle Tool Left ('[', L-Shoulder, Crank CCW) */
    SIM_ACT_NEXT_TOOL,  /* Cycle Tool Right (']', R-Shoulder, Crank CW) */

    /* Tool Direct Hotkeys */
    SIM_ACT_TOOL_ROAD,   /* 'R' */
    SIM_ACT_TOOL_WIRE,   /* 'W' */
    SIM_ACT_TOOL_DOZER,  /* 'B' or 'D' */
    SIM_ACT_TOOL_RES,    /* 'Z' */
    SIM_ACT_TOOL_COM,    /* 'C' */
    SIM_ACT_TOOL_IND,    /* 'I' */
    SIM_ACT_TOOL_FIRE,   /* 'F' */
    SIM_ACT_TOOL_POLICE, /* 'O' */
    SIM_ACT_TOOL_STAD,   /* 'S' */
    SIM_ACT_TOOL_PARK,   /* 'P' */
    SIM_ACT_TOOL_PORT,   /* 'T' */
    SIM_ACT_TOOL_COAL,   /* 'L' */
    SIM_ACT_TOOL_NUKE,   /* 'N' */
    SIM_ACT_TOOL_AIR,    /* 'A' */
    SIM_ACT_TOOL_QUERY,  /* 'Q' */

    /* Direct Modal Hotkeys */
    SIM_ACT_BUDGET,      /* 'U' */
    SIM_ACT_EVAL,        /* 'V' */
    SIM_ACT_DISASTERS,   /* 'X' */
    SIM_ACT_SCENARIOS,   /* 'G' */

    /* Speeds */
    SIM_ACT_SPEED_PAUSE, /* '1' */
    SIM_ACT_SPEED_SLOW,  /* '2' */
    SIM_ACT_SPEED_NORM,  /* '3' */
    SIM_ACT_SPEED_FAST,  /* '4' */

    SIM_ACT_COUNT
} sim_action_t;

typedef struct {
    vec2i_t pos;           /* Screen coordinates in 400x240 virtual canvas */
    bool left_down;
    bool left_pressed;
    bool left_released;
    bool right_down;
    bool right_pressed;
    bool right_released;
    bool middle_down;
    float wheel_delta;
} sim_mouse_t;

void sim_input_init(void);
void sim_input_poll(float dt);

bool sim_action_held(sim_action_t act);
bool sim_action_pressed(sim_action_t act);
bool sim_action_released(sim_action_t act);

const sim_mouse_t *sim_input_get_mouse(void);
float sim_input_get_crank_change(void);
void sim_input_trigger_action(sim_action_t act);

#endif /* SIM_INPUT_H */
