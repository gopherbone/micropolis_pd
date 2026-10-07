#ifndef TINY_SOUND_H
#define TINY_SOUND_H

void sound_init(int sample_rate);
void sound_cleanup(void);
void sound_play_sound(const char *path, float volume);

#endif
