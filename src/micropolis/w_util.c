#include "sim.h"


/* comefrom: drawTaxesCollected incBoxValue decBoxValue drawCurrentFunds
	     drawActualBox UpdateFunds updateCurrentCost */
void
makeDollarDecimalStr(char *numStr, char *dollarStr)
{
  register short leftMostSet;
  short numOfDigits;
  short numOfChars;
  register short numOfCommas;
  register short dollarIndex = 0;
  register short numIndex = 0;
  register short x;

  numOfDigits = strlen(numStr);

  if (numOfDigits == 1) {
    dollarStr[0] = '$';
    dollarStr[1] = numStr[ 0 ];
    dollarStr[2] = 0;
    return;
  } else if (numOfDigits == 2) {
    dollarStr[0] = '$';
    dollarStr[1] = numStr[0];
    dollarStr[2] = numStr[1];
    dollarStr[3] = 0;
    return;
  } else if (numOfDigits == 3) {
    dollarStr[0] = '$';
    dollarStr[1] = numStr[0];
    dollarStr[2] = numStr[1];
    dollarStr[3] = numStr[2];
    dollarStr[4] = 0;
  } else {
    leftMostSet = numOfDigits % 3;
    if (leftMostSet == 0)
      leftMostSet = 3;

    numOfCommas = (numOfDigits - 1) / 3;

    /* add 1 for the dollar sign */
    numOfChars = numOfDigits + numOfCommas + 1;

    dollarStr[numOfChars] = 0;

    dollarStr[dollarIndex++] = '$';

    for (x = 1; x <= leftMostSet; x++) {
      dollarStr[dollarIndex++] = numStr[numIndex++];
    }

    for (x = 1; x <= numOfCommas; x++) {
      dollarStr[dollarIndex++] = ',';
      dollarStr[dollarIndex++] = numStr[numIndex++];
      dollarStr[dollarIndex++] = numStr[numIndex++];
      dollarStr[dollarIndex++] = numStr[numIndex++];
    }
  }
}


void
Pause(void)
{
  if (!sim_paused) {
    sim_paused_speed = SimMetaSpeed;
    setSpeed(0);
    sim_paused = 1;
  }
}


void
Resume(void)
{
  if (sim_paused) {
    sim_paused = 0;
    setSpeed(sim_paused_speed);
  }
}


void
setSpeed(short speed)
{
  if (speed < 0) speed = 0;
  else if (speed > 3) speed = 3;

  SimMetaSpeed = speed;

  if (sim_paused) {
    sim_paused_speed = SimMetaSpeed;
    speed = 0;
  }

  SimSpeed = speed;

  if (speed == 0) {
    StopMicropolisTimer();
  } else {
    StartMicropolisTimer();
  }

  { char buf[256];
    sprintf(buf, "UISetSpeed %d", sim_paused ? 0 : SimMetaSpeed);
    Eval(buf);
  }
}


void
setSkips(int skips)
{
  sim_skips = skips;
  sim_skip = 0;
}


void
SetGameLevelFunds(short level)
{
  switch (level) {
  default:
  case 0:
    SetFunds(20000);
    SetGameLevel(0);
    break;
  case 1:
    SetFunds(10000);
    SetGameLevel(1);
    break;
  case 2:
    SetFunds(5000);
    SetGameLevel(2);
    break;
  }
}


void
SetGameLevel(short level)
{
  GameLevel = level;
  UpdateGameLevel();
}


void
UpdateGameLevel(void)
{
  char buf[256];

  sprintf(buf, "UISetGameLevel %d", GameLevel);
  Eval(buf);
}


void
setCityName(char *name)
{
  char *cp = name;

  while (*cp) {
    if (!isalnum(*cp))
      *cp = '_';
    cp++;
  }
  setAnyCityName(name);
}


void
setAnyCityName(char *name)
{
  char buf[1024];

  CityName = name;
  sprintf(buf, "UISetCityName {%s}", CityName);
  Eval(buf);
}


void
SetYear(int year)
{
  // Must prevent year from going negative, since it screws up the non-floored modulo arithmetic.
  if (year < StartingYear) {
    year = StartingYear;
  }

  year = (year - StartingYear) - (CityTime / 48);
  CityTime += year * 48;
  doTimeStuff();
}


int
CurrentYear(void)
{
  return (CityTime/48 + StartingYear);
}


void
DoSetMapState(SimView *view, short state)
{
  view->map_state = state;
  view->invalid = 1;
  EventuallyRedrawView(view);
}


void
DoNewGame(void)
{
  Eval("UINewGame");
}


void
DoGeneratedCityImage(char *name, int time, int pop, char *class, int score)
{
  (void)name; (void)time; (void)pop; (void)class; (void)score;
}


void
DoStartElmd(void)
{
}


void
DoPopUpMessage(char *msg)
{
  char buf[1024];

  sprintf(buf, "UIPopUpMessage {%s}", msg);
  Eval(buf);
}
