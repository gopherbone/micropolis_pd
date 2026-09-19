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
