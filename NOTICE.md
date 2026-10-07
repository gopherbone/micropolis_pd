# Notices

This repository is a **modified version** of Micropolis. It is not the original program and is not affiliated with or endorsed by Electronic Arts, Maxis, Micropolis GmbH or Panic.

- **Code license:** GNU General Public License, version 3 or later ([COPYING](COPYING)), with the additional terms from Electronic Arts reproduced below.
- **Name license:** the name "Micropolis" is used under the [Micropolis Public Name License](MicropolisPublicNameLicense.md).

## Where the code comes from

| Step | Project | What it contributed |
|---|---|---|
| 1 | **Micropolis** (Unix/OLPC release). Copyright © 1989–2007 Electronic Arts Inc. Released as free software in 2008 for the One Laptop Per Child program, through the work of Don Hopkins. Source: [SimHacker/micropolis](https://github.com/SimHacker/micropolis) `micropolis-activity/src/sim` (checked at commit `c98f6b085198`). | The simulation and the original game data: tiles, sprites, sounds, scenarios and example cities. |
| 2 | **[vtcity](https://github.com/tenox7/vtcity)** by tenox7 (GPL v3 or later; checked at commit `9aa2e9965f62`). | Simulation code extracted from the X11/Tcl front end and converted to C99, for DEC VT terminals. |
| 3 | **[tiny_micropolis](https://github.com/icedman/tiny_micropolis)** by icedman (this repository's history up to commit `0646c0c`). | The vtcity simulation moved onto icedman's Tiny Engine, with an SDL/Playdate front end and save slots. |
| 4 | **micropolis_pd**, this repository (2026). | A Playdate engine layer, a new interface, fixes and the build tooling, described below. |

Every file in `src/micropolis/` derives from the step 1 source. vtcity and tiny_micropolis had removed the original copyright notice from most of these files; in 2026 it was restored to each one from the original source, and each file is marked as modified.

### Changes in this repository (step 4)

- `engine/`: a new Playdate C API layer, compatible with the parts of Tiny Engine the game uses.
- `src/game.c`, `src/screens.c`, `src/scene_title.c`, `src/ui.c`, `src/map_view.c`, `src/sim_input.c`, `src/main_playdate.c`: a rewritten Playdate interface.
- `src/micropolis/`:
  - a fix for a stack overflow when saving (`s_fileio.c`),
  - correct populations in save slots (`s_fileio.c`),
  - loading a city no longer changes the sound setting (`s_fileio.c`),
  - restored license headers.
- `tools/`: the asset pipeline, tool icons and a screenshot harness.

## Original copyright notice and additional terms

The following notice is reproduced from the original source, as its terms require:

```
Micropolis, Unix Version.  This game was released for the Unix platform
in or about 1990 and has been modified for inclusion in the One Laptop
Per Child program.  Copyright (C) 1989 - 2007 Electronic Arts Inc.  If
you need assistance with this program, you may contact:
  http://wiki.laptop.org/go/Micropolis  or email  micropolis@laptop.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.  You should have received a
copy of the GNU General Public License along with this program.  If
not, see <http://www.gnu.org/licenses/>.

            ADDITIONAL TERMS per GNU GPL Section 7

No trademark or publicity rights are granted.  This license does NOT
give you any right, title or interest in the trademark SimCity or any
other Electronic Arts trademark.  You may not distribute any
modification of this program using the trademark SimCity or claim any
affliation or association with Electronic Arts Inc. or its employees.

Any propagation or conveyance of this program must include this
copyright notice and these terms.

If you convey this program (or any modifications of it) and assume
contractual liability for the program to recipients of it, you agree
to indemnify Electronic Arts for any liability that those contractual
assumptions impose on Electronic Arts.

You may not misrepresent the origins of this program; modified
versions of the program must be marked as such and not identified as
the original program.

This disclaimer supplements the one included in the General Public
License.  TO THE FULLEST EXTENT PERMISSIBLE UNDER APPLICABLE LAW, THIS
PROGRAM IS PROVIDED TO YOU "AS IS," WITH ALL FAULTS, WITHOUT WARRANTY
OF ANY KIND, AND YOUR USE IS AT YOUR SOLE RISK.  THE ENTIRE RISK OF
SATISFACTORY QUALITY AND PERFORMANCE RESIDES WITH YOU.  ELECTRONIC ARTS
DISCLAIMS ANY AND ALL EXPRESS, IMPLIED OR STATUTORY WARRANTIES,
INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY, SATISFACTORY QUALITY,
FITNESS FOR A PARTICULAR PURPOSE, NONINFRINGEMENT OF THIRD PARTY
RIGHTS, AND WARRANTIES (IF ANY) ARISING FROM A COURSE OF DEALING,
USAGE, OR TRADE PRACTICE.  ELECTRONIC ARTS DOES NOT WARRANT AGAINST
INTERFERENCE WITH YOUR ENJOYMENT OF THE PROGRAM; THAT THE PROGRAM WILL
MEET YOUR REQUIREMENTS; THAT OPERATION OF THE PROGRAM WILL BE
UNINTERRUPTED OR ERROR-FREE, OR THAT THE PROGRAM WILL BE COMPATIBLE
WITH THIRD PARTY SOFTWARE OR THAT ANY ERRORS IN THE PROGRAM WILL BE
CORRECTED.  NO ORAL OR WRITTEN ADVICE PROVIDED BY ELECTRONIC ARTS OR
ANY AUTHORIZED REPRESENTATIVE SHALL CREATE A WARRANTY.  SOME
JURISDICTIONS DO NOT ALLOW THE EXCLUSION OF OR LIMITATIONS ON IMPLIED
WARRANTIES OR THE LIMITATIONS ON THE APPLICABLE STATUTORY RIGHTS OF A
CONSUMER, SO SOME OR ALL OF THE ABOVE EXCLUSIONS AND LIMITATIONS MAY
NOT APPLY TO YOU.
```

## Trademarks

- **Micropolis** is a registered trademark of Micropolis Corporation (Micropolis GmbH) and is licensed here as a courtesy of the owner ([micropolis.com](https://micropolis.com)). The name is used under the terms of the [Micropolis Public Name License](MicropolisPublicNameLicense.md), which is included in this repository as that license requires.
- **SimCity** is a trademark of Electronic Arts Inc. No rights to it are claimed, and this project is not associated with Electronic Arts.
- **Playdate** is a trademark of Panic Inc.

## Fonts and button glyphs

The build pipeline (`tools/prep_assets.py`) converts these from the Playdate SDK into the game's font atlases at build time. They are not stored in this repository.

- **Main font:** Nontendo, by Shaun Inman.
- **Borrowed symbols:** Pedallica, Newsleak Serif and Bitmore, for symbols Nontendo lacks.
- **Button glyphs:** Asheville Sans, Panic's system font.

They are used under the Playdate SDK license, which covers distributing them as part of a Playdate game.
