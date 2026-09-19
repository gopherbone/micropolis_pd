#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "sim.h"
#include "sound.h"

/* Sound routines */

int SoundInitialized = 0;
short Dozing = 0;

static const struct {
    const char *token;
    const char *path;
} s_sound_files[] = {
    { "explosion-high", "assets/sounds/explosion-high.wav" },
    { "explosion-low",  "assets/sounds/explosion-low.wav" },
    { "explosion-hi",   "assets/sounds/explosion-hi.wav" },
    { "heavytraffic",   "assets/sounds/heavytraffic.wav" },
    { "honkhonk-high",  "assets/sounds/honkhonk-high.wav" },
    { "honkhonk-low",   "assets/sounds/honkhonk-low.wav" },
    { "honkhonk-med",   "assets/sounds/honkhonk-med.wav" },
    { "monster",        "assets/sounds/monster.wav" },
    { "uhuh",           "assets/sounds/uhuh.wav" },
    { "sorry",          "assets/sounds/sorry.wav" },
    { "siren",          "assets/sounds/siren.wav" },
    { "fire",           "assets/sounds/fire.wav" },
    { "build",          "assets/sounds/build.wav" },
    { "bulldozer",      "assets/sounds/bulldozer.wav" },
    { "1",              "assets/sounds/bulldozer.wav" },
    { "rumble",         "assets/sounds/rumble.wav" },
    { "skid",           "assets/sounds/skid.wav" },
    { "chalk",          "assets/sounds/chalk.wav" },
    { "eraser",         "assets/sounds/eraser.wav" },
    { "boing",          "assets/sounds/boing.wav" },
    { "bop",            "assets/sounds/bop.wav" },
    { "beep",           "assets/sounds/beep.wav" },
    { "cuckoo",         "assets/sounds/cuckoo.wav" },
    { "computer",       "assets/sounds/computer.wav" },
    { "ignition",       "assets/sounds/ignition.wav" },
    { "road",           "assets/sounds/road.wav" },
    { "wire",           "assets/sounds/wire.wav" },
    { "park",           "assets/sounds/park.wav" },
    { "res",            "assets/sounds/res.wav" },
    { "com",            "assets/sounds/com.wav" },
    { "ind",            "assets/sounds/ind.wav" },
    { "police",         "assets/sounds/police.wav" },
    { "stadium",        "assets/sounds/stadium.wav" },
    { "seaport",        "assets/sounds/seaport.wav" },
    { "coal",           "assets/sounds/coal.wav" },
    { "nuclear",        "assets/sounds/nuclear.wav" },
    { "airport",        "assets/sounds/airport.wav" },
    { "query",          "assets/sounds/query.wav" },
    { "zone",           "assets/sounds/zone.wav" }
};
#define SOUND_COUNT ((int)(sizeof(s_sound_files) / sizeof(s_sound_files[0])))

void InitializeSound(void)
{
    SoundInitialized = 1;
}

void ShutDownSound(void)
{
    SoundInitialized = 0;
}

static const char *resolve_sound_path(const char *id)
{
    if (!id || !id[0]) return NULL;

    /* Extract the first word from id (e.g. "HonkHonk-Low -speed 80" -> "honkhonk-low") */
    char token[64];
    int i = 0;
    while (id[i] && !isspace((unsigned char)id[i]) && i < (int)sizeof(token) - 1) {
        token[i] = tolower((unsigned char)id[i]);
        i++;
    }
    token[i] = '\0';

    for (int j = 0; j < SOUND_COUNT; j++) {
        if (strcmp(token, s_sound_files[j].token) == 0) {
            return s_sound_files[j].path;
        }
    }
    return NULL;
}

void MakeSound(char *channel, char *id)
{
    (void)channel;
    if (!UserSoundOn) return;
    if (!SoundInitialized) InitializeSound();

    const char *path = resolve_sound_path(id);
    if (path) {
        sound_play_sound(path, 1.0f);
    }
}

void MakeSoundOn(SimView *view, char *channel, char *id)
{
    (void)view;
    MakeSound(channel, id);
}

void StartBulldozer(void)
{
    if (!UserSoundOn) return;
    if (!SoundInitialized) InitializeSound();
    if (!Dozing) {
        sound_play_sound("assets/sounds/bulldozer.wav", 0.9f);
        Dozing = 1;
    }
}

void StopBulldozer(void)
{
    if (!UserSoundOn || !SoundInitialized) return;
    Dozing = 0;
}

void SoundOff(void)
{
    Dozing = 0;
}

void DoStartSound(char *channel, char *id)
{
    MakeSound(channel, id);
}

void DoStopSound(char *id)
{
    (void)id;
    Dozing = 0;
}
