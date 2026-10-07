/* sim.h:  Main include file
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

#ifndef SIM_H
#define SIM_H

/* workaround sun c compiler junk */

#ifdef sun

#define _TTYCHARS_
#define CTRL(c) (#c[0]&037)

#ifndef SOLARIS2
#define __sys_ioccom_h
#define _IOCPARM_MASK   0xff            /* parameters must be < 256 bytes */
#define _IOC_VOID       0x20000000      /* no parameters */
#define _IOC_OUT        0x40000000      /* copy out parameters */
#define _IOC_IN         0x80000000      /* copy in parameters */
#define _IOC_INOUT      (_IOC_IN|_IOC_OUT)
#define _IO(x,y)        (_IOC_VOID|(#x[0]<<8)|y)
#define _IOR(x,y,t)     (_IOC_OUT|((sizeof(t)&_IOCPARM_MASK)<<16)|(#x[0]<<8)|y)
#define _IORN(x,y,t)    (_IOC_OUT|(((t)&_IOCPARM_MASK)<<16)|(#x[0]<<8)|y)
#define _IOW(x,y,t)     (_IOC_IN|((sizeof(t)&_IOCPARM_MASK)<<16)|(#x[0]<<8)|y)
#define _IOWN(x,y,t)    (_IOC_IN|(((t)&_IOCPARM_MASK)<<16)|(#x[0]<<8)|y)
#define _IOWR(x,y,t)    (_IOC_INOUT|((sizeof(t)&_IOCPARM_MASK)<<16)|(#x[0]<<8)|y)
#define _IOWRN(x,y,t)   (_IOC_INOUT|(((t)&_IOCPARM_MASK)<<16)|(#x[0]<<8)|y)
#endif

#endif


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
/* #include <malloc.h> */	/* not portable (absent on some systems); stdlib.h has malloc */
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>

#ifdef sun
#ifdef SOLARIS2
#include <sys/systeminfo.h>
#else
#include <sys/vadvise.h>
#endif
#endif

#include "mac.h"

/* ncurses port: Tcl memory allocators replaced by libc.
 * ckalloc/ckfree were provided by the Tcl library in the X11 build.
 */
#define ckalloc(n) ((char *)malloc((unsigned)(n)))
#define ckfree(p)  free((void *)(p))

#include "view.h"
#include "macros.h"

/* Constants */

#define TRUE		 1
#define FALSE		 0

#ifdef MEGA
#define SimWidth	(120 * MEGA)
#define SimHeight	(100 * MEGA)
#else
#define SimWidth	120
#define SimHeight	100
#endif

#define WORLD_X		SimWidth
#define WORLD_Y		SimHeight
#define HWLDX		(SimWidth >>1)
#define HWLDY		(SimHeight >>1)
#define QWX		(SimWidth >>2)
#define QWY		(SimHeight >>2)
#define SmX		(SimWidth >>3)
#define SmY		((SimHeight + 7) >>3)

#define EDITOR_W (WORLD_X * 16)
#define EDITOR_H (WORLD_Y * 16)
#define MAP_W (WORLD_X * 3)
#define MAP_H (WORLD_Y * 3)

#define NIL		0
#define HORIZ		1
#define VERT		0

#define PROBNUM 10

#define HISTLEN		480
#define MISCHISTLEN	240

#define POWERMAPROW		((WORLD_X + 15) / 16)

#ifdef MEGA
#define POWERMAPLEN		((QUAD)(2 * POWERMAPROW * WORLD_Y))
#define POWERWORD(x, y)		((((QUAD)(x)) >>4) + (((QUAD)(y)) * POWERMAPROW))
#else
#define POWERMAPLEN		1700 /* ??? PWRMAPSIZE */
#define POWERWORD(x, y)		(((x) >>4) + ((y) <<3))
#endif

#define SETPOWERBIT(x, y)	PowerMap[POWERWORD((x), (y))] |= 1 << ((x) & 15)
#define PWRMAPSIZE		(POWERMAPROW * WORLD_Y)
#define PWRSTKSIZE		((WORLD_X * WORLD_Y) / 4)

#define ALMAP 0 /* all */
#define REMAP 1 /* residential */
#define COMAP 2 /* commercial */
#define INMAP 3 /* industrial */

