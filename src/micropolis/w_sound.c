/* w_sound.c
 *
 * Micropolis, Unix Version.  This game was released for the Unix platform
 * in or about 1990 and has been modified for inclusion in the One Laptop
 * Per Child program.  Copyright (C) 1989 - 2007 Electronic Arts Inc.  If
 * you need assistance with this program, you may contact:
 *   http://wiki.laptop.org/go/Micropolis  or email  micropolis@laptop.org.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.  You should have received a
 * copy of the GNU General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 * 
 *             ADDITIONAL TERMS per GNU GPL Section 7
 * 
 * No trademark or publicity rights are granted.  This license does NOT
 * give you any right, title or interest in the trademark SimCity or any
 * other Electronic Arts trademark.  You may not distribute any
 * modification of this program using the trademark SimCity or claim any
 * affliation or association with Electronic Arts Inc. or its employees.
 * 
 * Any propagation or conveyance of this program must include this
 * copyright notice and these terms.
 * 
 * If you convey this program (or any modifications of it) and assume
 * contractual liability for the program to recipients of it, you agree
 * to indemnify Electronic Arts for any liability that those contractual
 * assumptions impose on Electronic Arts.
 * 
 * You may not misrepresent the origins of this program; modified
 * versions of the program must be marked as such and not identified as
 * the original program.
 * 
 * This disclaimer supplements the one included in the General Public
 * License.  TO THE FULLEST EXTENT PERMISSIBLE UNDER APPLICABLE LAW, THIS
 * PROGRAM IS PROVIDED TO YOU "AS IS," WITH ALL FAULTS, WITHOUT WARRANTY
 * OF ANY KIND, AND YOUR USE IS AT YOUR SOLE RISK.  THE ENTIRE RISK OF
 * SATISFACTORY QUALITY AND PERFORMANCE RESIDES WITH YOU.  ELECTRONIC ARTS
 * DISCLAIMS ANY AND ALL EXPRESS, IMPLIED OR STATUTORY WARRANTIES,
 * INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY, SATISFACTORY QUALITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS, AND WARRANTIES (IF ANY) ARISING FROM A COURSE OF DEALING,
 * USAGE, OR TRADE PRACTICE.  ELECTRONIC ARTS DOES NOT WARRANT AGAINST
 * INTERFERENCE WITH YOUR ENJOYMENT OF THE PROGRAM; THAT THE PROGRAM WILL
 * MEET YOUR REQUIREMENTS; THAT OPERATION OF THE PROGRAM WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT THE PROGRAM WILL BE COMPATIBLE
 * WITH THIRD PARTY SOFTWARE OR THAT ANY ERRORS IN THE PROGRAM WILL BE
 * CORRECTED.  NO ORAL OR WRITTEN ADVICE PROVIDED BY ELECTRONIC ARTS OR
 * ANY AUTHORIZED REPRESENTATIVE SHALL CREATE A WARRANTY.  SOME
 * JURISDICTIONS DO NOT ALLOW THE EXCLUSION OF OR LIMITATIONS ON IMPLIED
 * WARRANTIES OR THE LIMITATIONS ON THE APPLICABLE STATUTORY RIGHTS OF A
 * CONSUMER, SO SOME OR ALL OF THE ABOVE EXCLUSIONS AND LIMITATIONS MAY
 * NOT APPLY TO YOU.
 */

/* Modified version (GPL v3 section 5a / additional terms): this file is not
 * the original Micropolis source. It was converted to C99 for vtcity by
 * tenox7, adapted for Tiny Engine by icedman (tiny_micropolis), and changed
 * further for the Playdate in 2026 by micropolis_pd. See NOTICE.md. */

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
