/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
#include <stdlib.h>
#include <string.h>

#include "sound.h"
#include "pd_shim.h"

#define MAX_SAMPLES 64
#define MAX_VOICES 6

typedef struct {
    char path[96];
    AudioSample *sample; /* NULL if the file is missing (cached miss) */
} sample_entry_t;

static sample_entry_t s_samples[MAX_SAMPLES];
static int s_sample_count = 0;
static SamplePlayer *s_voices[MAX_VOICES];
static int s_next_voice = 0;
static bool s_ready = false;

void sound_init(int sample_rate)
{
    (void)sample_rate;
    for (int i = 0; i < MAX_VOICES; i++) {
        s_voices[i] = g_pd->sound->sampleplayer->newPlayer();
    }
    s_ready = true;
}

void sound_cleanup(void)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (s_voices[i]) g_pd->sound->sampleplayer->freePlayer(s_voices[i]);
        s_voices[i] = NULL;
    }
    for (int i = 0; i < s_sample_count; i++) {
        if (s_samples[i].sample) g_pd->sound->sample->freeSample(s_samples[i].sample);
    }
    s_sample_count = 0;
    s_ready = false;
}

static AudioSample *get_sample(const char *path)
{
    for (int i = 0; i < s_sample_count; i++) {
        if (strcmp(s_samples[i].path, path) == 0) return s_samples[i].sample;
    }
    if (s_sample_count >= MAX_SAMPLES) return NULL;

    /* pdc compiles .wav to .pda; load by base name */
    char base[96];
    strncpy(base, path, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    sample_entry_t *e = &s_samples[s_sample_count++];
    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';
    e->sample = g_pd->sound->sample->load(base);
    return e->sample;
}

void sound_play_sound(const char *path, float volume)
{
    if (!s_ready || !path) return;
    AudioSample *sample = get_sample(path);
    if (!sample) return;

    /* Prefer an idle voice; otherwise steal round-robin */
    int v = -1;
    for (int i = 0; i < MAX_VOICES; i++) {
        int idx = (s_next_voice + i) % MAX_VOICES;
        if (s_voices[idx] && !g_pd->sound->sampleplayer->isPlaying(s_voices[idx])) {
            v = idx;
            break;
        }
    }
    if (v < 0) v = s_next_voice;
    s_next_voice = (v + 1) % MAX_VOICES;

    SamplePlayer *p = s_voices[v];
    if (!p) return;
    g_pd->sound->sampleplayer->stop(p);
    g_pd->sound->sampleplayer->setSample(p, sample);
    g_pd->sound->sampleplayer->setVolume(p, volume, volume);
    g_pd->sound->sampleplayer->play(p, 1, 1.0f);
}