#define PRMAP 4 /* power */
#define RDMAP 5 /* road */

#define PDMAP 6 /* population density */
#define RGMAP 7 /* rate of growth */

#define TDMAP 8 /* traffic density */
#define PLMAP 9 /* pollution */
#define CRMAP 10 /* crime */
#define LVMAP 11 /* land value */

#define FIMAP 12 /* fire radius */
#define POMAP 13 /* police radius */
#define DYMAP 14 /* dynamic */

#define NMAPS 15

/* These adjust frequency in Simulate() */

#define VALVERATE 2
#define CENSUSRATE 4
#define TAXFREQ 48

/* These are names of the 16 colors */
#define SIMCOLOR_WHITE		0
#define SIMCOLOR_YELLOW		1
#define SIMCOLOR_ORANGE		2
#define SIMCOLOR_RED		3
#define SIMCOLOR_DARKRED		4
#define SIMCOLOR_DARKBLUE		5
#define SIMCOLOR_LIGHTBLUE		6
#define SIMCOLOR_BROWN		7
#define SIMCOLOR_LIGHTGREEN	8
#define SIMCOLOR_DARKGREEN		9
#define SIMCOLOR_OLIVE		10
#define SIMCOLOR_LIGHTBROWN	11
#define SIMCOLOR_LIGHTGRAY		12
#define SIMCOLOR_MEDIUMGRAY	13
#define SIMCOLOR_DARKGRAY		14
#define SIMCOLOR_BLACK		15

/* Status Bits */

#define PWRBIT		32768	/*20	bit 15	*/
#define CONDBIT		16384	/*10	bit 14	*/
#define BURNBIT		8192	/*8	bit 13	*/
#define BULLBIT		4096	/*4	bit 12	*/
#define ANIMBIT		2048	/*2	bit 11	*/
#define ZONEBIT		1024	/*1	bit 10	*/
#define ALLBITS		64512	/*  mask for upper 6 bits	*/
#define LOMASK		1023	/*	mask for low 10 bits	*/

#define BLBNBIT		(BULLBIT+BURNBIT)
#define BLBNCNBIT	(BULLBIT+BURNBIT+CONDBIT)
#define BNCNBIT		(BURNBIT+CONDBIT)

/* Object & Sound Numbers */

#define TRA		1
#define COP		2
#define AIR		3
#define SHI		4
#define GOD		5
#define TOR		6
#define EXP		7
#define BUS		8

/* Max # of Objects */

#define OBJN		9

/* Graph Histories */
#define RES_HIST	0
#define COM_HIST	1
#define IND_HIST	2
#define MONEY_HIST	3
#define CRIME_HIST	4
#define POLLUTION_HIST	5
#define HISTORIES	6
#define ALL_HISTORIES	((1 <<HISTORIES) - 1)

/* Character Mapping */

