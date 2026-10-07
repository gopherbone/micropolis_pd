#include <stddef.h>

#include "scene.h"

static scene_t *s_current = NULL;
static scene_t *s_pending = NULL;

void scene_manager_init(void)
{
    s_current = NULL;
    s_pending = NULL;
}

/* Switches are deferred to the next update so a scene can call scene_set()
 * from inside its own update without being torn down mid-frame. */
void scene_set(scene_t *scene)
{
    s_pending = scene;
}

scene_t *scene_get_current(void)
{
    return s_pending ? s_pending : s_current;
}

static void apply_pending(void)
{
    if (!s_pending) return;
    scene_t *next = s_pending;
    s_pending = NULL;

    if (s_current && s_current != next) {
        if (s_current->vtab && s_current->vtab->cleanup) s_current->vtab->cleanup(s_current);
        s_current->is_initialized = false;
    }
    s_current = next;
    if (!s_current->is_initialized) {
        if (s_current->vtab && s_current->vtab->init) s_current->vtab->init(s_current);
        s_current->is_initialized = true;
    }
}

void scene_update(float dt)
{
    apply_pending();
    if (s_current && s_current->vtab && s_current->vtab->update) {
        s_current->vtab->update(s_current, dt);
    }
    apply_pending();
}

void scene_draw(void)
{
    if (s_current && s_current->vtab && s_current->vtab->draw) {
        s_current->vtab->draw(s_current);
    }
}
