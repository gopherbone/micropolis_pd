/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#ifndef TINY_SOUND_H
#define TINY_SOUND_H

void sound_init(int sample_rate);
void sound_cleanup(void);
void sound_play_sound(const char *path, float volume);

#endif