#define DIRT		0
#define RIVER		2
#define REDGE		3
#define CHANNEL		4
#define FIRSTRIVEDGE	5
#define LASTRIVEDGE	20
#define TREEBASE	21
#define LASTTREE	36
#define WOODS		37
#define UNUSED_TRASH1	38
#define UNUSED_TRASH2	39
#define WOODS2		40
#define WOODS3		41
#define WOODS4		42
#define WOODS5		43
#define RUBBLE		44
#define LASTRUBBLE	47
#define FLOOD		48
#define LASTFLOOD	51
#define RADTILE		52
#define UNUSED_TRASH3	53
#define UNUSED_TRASH4	54
#define UNUSED_TRASH5	55
#define FIRE		56
#define FIREBASE	56
#define LASTFIRE	63
#define ROADBASE	64
#define HBRIDGE		64
#define VBRIDGE		65
#define ROADS		66
#define INTERSECTION	76
#define HROADPOWER	77
#define VROADPOWER	78
#define BRWH		79
#define LTRFBASE	80
#define BRWV		95
#define BRWXXX1		111
#define BRWXXX2		127
#define BRWXXX3		143
#define HTRFBASE	144
#define BRWXXX4		159
#define BRWXXX5		175
#define BRWXXX6		191
#define LASTROAD	206
#define BRWXXX7		207
#define POWERBASE	208
#define HPOWER		208
#define VPOWER		209
#define LHPOWER		210
#define LVPOWER		211
#define RAILHPOWERV	221
#define RAILVPOWERH	222
#define LASTPOWER	222
#define UNUSED_TRASH6	223
#define RAILBASE	224
#define HRAIL		224
#define VRAIL		225
#define LHRAIL		226
#define LVRAIL		227
#define HRAILROAD	237
#define VRAILROAD	238
#define LASTRAIL	238
#define ROADVPOWERH	239 /* bogus? */
#define RESBASE		240
#define FREEZ		244
#define HOUSE		249
#define LHTHR		249
#define HHTHR		260
#define RZB		265
#define HOSPITAL	409
#define CHURCH		418
#define COMBASE		423
#define COMCLR		427
#define CZB		436
#define INDBASE		612
#define INDCLR		616
#define LASTIND		620
#define IND1		621
#define IZB		625
#define IND2		641
#define IND3		644
#define IND4		649
#define IND5		650
#define IND6		676
#define IND7		677
#define IND8		686
#define IND9		689
#define PORTBASE	693
#define PORT		698
#define LASTPORT	708
#define AIRPORTBASE	709
#define RADAR		711
#define AIRPORT		716
#define COALBASE	745
#define POWERPLANT	750
#define LASTPOWERPLANT	760
#define FIRESTBASE	761
#define FIRESTATION	765
#define POLICESTBASE	770
#define POLICESTATION	774
#define STADIUMBASE	779
#define STADIUM		784
#define FULLSTADIUM	800
#define NUCLEARBASE	811
#define NUCLEAR		816
#define LASTZONE	826
#define LIGHTNINGBOLT	827
#define HBRDG0		828
#define HBRDG1		829
#define HBRDG2		830
#define HBRDG3		831
#define RADAR0		832
#define RADAR1		833
#define RADAR2		834
#define RADAR3		835
#define RADAR4		836
#define RADAR5		837
#define RADAR6		838
#define RADAR7		839
#define FOUNTAIN	840
#define INDBASE2	844
#define TELEBASE	844
#define TELELAST	851
#define SMOKEBASE	852
#define TINYEXP		860
#define SOMETINYEXP	864
#define LASTTINYEXP	867
#define COALSMOKE1	916
#define COALSMOKE2	920
#define COALSMOKE3	924
#define COALSMOKE4	928
#define FOOTBALLGAME1	932
#define FOOTBALLGAME2	940
#define VBRDG0		948
#define VBRDG1		949
#define VBRDG2		950
#define VBRDG3		951

#define TILE_COUNT	960

/* 
 * These describe the wand values, the object dragged around on the screen.
 */

#define residentialState 0
#define commercialState 1
#define industrialState 2
#define fireState 3
#define queryState 4
#define policeState 5
#define wireState 6
#define dozeState 7
#define rrState 8
#define roadState 9
#define chalkState 10
#define eraserState 11
#define stadiumState 12
#define parkState 13
#define seaportState  14
#define powerState 15
#define nuclearState 16
#define airportState 17
#define networkState 18


#define firstState residentialState
#define lastState networkState

#if 0
#define specialState 16
#define monsterGoalState 17
#define helicopterGoalState 18
#define blackState 19
#define whiteState 20
#define redState 21
#define orangeState 23
#define blueState 24
#define greenState 25
#define brownState 26
#endif


#define STATE_CMD 0
#define STATE_TILES 1
#define STATE_OVERLAYS 2
#define STATE_GRAPHS 3


