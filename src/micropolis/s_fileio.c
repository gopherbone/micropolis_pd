#include "sim.h"
#include <string.h>
#include <stdlib.h>
#include "platform.h"


/*
 * The .cty / scenario files are stored big-endian.  The original decided at
 * compile time (IS_INTEL etc.) whether to byte-swap.  That is fragile across
 * hosts, so the port detects host endianness at RUNTIME: swap on little-endian
 * hosts (Intel/ARM), leave alone on big-endian hosts (the classic Unix targets
 * -- SPARC, PA-RISC, MIPS, POWER).  Works everywhere with no build flag.
 */

static int
_host_little_endian(void)
{
  unsigned short s = 1;
  return (int)(*(unsigned char *)&s);	/* 1 => little-endian host */
}

static void
_swap_shorts(short *buf, int len)
{
  int i;

  /* Flip bytes in each short! */
  for (i = 0; i < len; i++) {
    *buf = ((*buf & 0xFF) <<8) | ((*buf &0xFF00) >>8);
    buf++;
  }
}

#if 0
static void
_swap_longs(long *buf, int len)
{
  int i;

  /* Flip bytes in each long! */
  for (i = 0; i < len; i++) {
    long l = *buf;
    *buf =
      ((l & 0x000000ff) << 24) |
      ((l & 0x0000ff00) << 8) |
      ((l & 0x00ff0000) >> 8) |
      ((l & 0xff000000) >> 24);
    buf++;
  }
}
#endif

static void
_half_swap_longs(long *buf, int len)
{
  int i;

  /* Flip bytes in each long! */
  for (i = 0; i < len; i++) {
    long l = *buf;
    *buf =
      ((l & 0x0000ffff) << 16) |
      ((l & 0xffff0000) >> 16);
    buf++;
  }
}

#define SWAP_SHORTS(a, b)	do { if (_host_little_endian()) _swap_shorts(a, b); } while (0)
#if 0
#define SWAP_LONGS(a, b)	do { if (_host_little_endian()) _swap_longs(a, b); } while (0)
#endif
#define HALF_SWAP_LONGS(a, b)	do { if (_host_little_endian()) _half_swap_longs(a, b); } while (0)


#define CTY_FILE_SIZE     27120
#define MISC_MAGIC_OFFSET 64
#define MISC_NAME_OFFSET  68
#define CTY_MAGIC         "TINYCTY1"

/* Load a city from an in-memory buffer (27120 bytes standard) */
static int
_load_mem(const unsigned char *src, QUAD size)
{
  const unsigned char *p = src;

  if (size < CTY_FILE_SIZE) {
    return 0;
  }

#define RD(arr, len) do { \
    memcpy((arr), p, (len) * sizeof(short)); \
    SWAP_SHORTS((short *)(arr), (len)); \
    p += (len) * sizeof(short); \
  } while (0)

  RD(ResHis,       HISTLEN / 2);
  RD(ComHis,       HISTLEN / 2);
  RD(IndHis,       HISTLEN / 2);
  RD(CrimeHis,     HISTLEN / 2);
  RD(PollutionHis, HISTLEN / 2);
  RD(MoneyHis,     HISTLEN / 2);
  RD(MiscHis,      MISCHISTLEN / 2);
  RD(&Map[0][0],   WORLD_X * WORLD_Y);

#undef RD
  return 1;
}

/* Shared tail of loadFile/loadMem: unpack funds/date/flags from MiscHis and
 * kick the engine.  The raw map+history arrays are already populated. */
