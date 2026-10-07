#ifndef TINY_PD_SHIM_H
#define TINY_PD_SHIM_H

#include "pd_api.h"

extern PlaydateAPI *g_pd;

/* Temporary clip intersected with the clip set via render_set_clip() */
void shim_push_clip(int x, int y, int w, int h);
void shim_pop_clip(void);

/* Size of the current render target (screen or texture) */
void shim_target_size(int *w, int *h);

#endif