extern short *Map[WORLD_X];		/* Main Map 120 x 100  */
extern Byte *PopDensity[HWLDX];		/* 2X2 Maps  60 x 50 */
extern Byte *TrfDensity[HWLDX];
extern Byte *PollutionMem[HWLDX];
extern Byte *LandValueMem[HWLDX];
extern Byte *CrimeMem[HWLDX];
extern Byte *tem[HWLDX];
extern Byte *tem2[HWLDX];
extern Byte *TerrainMem[QWX];		/* 4X4 Maps  30 x 25 */
extern Byte *Qtem[QWX];
extern short RateOGMem[SmX][SmY];
extern short FireStMap[SmX][SmY];	/* 8X8 Maps  15 x 13 */
extern short PoliceMap[SmX][SmY];
extern short PoliceMapEffect[SmX][SmY];
extern short ComRate[SmX][SmY];
extern short FireRate[SmX][SmY];
extern short STem[SmX][SmY];
extern short SpriteXOffset[OBJN];
extern short SpriteYOffset[OBJN];
extern short SMapX, SMapY;
extern short CChr, CChr9;
extern short RoadTotal, RailTotal, FirePop;
extern short ResPop, ComPop, IndPop, TotalPop, LastTotalPop;
extern short ResZPop, ComZPop, IndZPop, TotalZPop;
extern short HospPop, ChurchPop, StadiumPop;
extern short PolicePop, FireStPop;
extern short CoalPop, NuclearPop, PortPop, APortPop;
extern short NeedHosp, NeedChurch;
extern short CrimeAverage, PolluteAverage, LVAverage;
extern char *MicropolisVersion;
extern char *CityName;
extern char *CityFileName;
extern char *StartupName;
extern short StartingYear;
extern QUAD CityTime;
extern QUAD LastCityTime;
extern QUAD LastCityMonth;
extern QUAD LastCityYear;
extern QUAD LastFunds;
extern QUAD LastR, LastC, LastI;
extern short GameLevel;
extern short Cycle;
extern short ScenarioID;
extern short ShakeNow;
extern QUAD DonDither;
extern int DoOverlay;

extern short *ResHis, ResHisMax;
extern short *ComHis, ComHisMax;
extern short *IndHis, IndHisMax;
extern short *MoneyHis, *CrimeHis, *PollutionHis, *MiscHis;
extern short *PowerMap;

extern float roadPercent, policePercent, firePercent;
extern QUAD RoadSpend, PoliceSpend, FireSpend;
extern QUAD roadMaxValue, policeMaxValue, fireMaxValue;
extern QUAD TaxFund, RoadFund, PoliceFund, FireFund;
extern short RoadEffect, PoliceEffect, FireEffect;
extern short TaxFlag, CityTax;
extern QUAD TotalFunds;

extern QUAD costOf[];

extern short flagBlink;
extern unsigned char tileSynch;
extern short aniTile[];
extern unsigned char aniSynch[];
extern int TilesAnimated;
extern int DoAnimation;
extern int DoMessages;
extern int DoNotices;
extern unsigned char ColorIntensities[];

extern short MesX, MesY;
extern short MesNum, MessagePort;
extern QUAD LastMesTime;

extern short SimSpeed;
extern short SimMetaSpeed;
extern short NoDisasters;
extern short autoBulldoze;
extern short autoBudget;
extern short autoGo;
extern short UserSoundOn;

extern short DisasterEvent;
extern short DisasterWait;

extern short ResCap, ComCap, IndCap;
extern short RValve, CValve, IValve;
extern short PwrdZCnt;
extern short unPwrdZCnt;

extern char *HomeDir, *ResourceDir, *HostName;

extern short Graph10Max, Graph120Max;
extern short Res2HisMax, Com2HisMax, Ind2HisMax;
extern unsigned char *History10[HISTORIES], *History120[HISTORIES];
extern short CityScore;
extern short deltaCityScore;
extern short AverageCityScore;
extern short ScoreType;
extern short ScoreWait;
extern short CityClass;
extern short PolMaxX, PolMaxY;
extern int PowerStackNum;
extern short TrafficAverage;
extern short PosStackN;
extern short SMapXStack[], SMapYStack[];
extern short Zsource;
extern short HaveLastMessage;
extern char LastMessage[256];
extern short PdestX, PdestY;
extern short CdestX, CdestY;
extern int absDist;
extern short CopFltCnt;
extern short GodCnt;
extern short GdestX, GdestY;
extern short GorgX, GorgY;
extern short GodControl;
extern short CopControl;
extern short CdestX, CdestY;
extern short TrafMaxX, TrafMaxY;
extern short CrimeMaxX, CrimeMaxY;
extern short FloodX, FloodY;
extern short CrashX, CrashY;
extern short CCx, CCy;
extern QUAD CityPop, deltaCityPop;
extern char *cityClassStr[6];
extern short CityYes, CityNo;
extern short ProblemTable[PROBNUM];
extern short ProblemVotes[PROBNUM];
extern short ProblemOrder[4];
extern QUAD CityAssValue;

extern short InitSimLoad;
extern short DoInitialEval;
extern int Startup;
extern int StartupGameLevel;
extern int PerformanceTiming;
extern double FlushTime;