static void
_finish_load(void)
{
  long l;

  /* total funds is a long.....    MiscHis is array of shorts */
  /* total funds is being put in the 50th & 51th word of MiscHis */
  /* find the address, cast the ptr to a lontPtr, take contents */

  l = *(QUAD *)(MiscHis + 50);
  HALF_SWAP_LONGS(&l, 1);
  SetFunds(l);

  l = *(QUAD *)(MiscHis + 8);
  HALF_SWAP_LONGS(&l, 1);
  CityTime = l;

  autoBulldoze = MiscHis[52];	/* flag for autoBulldoze */
  autoBudget = MiscHis[53];	/* flag for autoBudget */
  autoGo = MiscHis[54];		/* flag for autoGo */
  UserSoundOn = MiscHis[55];	/* flag for the sound on/off */
  CityTax = MiscHis[56];
  SimSpeed = MiscHis[57];
  //  sim_skips = sim_skip = 0;
  ChangeCensus();
  MustUpdateOptions = 1;

  /* yayaya */

  l = *(QUAD *)(MiscHis + 58);
  HALF_SWAP_LONGS(&l, 1);
  policePercent = l / 65536.0;

  l = *(QUAD *)(MiscHis + 60);
  HALF_SWAP_LONGS(&l, 1);
  firePercent = l / 65536.0;

  l = *(QUAD *)(MiscHis + 62);
  HALF_SWAP_LONGS(&l, 1);
  roadPercent = l / 65536.0;

  policePercent = (*(QUAD*)(MiscHis + 58)) / 65536.0;	/* and 59 */
  firePercent = (*(QUAD*)(MiscHis + 60)) / 65536.0;	/* and 61 */
  roadPercent =(*(QUAD*)(MiscHis + 62)) / 65536.0;	/* and 63 */

  if (CityTime < 0)
    CityTime = 0;
  if ((CityTax > 20) || (CityTax < 0))
    CityTax = 7;
  /* ignore the speed stored in the file (often 0): a loaded city always
   * starts running slow, never paused */
  setSpeed(1);
  setSkips(0);

  InitFundingLevel();

  /* set the scenario id to 0 */
  InitWillStuff();
  ScenarioID = 0;
  InitSimLoad = 1;
  DoInitialEval = 0;
  DoSimInit();
  InvalidateEditors();
  InvalidateMaps();
}


int loadFile(char *filename)
{
  if (!filename) return 0;
  uint32_t size = 0;
  uint8_t *data = platform_load_data(filename, &size);
  bool free_data = false;

  if (!data || size < CTY_FILE_SIZE) {
    FILE *f = fopen(filename, "rb");
    if (f) {
      fseek(f, 0L, SEEK_END);
      long fsize = ftell(f);
      fseek(f, 0L, SEEK_SET);
      if (fsize >= CTY_FILE_SIZE) {
        uint8_t *buf = (uint8_t *)malloc(fsize);
        if (buf) {
          if (fread(buf, 1, fsize, f) == (size_t)fsize) {
            data = buf;
            size = (uint32_t)fsize;
            free_data = true;
          } else {
            free(buf);
          }
        }
      }
      fclose(f);
    }
  }

  if (!data || size < CTY_FILE_SIZE) {
    if (free_data && data) free(data);
    return 0;
  }

  if (_load_mem(data, size) == 0) {
    if (free_data && data) free(data);
    return 0;
  }
  _finish_load();

  /* Check Tiny Engine metadata in MiscHis[64..] */
  if (memcmp((const char *)(MiscHis + MISC_MAGIC_OFFSET), CTY_MAGIC, 8) == 0) {
    const char *saved_name = (const char *)(MiscHis + MISC_NAME_OFFSET);
    if (saved_name[0] != '\0') {
      char clean_name[33];
      strncpy(clean_name, saved_name, 32);
      clean_name[32] = '\0';
      setCityName(clean_name);
    }
  }

  if (free_data && data) free(data);
  return 1;
}


/* Load a city from an in-memory blob (embedded resource) rather than disk. */
int loadMem(const unsigned char *buf, QUAD size)
{
  if (_load_mem(buf, size) == 0)
    return 0;
  _finish_load();
  return 1;
}


