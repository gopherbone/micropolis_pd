/* sim_stubs.c:  Stubs replacing the original X11/Tcl user interface hooks
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

/* sim_stubs.c -- Engine stubs, lifecycle globals, and UI hooks.
 *
 * Replaces the discarded X11/Tk and DEC VT terminal front-ends with clean,
 * modular stubs and state variables for Tiny Engine integration.
 */

#include "sim.h"

/* ---- Engine Lifecycle & Version Globals ---- */

char *MicropolisVersion = "4.0";
Sim *sim = NULL;
int sim_loops = 0;
int sim_delay = 50;
int sim_skips = 0;
int sim_skip = 0;
int sim_paused = 0;
int sim_paused_speed = 3;
int sim_tty = 0;
int heat_steps = 0;
int heat_flow = -7;
int heat_rule = 0;
int heat_wrap = 3;
struct timeval start_time, now_time, beat_time, last_now_time;
char *CityFileName = NULL;
int Startup = 0;
int StartupGameLevel = 0;
char *StartupName = NULL;
int WireMode = 0;
int MultiPlayerMode = 0;
int TilesAnimated = 0;
int DoAnimation = 1;
int DoMessages = 1;
int DoNotices = 1;
char *Displays = NULL;
char *FirstDisplay = NULL;
int ExitReturn = 0;
int tkMustExit = 0;

QUAD TotalFunds = 5000;
short PunishCnt = 0;
short autoBulldoze = 1;
short autoBudget = 1;
QUAD LastMesTime = 0;
short GameLevel = 0;
short InitSimLoad = 2;
short ScenarioID = 0;
short SimSpeed = 1;
short SimMetaSpeed = 0;
short UserSoundOn = 1;
char *CityName = NULL;
short NoDisasters = 0;
short MesNum = 0;
short EvalChanged = 0;
short flagBlink = 1;

short Graph10Max = 0;
short Graph120Max = 0;
short NewGraph = 0;
int UpdateDelayed = 0;

/* ---- Lifecycle Functions ---- */

void sim_exit(int val)
{
  tkMustExit = 1;
  ExitReturn = val;
}

void Kick(void) { }
void UpdateFlush(void) { }
void DoTimeoutListen(void) { }
void DoStopMicropolis(void) { }
void StopToolkit(void) { }

/* ---- Funds & Spend ---- */

void Spend(int dollars)
{
  SetFunds(TotalFunds - dollars);
}

void SetFunds(int dollars)
{
  TotalFunds = dollars;
  UpdateFunds();
}

/* ---- Mac Emulation Helpers ---- */

QUAD TickCount(void)
{
  struct timeval time;
  gettimeofday(&time, 0);
  return (QUAD)((time.tv_sec / 60) + (time.tv_usec * 1000000 / 60));
}

Ptr NewPtr(int size)
{
  return ((Ptr)calloc(size, sizeof(Byte)));
}

/* ---- High-level Game Flow Handlers ---- */

void GameStarted(void)
{
  InvalidateMaps();
  InvalidateEditors();
  gettimeofday(&start_time, NULL);

  switch (Startup) {
  case -2: /* Load a city */
    if (LoadCity(StartupName)) {
      DoStartLoad();
      StartupName = NULL;
      break;
    }
    StartupName = NULL;
    /* fallthrough */
  case -1:
    if (StartupName != NULL) {
      setCityName(StartupName);
      StartupName = NULL;
    } else {
      setCityName("NowHere");
    }
    DoPlayNewCity();
    break;
  case 0:
    DoReallyStartGame();
    break;
  default: /* scenario number */
    DoStartScenario(Startup);
    break;
  }
}

void DoPlayNewCity(void)
{
  Eval("UIPlayNewCity");
}

void DoReallyStartGame(void)
{
  Eval("UIReallyStartGame");
}

void DoStartLoad(void)
{
  Eval("UIStartLoad");
}

void DoStartScenario(int scenario)
{
  char buf[256];
  sprintf(buf, "UIStartScenario %d", scenario);
  Eval(buf);
}

void DropFireBombs(void)
{
  Eval("DropFireBombs");
}