extern char *optarg;
extern int optind;
extern struct timeval start_time, now_time, beat_time, last_now_time;
extern Sim *sim;
extern int WireMode;
extern int MultiPlayerMode;
extern int SugarMode;
extern int sim_delay;
extern int sim_skips;
extern int sim_skip;
extern int sim_paused;
extern int sim_paused_speed;
extern int sim_tty;
#ifdef CAM
extern int sim_just_cam;
#endif
extern int heat_steps;
extern int heat_flow;
extern int heat_rule;
extern int UpdateDelayed;
extern int DynamicData[32];
extern int Players;
extern int Votes;
extern int BobHeight;
extern int OverRide;
extern int Expensive;
extern int PendingTool;
extern int PendingX;
extern int PendingY;
extern int TreeLevel;
extern int LakeLevel;
extern int CurveLevel;
extern int CreateIsland;
extern short specialBase;
extern short PunishCnt;
extern short Dozing;
extern short toolSize[];
extern short toolOffset[];
extern QUAD toolColors[];
extern char *Displays;
extern char *FirstDisplay;
extern char *dateStr[12];

extern short NewMap;
extern short NewMapFlags[NMAPS];
extern short NewGraph;
extern short ValveFlag;
extern short MustUpdateFunds;
extern short MustUpdateOptions;
extern short CensusChanged;
extern short EvalChanged;
extern short MeltX, MeltY;
extern int NeedRest;
extern int ExitReturn;
extern int tkMustExit;		/* was declared via Tk headers; run-loop exit flag */

extern int FlushStyle;
extern int GotXError;

/* ---- Complete C99 Function Prototypes ---- */

/* s_alloc.c */
void initMapArrays(void);

/* s_disast.c */
void DoDisasters(void);
void ScenarioDisaster(void);
void MakeMeltdown(void);
void FireBomb(void);
void MakeEarthquake(void);
void SetFire(void);
void MakeFire(void);
int Vunerable(int tem);
void MakeFlood(void);
void DoFlood(void);

/* s_eval.c */
void CityEvaluation(void);
void EvalInit(void);
void GetAssValue(void);
int GetUnemployment(void);

/* s_fileio.c */
typedef struct {
  int exists;
  char name[32];
  long funds;
  long population;
  int year;
  int month;
  int difficulty;
  int score;
} city_meta_t;

#define CITY_MAX_SLOTS 4

int SaveCity(void);
int SaveCityAs(char *filename);
int loadFile(char *filename);
int loadMem(const unsigned char *buf, QUAD size);
int saveFile(char *filename);
int LoadCity(char *filename);
int LoadEmbeddedCity(char *name);
void LoadScenario(short s);
void DidLoadScenario(void);
void DidLoadCity(void);
void DidntLoadCity(char *msg);
void DoSaveCityAs(void);
void DidSaveCity(void);
void DidntSaveCity(char *msg);
int city_read_meta(const char *filename, city_meta_t *meta);
int city_get_slot_path(int slot, char *out_path, size_t max_len);
int city_save_slot(int slot);
int city_load_slot(int slot);
int city_get_slot_meta(int slot, city_meta_t *meta);

/* s_gen.c */
void GenerateNewCity(void);
void GenerateMap(int r);
void GenerateSomeCity(int seed);
void ClearMap(void);
void ClearAll(void);
void ClearUnnatural(void);

/* s_init.c */
void InitWillStuff(void);
void ResetMapState(void);
void ResetEditorState(void);

/* s_msg.c */
void SendMessages(void);
int SendMes(short mes);
void SendMesAt(short mes, short x, short y);
void ClearMes(void);
void doMessage(void);
void CheckGrowth(void);
void DoAutoGoto(short x, short y, char *msg);
void SetMessageField(char *str);
void DoShowPicture(short id);
void DoLoseGame(void);
void DoWinGame(void);

/* s_power.c */
int MoveMapSim(short MDir);
void DoPowerScan(void);
void PushPowerStack(void);
void PullPowerStack(void);

/* s_scan.c */
void FireAnalysis(void);
void PopDenScan(void);
void PTLScan(void);
void CrimeScan(void);