int saveFile(char *filename)
{
  if (!filename) return 0;

  long l;
  l = TotalFunds;
  HALF_SWAP_LONGS(&l, 1);
  (*(QUAD *)(MiscHis + 50)) = l;

  l = CityTime;
  HALF_SWAP_LONGS(&l, 1);
  (*(QUAD *)(MiscHis + 8)) = l;

  MiscHis[52] = autoBulldoze;	/* flag for autoBulldoze */
  MiscHis[53] = autoBudget;	/* flag for autoBudget */
  MiscHis[54] = autoGo;		/* flag for autoGo */
  MiscHis[55] = UserSoundOn;	/* flag for the sound on/off */
  MiscHis[57] = SimSpeed;
  MiscHis[56] = CityTax;

  l = (int)(policePercent * 65536);
  HALF_SWAP_LONGS(&l, 1);
  (*(QUAD *)(MiscHis + 58)) = l;

  l = (int)(firePercent * 65536);
  HALF_SWAP_LONGS(&l, 1);
  (*(QUAD *)(MiscHis + 60)) = l;

  l = (int)(roadPercent * 65536);
  HALF_SWAP_LONGS(&l, 1);
  (*(QUAD *)(MiscHis + 62)) = l;

  /* Store Tiny Engine City Name in unused MiscHis space (indices 64..119) */
  memcpy((char *)(MiscHis + MISC_MAGIC_OFFSET), CTY_MAGIC, 8);
  memset((char *)(MiscHis + MISC_NAME_OFFSET), 0, 32);
  if (CityName && CityName[0]) {
    strncpy((char *)(MiscHis + MISC_NAME_OFFSET), CityName, 31);
  } else {
    strncpy((char *)(MiscHis + MISC_NAME_OFFSET), "Metropolis", 31);
  }

  /* 27KB: far too big for the Playdate's game stack, so build it on the heap */
  unsigned char *buf = (unsigned char *)calloc(1, CTY_FILE_SIZE);
  if (!buf) return 0;
  unsigned char *p = buf;

#define WR(arr, len) do { \
    memcpy(p, (arr), (len) * sizeof(short)); \
    SWAP_SHORTS((short *)p, (len)); \
    p += (len) * sizeof(short); \
  } while (0)

  WR(ResHis,       HISTLEN / 2);
  WR(ComHis,       HISTLEN / 2);
  WR(IndHis,       HISTLEN / 2);
  WR(CrimeHis,     HISTLEN / 2);
  WR(PollutionHis, HISTLEN / 2);
  WR(MoneyHis,     HISTLEN / 2);
  WR(MiscHis,      MISCHISTLEN / 2);
  WR(&Map[0][0],   WORLD_X * WORLD_Y);

#undef WR

  int ok = 0;

  /* 1. Try platform_save_data */
  if (platform_save_data(filename, buf, CTY_FILE_SIZE)) {
    ok = 1;
  } else {
    /* 2. Fallback to direct fopen for desktop */
    FILE *f = fopen(filename, "wb");
    if (f) {
      size_t written = fwrite(buf, 1, CTY_FILE_SIZE, f);
      fclose(f);
      ok = (written == CTY_FILE_SIZE);
    }
  }

  free(buf);
  return ok;
}


void
LoadScenario(short s)
{
  char *name, *fname;

  if (CityFileName != NULL) {
    ckfree(CityFileName);
    CityFileName = NULL;
  }

  SetGameLevel(0);

  if ((s < 1) || (s > 8)) s = 1;

  switch (s) {
  case 1:
    name = "Dullsville";
    fname = "snro.111";
    ScenarioID = 1;
    CityTime = ((1900 - 1900) * 48) + 2;
    SetFunds(5000);
    break;
  case 2:
    name = "San Francisco";
    fname = "snro.222";
    ScenarioID = 2;
    CityTime = ((1906 - 1900) * 48) + 2;
    SetFunds(20000);
    break;
  case 3:
    name = "Hamburg";
    fname = "snro.333";
    ScenarioID = 3;
    CityTime = ((1944 - 1900) * 48) + 2;
    SetFunds(20000);
    break;
  case 4:
    name = "Bern";
    fname = "snro.444";
    ScenarioID = 4;
    CityTime = ((1965 - 1900) * 48) + 2;
    SetFunds(20000);
    break;
  case 5:
    name = "Tokyo";
    fname = "snro.555";
    ScenarioID = 5;
    CityTime = ((1957 - 1900) * 48) + 2;
    SetFunds(20000);
    break;
  case 6:
    name = "Detroit";
    fname = "snro.666";
    ScenarioID = 6;
    CityTime = ((1972 - 1900) * 48) + 2;
    SetFunds(20000);
    break;
  case 7:
    name = "Boston";
    fname = "snro.777";
    ScenarioID = 7;
    CityTime = ((2010 - 1900) * 48) + 2;
    SetFunds(20000);
    break;
  case 8:
    name = "Rio de Janeiro";
    fname = "snro.888";
    ScenarioID = 8;
    CityTime = ((2047 - 1900) * 48) + 2;
    SetFunds(20000);
    break;
  }

  setAnyCityName(name);
  //  sim_skips = sim_skip = 0;
  InvalidateMaps();
  InvalidateEditors();
  setSpeed(1);
  CityTax = 7;
  gettimeofday(&start_time, NULL);

  {
    unsigned int sz;
    const unsigned char *d = res_find(fname, &sz);
    if (d != NULL)
      _load_mem(d, sz);
  }

  InitWillStuff();
  InitFundingLevel();
  UpdateFunds();
  InvalidateEditors();
  InvalidateMaps();
  InitSimLoad = 1;
  DoInitialEval = 0;
  DoSimInit();
  DidLoadScenario();
  Kick();
}