void InitGame(void)
{
  sim_skips = sim_skip = sim_paused = sim_paused_speed = heat_steps = 0;
  setSpeed(0);
}

void ReallyQuit(void)
{
  sim_exit(0);
}

/* ---- View Invalidation & Redraw Stubs ---- */

void InvalidateMaps(void) { }
void InvalidateEditors(void) { }
void RedrawMaps(void) { }
void RedrawEditors(void) { }
void EventuallyRedrawView(SimView *view) { (void)view; }
void CancelRedrawView(SimView *view) { (void)view; }
int  DoUpdateMap(SimView *view) { (void)view; return 0; }
int  DoUpdateEditor(SimView *view) { (void)view; return 0; }
void DoNewEditor(SimView *view) { (void)view; }
void DoNewMap(SimView *view) { (void)view; }
void ViewToTileCoords(SimView *view, int x, int y, int *tx, int *ty)
{
  (void)view; (void)x; (void)y;
  *tx = 0; *ty = 0;
}
void ViewToPixelCoords(SimView *view, int x, int y, int *px, int *py)
{
  (void)view; (void)x; (void)y;
  *px = 0; *py = 0;
}
void DidStopPan(SimView *view) { (void)view; }

/* ---- Allocators ---- */

Sim *MakeNewSim(void)
{
  return (Sim *)calloc(1, sizeof(Sim));
}

SimView *MakeNewView(void)
{
  return (SimView *)calloc(1, sizeof(SimView));
}

/* ---- Timer & Earthquake ---- */

void StartMicropolisTimer(void) { }
void StopMicropolisTimer(void) { }
void FixMicropolisTimer(void) { }

void DoEarthQuake(void)
{
  ShakeNow = 20;
}

void StopEarthquake(void)
{
  ShakeNow = 0;
}

/* ---- Chalk / Ink Overlay (Discarded Legacy Feature) ---- */

Ink *NewInk(void) { return (Ink *)0; }
void FreeInk(Ink *ink) { (void)ink; }
void StartInk(Ink *ink, int x, int y) { (void)ink; (void)x; (void)y; }
void AddInk(Ink *ink, int x, int y) { (void)ink; (void)x; (void)y; }
void EraseOverlay(void) { }

/* ---- Graphs & Census ---- */

void ChangeCensus(void) { }
void doAllGraphs(void) { }
void graphDoer(void) { }
void initGraphs(void) { }
void InitGraphMax(void) { }
void graph_command_init(void) { }
void drawGraph(void) { }

/* ---- Minimap & Sprites ---- */

void setUpMapProcs(void) { }
void drawAll(SimView *view) { (void)view; }
void DrawObjects(SimView *view) { (void)view; }

/* ---- Eval (Tcl Bridge Stub) ---- */

int Eval(char *buf)
{
  (void)buf;
  return 0;
}

/* ---- UI Hooks (Replacing vt_* frontend calls) ---- */

sim_ui_callbacks_t g_sim_ui_callbacks = { 0 };

void sim_ui_auto_goto(int x, int y)
{
  if (g_sim_ui_callbacks.on_auto_goto) {
    g_sim_ui_callbacks.on_auto_goto(x, y);
  }
}

void sim_ui_show_notice(int id)
{
  if (g_sim_ui_callbacks.on_show_notice) {
    g_sim_ui_callbacks.on_show_notice(id);
  }
}

void sim_ui_budget_modal(void)
{
  if (g_sim_ui_callbacks.on_budget_modal) {
    g_sim_ui_callbacks.on_budget_modal();
  }
}

void sim_ui_show_zone_status(char *str, char *s0, char *s1,
                            char *s2, char *s3, char *s4,
                            int x, int y)
{
  if (g_sim_ui_callbacks.on_show_zone_status) {
    g_sim_ui_callbacks.on_show_zone_status(str, s0, s1, s2, s3, s4, x, y);
  }
}

void sim_ui_did_tool(const char *name, int x, int y)
{
  if (g_sim_ui_callbacks.on_did_tool) {
    g_sim_ui_callbacks.on_did_tool(name, x, y);
  }
}