/* s_sim.c */
void SimFrame(void);
void Simulate(int mod16);
void DoSimInit(void);
short Rand(short range);
int Rand16(void);
int Rand16Signed(void);
void RandomlySeedRand(void);
void SeedRand(int seed);
void DoMeltdown(int SX, int SY);
int DoBridge(void);
void DoSPZone(short PwrOn);
void RepairZone(short ZCent, short zsize);
int GetBoatDis(void);
void FireZone(int Xloc, int Yloc, int ch);
void UpdateFundEffects(void);

/* s_traf.c */
int MakeTraf(int Zt);
int FindPRoad(void);

/* s_zone.c */
void DoZone(void);
int SetZPower(void);
void DoIndustrial(int ZonePwrFlg);
void DoCommercial(int ZonePwrFlg);
void DoResidential(int ZonePwrFlg);
int DoFreePop(void);
int RZPop(int Ch9);
int CZPop(int Ch9);
int IZPop(int Ch9);

/* g_ani.c */
void animateTiles(void);

/* rand.c / random.c */
int sim_rand(void);
void sim_srand(unsigned int seed);
QUAD sim_random(void);
void sim_srandom(unsigned int x);
char *sim_initstate(unsigned int seed, char *arg_state, int n);
char *sim_setstate(char *arg_state);

/* w_budget.c */
void InitFundingLevel(void);
void DoBudget(void);
void DoBudgetFromMenu(void);
void DoBudgetNow(int fromMenu);
void drawBudgetWindow(void);
void ReallyDrawBudgetWindow(void);
void drawCurrPercents(void);
void ReallyDrawCurrPercents(void);
void UpdateBudgetWindow(void);
void UpdateBudget(void);
void ShowBudgetWindowAndStartWaiting(void);
void SetBudget(char *flowStr, char *previousStr, char *currentStr, char *collectedStr, short tax);
void SetBudgetValues(char *roadGot, char *roadWant, char *policeGot, char *policeWant, char *fireGot, char *fireWant);

/* w_con.c */
int ConnecTile(short x, short y, short *TileAdrPtr, short Command);

/* w_eval.c */
void doScoreCard(void);
void ChangeEval(void);
void scoreDoer(void);
void SetEvaluation(char *changed, char *score,
                   char *ps0, char *ps1, char *ps2, char *ps3,
                   char *pv0, char *pv1, char *pv2, char *pv3,
                   char *pop, char *delta, char *assessed_dollars,
                   char *cityclass, char *citylevel,
                   char *goodyes, char *goodno, char *title);

/* w_keys.c */
void ResetLastKeys(void);
void doKeyDown(SimView *view, short charCode);
void doKeyUp(SimView *view, short charCode);

/* w_resrc.c */
const unsigned char *res_find(const char *name, unsigned int *size);
const unsigned char *city_find(const char *name, unsigned int *size);
int EmbeddedCityCount(void);
const char *EmbeddedCityName(int i);
Handle GetResource(char *name, QUAD id);
void ReleaseResource(Handle r);
QUAD ResourceSize(Handle h);
char *ResourceName(Handle h);
QUAD ResourceID(Handle h);
void GetIndString(char *str, int id, short num);

/* w_sound.c */
void InitializeSound(void);
void ShutDownSound(void);
void MakeSound(char *channel, char *id);
void MakeSoundOn(SimView *view, char *channel, char *id);
void StartBulldozer(void);
void StopBulldozer(void);
void SoundOff(void);
void DoStartSound(char *channel, char *id);
void DoStopSound(char *id);

/* w_sprite.c */
SimSprite *NewSprite(char *name, int type, int x, int y);
void InitSprite(SimSprite *sprite, int x, int y);
void DestroyAllSprites(void);
void DestroySprite(SimSprite *sprite);
SimSprite *GetSprite(int type);
SimSprite *MakeSprite(int type, int x, int y);
SimSprite *MakeNewSprite(int type, int x, int y);
void MoveObjects(void);
void StartFire(int x, int y);
void MakeMonster(void);
void MakeTornado(void);
void MakeExplosion(int x, int y);
void MakeExplosionAt(int x, int y);
void GenerateTrain(int x, int y);
void GenerateShip(void);
void GenerateCopter(int x, int y);
void GeneratePlane(int x, int y);