void
DidLoadScenario(void)
{
  Eval("UIDidLoadScenario");
}


int LoadCity(char *filename)
{
  char *cp;
  char msg[256];

  if (!filename) return 0;

  if (loadFile(filename)) {
    if (CityFileName != NULL)
      ckfree(CityFileName);
    CityFileName = (char *)ckalloc(strlen(filename) + 1);
    strcpy(CityFileName, filename);

    if (!CityName || !CityName[0]) {
      char tmp[128];
      strncpy(tmp, filename, sizeof(tmp) - 1);
      tmp[sizeof(tmp) - 1] = '\0';
      if ((cp = strrchr(tmp, '.')))
        *cp = 0;
      if ((cp = strrchr(tmp, '/')))
        cp++;
      else if ((cp = strrchr(tmp, '\\')))
        cp++;
      else
        cp = tmp;
      setCityName(cp);
    }
    gettimeofday(&start_time, NULL);

    InvalidateMaps();
    InvalidateEditors();
    DidLoadCity();
    return (1);
  } else {
    sprintf(msg, "Unable to load city from \"%s\".", filename ? filename : "(null)");
    DidntLoadCity(msg);
    return (0);
  }
}


/* Load one of the baked-in example cities (cities/...cty) by basename, e.g.
 * "about.cty".  Mirrors LoadCity but reads from the embedded blob and leaves
 * CityFileName NULL, so Save / Save As prompt for a path -- an embedded city
 * is fully saveable to disk. */
int LoadEmbeddedCity(char *name)
{
  unsigned int sz;
  const unsigned char *d = city_find(name, &sz);
  char *nm, *cp;
  char msg[256];

  if ((d != NULL) && loadMem(d, sz)) {
    if (CityFileName != NULL) {
      ckfree(CityFileName);
      CityFileName = NULL;			/* force Save As (no disk path) */
    }

    /* persistent, writable copy of "<name>" minus its extension: setCityName
     * stores the pointer and rewrites it in place, so it must not be a stack
     * buffer nor the read-only table string. */
    nm = (char *)ckalloc(strlen(name) + 1);
    strcpy(nm, name);
    if ((cp = strrchr(nm, '.')))
      *cp = 0;
    setCityName(nm);
    gettimeofday(&start_time, NULL);

    InvalidateMaps();
    InvalidateEditors();
    DidLoadCity();
    return (1);
  } else {
    sprintf(msg, "Unable to load the built-in city \"%s\".",
	    name ? name : "(null)");
    DidntLoadCity(msg);
    return (0);
  }
}


void
DidLoadCity(void)
{
  Eval("UIDidLoadCity");
}


void
DidntLoadCity(char *msg)
{
  char buf[1024];
  sprintf(buf, "UIDidntLoadCity {%s}", msg);
  Eval(buf);
}


int
SaveCity(void)
{
  char msg[256];

  if (CityFileName == NULL) {
    return SaveCityAs("city_slot1.cty");
  } else {
    if (saveFile(CityFileName)) {
      DidSaveCity();
      return (1);
    } else {
      sprintf(msg, "Unable to save city to \"%s\".",
	      CityFileName ? CityFileName : "(null)");
      DidntSaveCity(msg);
    }
  }
  return (0);
}


void
DoSaveCityAs(void)
{
  Eval("UISaveCityAs");
}


void
DidSaveCity(void)
{
  Eval("UIDidSaveCity");
}


void
DidntSaveCity(char *msg)
{
  char buf[1024];
  sprintf(buf, "UIDidntSaveCity {%s}", msg);
  Eval(buf);
}


