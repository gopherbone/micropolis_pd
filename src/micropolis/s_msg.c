/* s_msg.c
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


QUAD LastCityPop;
short LastCategory;
short LastPicNum;
short autoGo;
short HaveLastMessage = 0;
char LastMessage[256];

static void DoScenarioScore(int type);


/* comefrom: Simulate */
void
SendMessages(void)
{
  register int z;
  short PowerPop;
  float TM;

  if ((ScenarioID) && (ScoreType) && (ScoreWait)) {
    ScoreWait--;
    if (!ScoreWait)
      DoScenarioScore(ScoreType);
  }

  CheckGrowth();

  TotalZPop = ResZPop + ComZPop + IndZPop;
  PowerPop = NuclearPop + CoalPop;

  /* test for coal out of fuel post */

  if (!PowerPop) {
    SendMes(1);
    return;
  }

  if (PolluteAverage > 60) {
    SendMes(2);
  }

  if (CrimeAverage > 100) {
    SendMes(3);
  }

  if ((TotalZPop) &&
      (TrafficAverage > 60)) {
    SendMes(4);
  }

  if ((FirePop > 24) &&
      (FirePop < 40)) {
    SendMes(5);
  }

  if (NeedHosp) {
    SendMes(6);
    NeedHosp = 0;
  }

  if (NeedChurch) {
    SendMes(7);
    NeedChurch = 0;
  }

  /* changed from 100 to 80 post */
  if (TotalPop > 20) {
    z = TotalPop / 80;
    if (z > StadiumPop) {
      SendMes(8);
    }
  }

  if (IndPop > 70) {
    if (!PortPop) {
      SendMes(9);
    }
  }

  if (ComPop > 100) {
    if (!APortPop) {
      SendMes(10);
    }
  }

  if (CityTax > 12) {
    SendMes(11);
  }

  if (RoadEffect < 20) {
    SendMes(12);
  }

  if (FireEffect < 700) {
    SendMes(13);
  }

  if (PoliceEffect < 700) {
    SendMes(14);
  }

  if (TotalZPop > 10) {
    TM = unPwrdZCnt + PwrdZCnt;
    if (TM) {
      if ((PwrdZCnt / TM) < .7) {
	SendMes(16);
      }
    }
  }

  if (CityTime && !(CityTime % (48 * 4))) {
    z = CityScore - AverageCityScore;
    if (z > 100)
      SendMes(43);
    else if (z < -100)
      SendMes(44);
  }
}


/* comefrom: SendMessages */
void
CheckGrowth(void)
{
  QUAD ThisCityPop;
  short z;

  if (!(CityTime & 3)) {
    z = 0;
    ThisCityPop = ((ResPop) + (ComPop * 8L) + (IndPop * 8L)) * 20L;
    if (LastCityPop) {
      if ((LastCityPop < 2000) && (ThisCityPop >= 2000))	z = 35;
      if ((LastCityPop < 10000) && (ThisCityPop >= 10000)) 	z = 36;
      if ((LastCityPop < 50000L) && (ThisCityPop >= 50000L)) 	z = 37;
      if ((LastCityPop < 100000L) && (ThisCityPop >= 100000L))	z = 38;
      if ((LastCityPop < 500000L) && (ThisCityPop >= 500000L))	z = 39;
    }
    if (z)
      if (z != LastCategory) {
	SendMes(-z);
	LastCategory = z;
      }
    LastCityPop = ThisCityPop;
  }
}


/* comefrom: SendMessages */
static void
DoScenarioScore(int type)
{
  short z;

  z = -200;	/* you lose */
  switch(type) {
  case 1:	/* Dullsville */
	  if (CityClass >= 4)		z = -100;
	  break;
  case 2:	/* San Francisco */
	  if (CityClass >= 4)		z = -100;
	  break;
  case 3:	/* Hamburg */
	  if (CityClass >= 4)		z = -100;
	  break;
  case 4:	/* Bern */
	  if (TrafficAverage < 80)	z = -100;
	  break;
  case 5:	/* Tokyo */
	  if (CityScore > 500)		z = -100;
	  break;
  case 6:	/* Detroit */
	  if (CrimeAverage < 60)	z = -100;
	  break;
  case 7:	/* Boston */
	  if (CityScore > 500)		z = -100;
	  break;
  case 8:	/* Rio de Janeiro */
	  if (CityScore > 500)		z = -100;
	  break;
  }
  ClearMes();
  SendMes(z);

  if (z == -200)
    DoLoseGame();
}


void
ClearMes(void)
{
  MessagePort = 0;
  MesX = 0;
  MesY = 0;
  LastPicNum = 0;
}


/* comefrom: MakeEarthquake MakeFire MakeFire MakeFlood SendMessages
	     CheckGrowth DoScenarioScore DoPowerScan */
int
SendMes(short Mnum)
{
  if (Mnum < 0) {
    if (Mnum != LastPicNum) {
      MessagePort = Mnum;
      MesX = 0;
      MesY = 0;
      LastPicNum = Mnum;
      return (1);
    }
  } else {
    if (!(MessagePort)) {
      MessagePort = Mnum;
      MesX = 0;
      MesY = 0;
      return(1);
    }
  }
  return(0);
}


/* comefrom: DoExplosion DoCopter ExplodeObject */
void
SendMesAt(short Mnum, short x, short y)
{
  if (SendMes(Mnum)) {
    MesX = x;
    MesY = y;
  }
}


void
doMessage(void)
{
  char messageStr[256];
  short pictId;
  short firstTime;

  if (MessagePort) {
    firstTime = 0;
    if (MessagePort < 0) {
      pictId = -MessagePort;
      firstTime = 1;
    } else {
      pictId = MessagePort;
    }

    if (pictId > 60) {
      return;
    }

    GetIndString(messageStr, 301, pictId);

    if (firstTime) {
      /* picture */
      DoShowPicture(pictId);
    }

    if ((MesX) && (MesY)) {
      DoAutoGoto(MesX, MesY, messageStr);
    } else {
      SetMessageField(messageStr);
    }
    MessagePort = 0;
  }
}


void
DoAutoGoto(short x, short y, char *msg)
{
  SetMessageField(msg);
  sim_ui_auto_goto(x, y);
}


void
SetMessageField(char *str)
{
  char buf[256];

  if (!HaveLastMessage ||
      strcmp(LastMessage, str)) {
    strcpy(LastMessage, str);
    HaveLastMessage = 1;
    sprintf(buf, "UISetMessage {%s}", str);
    Eval(buf);
  }
}


void
DoShowPicture(short id)
{
  sim_ui_show_notice(id);
}


void
DoLoseGame(void)
{
  Eval("UILoseGame");
}


void
DoWinGame(void)
{
  Eval("UIWinGame");
}
