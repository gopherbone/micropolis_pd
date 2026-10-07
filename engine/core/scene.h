/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#ifndef TINY_SCENE_H
#define TINY_SCENE_H

#include "types.h"

typedef struct scene_t scene_t;

typedef struct scene_vtab_t {
    void (*init)(scene_t *self);
    void (*update)(scene_t *self, float dt);
    void (*draw)(scene_t *self);
    void (*cleanup)(scene_t *self);
} scene_vtab_t;

struct scene_t {
    const scene_vtab_t *vtab;
    void *user_data;
    bool is_initialized;
};

void scene_manager_init(void);
void scene_set(scene_t *scene);
scene_t *scene_get_current(void);
void scene_update(float dt);
void scene_draw(void);

#endif