/* w_tool.c */
void setWandState(SimView *view, short state);
int DoTool(SimView *view, short tool, short x, short y);
void ToolDown(SimView *view, int x, int y);
void ToolUp(SimView *view, int x, int y);
void ToolDrag(SimView *view, int px, int py);
void DoPendTool(SimView *view, int tool, int x, int y);
short tally(short tileValue);
int bulldozer_tool(SimView *view, short x, short y);

/* w_update.c */
void DoUpdateHeads(void);
void UpdateEditors(void);
void UpdateMaps(void);
void UpdateGraphs(void);
void UpdateEvaluation(void);
void UpdateHeads(void);
void UpdateFunds(void);
void ReallyUpdateFunds(void);
void doTimeStuff(void);
void updateDate(void);
void showValves(void);
void drawValve(void);
void SetDemand(double r, double c, double i);
void updateOptions(void);
void UpdateOptionsMenu(int options);

/* w_util.c */
void makeDollarDecimalStr(char *numStr, char *dollarStr);
void Pause(void);
void Resume(void);
void setSpeed(short speed);
void setSkips(int skips);
void SetGameLevelFunds(short level);
void SetGameLevel(short level);
void UpdateGameLevel(void);
void setCityName(char *name);
void setAnyCityName(char *name);
void SetYear(int year);
int CurrentYear(void);
void DoSetMapState(SimView *view, short state);
void DoNewGame(void);
void DoGeneratedCityImage(char *name, int time, int pop, char *class, int score);
void DoStartElmd(void);
void DoPopUpMessage(char *msg);

/* sim_stubs.c */
void sim_exit(int val);
void Kick(void);
void UpdateFlush(void);
void DoTimeoutListen(void);
void DoStopMicropolis(void);
void StopToolkit(void);
void Spend(int dollars);
void SetFunds(int dollars);
QUAD TickCount(void);
Ptr NewPtr(int size);
void GameStarted(void);
void DoPlayNewCity(void);
void DoReallyStartGame(void);
void DoStartLoad(void);
void DoStartScenario(int scenario);
void DropFireBombs(void);
void InitGame(void);
void ReallyQuit(void);
void InvalidateMaps(void);
void InvalidateEditors(void);
void RedrawMaps(void);
void RedrawEditors(void);
void EventuallyRedrawView(SimView *view);
void CancelRedrawView(SimView *view);
int DoUpdateMap(SimView *view);
int DoUpdateEditor(SimView *view);
void DoNewEditor(SimView *view);
void DoNewMap(SimView *view);
void ViewToTileCoords(SimView *view, int x, int y, int *tx, int *ty);
void ViewToPixelCoords(SimView *view, int x, int y, int *px, int *py);
void DidStopPan(SimView *view);
Sim *MakeNewSim(void);
SimView *MakeNewView(void);
void StartMicropolisTimer(void);
void StopMicropolisTimer(void);
void FixMicropolisTimer(void);
void DoEarthQuake(void);
void StopEarthquake(void);
Ink *NewInk(void);
void FreeInk(Ink *ink);
void StartInk(Ink *ink, int x, int y);
void AddInk(Ink *ink, int x, int y);
void EraseOverlay(void);
void ChangeCensus(void);
void doAllGraphs(void);
void graphDoer(void);
void initGraphs(void);
void InitGraphMax(void);
void graph_command_init(void);
void drawGraph(void);
void setUpMapProcs(void);
void drawAll(SimView *view);
void DrawObjects(SimView *view);
int Eval(char *buf);

/* sim_stubs.c UI hooks */
void sim_ui_auto_goto(int x, int y);
void sim_ui_show_notice(int id);
void sim_ui_budget_modal(void);
void sim_ui_show_zone_status(char *str, char *s0, char *s1,
                            char *s2, char *s3, char *s4,
                            int x, int y);
void sim_ui_did_tool(const char *name, int x, int y);

typedef struct {
    void (*on_auto_goto)(int x, int y);
    void (*on_show_notice)(int id);
    void (*on_budget_modal)(void);
    void (*on_show_zone_status)(char *str, char *s0, char *s1,
                                char *s2, char *s3, char *s4,
                                int x, int y);
    void (*on_did_tool)(const char *name, int x, int y);
} sim_ui_callbacks_t;

extern sim_ui_callbacks_t g_sim_ui_callbacks;

#endif /* SIM_H */