int
SaveCityAs(char *filename)
{
  char msg[256];

  if (!filename) return 0;
  if (CityFileName != NULL)
    ckfree(CityFileName);
  CityFileName = (char *)ckalloc(strlen(filename) + 1);
  strcpy(CityFileName, filename);

  if (saveFile(CityFileName)) {
    DidSaveCity();
    return (1);
  } else {
    sprintf(msg, "Unable to save city to \"%s\".",
	    CityFileName ? CityFileName : "(null)");
    DidntSaveCity(msg);
  }
  return (0);
}


int city_read_meta(const char *filename, city_meta_t *meta)
{
  if (!filename || !meta) return 0;
  memset(meta, 0, sizeof(city_meta_t));

  uint32_t size = 0;
  uint8_t *data = platform_load_data(filename, &size);
  bool free_data = false;

  if (!data || size < CTY_FILE_SIZE) {
    FILE *f = fopen(filename, "rb");
    if (f) {
      fseek(f, 0L, SEEK_END);
      long fsize = ftell(f);
      fseek(f, 0L, SEEK_SET);
      if (fsize >= CTY_FILE_SIZE) {
        uint8_t *buf = (uint8_t *)malloc(fsize);
        if (buf) {
          if (fread(buf, 1, fsize, f) == (size_t)fsize) {
            data = buf;
            size = (uint32_t)fsize;
            free_data = true;
          } else {
            free(buf);
          }
        }
      }
      fclose(f);
    }
  }

  if (!data || size < CTY_FILE_SIZE) {
    if (free_data && data) free(data);
    meta->exists = 0;
    return 0;
  }

  meta->exists = 1;

  /* MiscHis is located at offset: 6 * (HISTLEN / 2) * sizeof(short) = 2880 bytes */
  short misc[MISCHISTLEN / 2];
  memcpy(misc, data + (6 * (HISTLEN / 2) * sizeof(short)), sizeof(misc));
  SWAP_SHORTS(misc, MISCHISTLEN / 2);

  long funds = *(const QUAD *)(misc + 50);
  HALF_SWAP_LONGS(&funds, 1);
  meta->funds = funds;

  long time = *(const QUAD *)(misc + 8);
  HALF_SWAP_LONGS(&time, 1);
  meta->year = 1900 + (int)(time / 48);
  meta->month = (int)((time % 48) / 4);

  short res_pop = misc[2];
  short com_pop = misc[3];
  short ind_pop = misc[4];
  meta->population = (res_pop + com_pop + ind_pop) * 100;
  meta->difficulty = misc[15];
  meta->score = misc[17];

  /* Check for Tiny Engine City Name in MiscHis[64..] */
  if (memcmp((const char *)(misc + MISC_MAGIC_OFFSET), CTY_MAGIC, 8) == 0) {
    const char *saved_name = (const char *)(misc + MISC_NAME_OFFSET);
    if (saved_name[0] != '\0') {
      strncpy(meta->name, saved_name, sizeof(meta->name) - 1);
      meta->name[sizeof(meta->name) - 1] = '\0';
      if (free_data) free(data);
      return 1;
    }
  }

  /* Default name from file basename */
  const char *base = strrchr(filename, '/');
  if (!base) base = strrchr(filename, '\\');
  base = base ? base + 1 : filename;
  strncpy(meta->name, base, sizeof(meta->name) - 1);
  meta->name[sizeof(meta->name) - 1] = '\0';
  char *dot = strrchr(meta->name, '.');
  if (dot) *dot = '\0';

  if (free_data) free(data);
  return 1;
}

int city_get_slot_path(int slot, char *out_path, size_t max_len)
{
  if (slot < 1 || slot > CITY_MAX_SLOTS || !out_path) return 0;
  snprintf(out_path, max_len, "city_slot%d.cty", slot);
  return 1;
}

int city_save_slot(int slot)
{
  char path[64];
  if (!city_get_slot_path(slot, path, sizeof(path))) return 0;
  return SaveCityAs(path);
}

int city_load_slot(int slot)
{
  char path[64];
  if (!city_get_slot_path(slot, path, sizeof(path))) return 0;
  return LoadCity(path);
}

int city_get_slot_meta(int slot, city_meta_t *meta)
{
  char path[64];
  if (!city_get_slot_path(slot, path, sizeof(path))) return 0;
  return city_read_meta(path, meta);
}


