/* w_resrc.c:  Get resources (from files)
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

#include "sim.h"
#include <string.h>
#include "res_data.h"		/* baked-in stri.* / snro.* resources */


char *HomeDir, *ResourceDir, *KeyDir, *HostName;

struct Resource *Resources = NULL;

/* Find an embedded resource by exact basename ("stri.301", "snro.111"). */
const unsigned char *
res_find(const char *name, unsigned int *size)
{
  int i;
  for (i = 0; i < RES_COUNT; i++) {
    if (strcmp(res_table[i].name, name) == 0) {
      if (size) *size = res_table[i].size;
      return res_table[i].data;
    }
  }
  if (size) *size = 0;
  return NULL;
}

/* Find an embedded example city by exact basename ("about.cty"). */
const unsigned char *
city_find(const char *name, unsigned int *size)
{
  int i;
  for (i = 0; i < CITY_COUNT; i++) {
    if (strcmp(city_table[i].name, name) == 0) {
      if (size) *size = city_table[i].size;
      return city_table[i].data;
    }
  }
  if (size) *size = 0;
  return NULL;
}

/* Enumerate the embedded cities (for the built-in load picker). */
int
EmbeddedCityCount(void)
{
  return CITY_COUNT;
}

const char *
EmbeddedCityName(int i)
{
  return (i >= 0 && i < CITY_COUNT) ? city_table[i].name : NULL;
}

struct StringTable {
  QUAD id;
  int lines;
  char **strings;
  struct StringTable *next;
} *StringTables;


Handle GetResource(char *name, QUAD id)
{
  struct Resource *r = Resources;
  char key[16];
  const unsigned char *data;
  unsigned int size;

  while (r != NULL) {
    if ((r->id == id) &&
	(strncmp(r->name, name, 4) == 0)) {
      return ((Handle)&r->buf);
    }
    r = r->next;
  }

  /* resources are baked into the binary (res_data.h); look up by basename */
  sprintf(key, "%c%c%c%c.%d", name[0], name[1], name[2], name[3], (int)id);
  data = res_find(key, &size);
  if ((data == NULL) || (size == 0))
    return (NULL);		/* missing: caller (GetIndString) degrades */

  r = (struct Resource *)ckalloc(sizeof(struct Resource));
  r->name[0] = name[0];
  r->name[1] = name[1];
  r->name[2] = name[2];
  r->name[3] = name[3];
  r->id = id;
  r->size = size;

  /* hand back a MUTABLE copy: GetIndString rewrites '\n' -> '\0' in place,
   * so we must never expose the const baked-in bytes directly. */
  r->buf = (char *)ckalloc(size);
  memcpy(r->buf, data, size);
  r->next = Resources; Resources = r;
  return ((Handle)&r->buf);
}


void
ReleaseResource(Handle r)
{
}


QUAD
ResourceSize(Handle h)
{
  struct Resource *r = (struct Resource *)h;

  return (r->size);
}


char *
ResourceName(Handle h)
{
  struct Resource *r = (struct Resource *)h;

  return (r->name);
}


QUAD
ResourceID(Handle h)
{
  struct Resource *r = (struct Resource *)h;

  return (r->id);
}


void
GetIndString(char *str, int id, short num)
{
  struct StringTable **tp, *st = NULL;
  Handle h;

  tp = &StringTables;

  while (*tp) {
    if ((*tp)->id == id) {
      st = *tp;
      break;
    }
    tp = &((*tp)->next);
  }
  if (!st) {
    QUAD i, lines, size;
    char *buf;

    st = (struct StringTable *)ckalloc(sizeof (struct StringTable));
    st->id = id;
    h = GetResource("stri", id);
    if (h == NULL) {		/* resource file missing: degrade, don't crash */
      ckfree((char *)st);
      strcpy(str, "");
      return;
    }
    size = ResourceSize(h);
    buf = (char *)*h;
    for (i=0, lines=0; i<size; i++)
      if (buf[i] == '\n') {
	buf[i] = 0;
	lines++;
      }
    st->lines = lines;
    st->strings = (char **)ckalloc(size * sizeof(char *));
    for (i=0; i<lines; i++) {
      st->strings[i] = buf;
      buf += strlen(buf) + 1;
    }
    st->next = StringTables;
    StringTables = st;
  }
  if ((num < 1) || (num > st->lines)) {
    strcpy(str, "");		/* ncurses port: silent (was a stderr print) */
  } {
    strcpy(str, st->strings[num-1]);
  }
}
