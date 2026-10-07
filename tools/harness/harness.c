/* SPDX-License-Identifier: GPL-3.0-or-later
 * Part of micropolis_pd, a modified version of Micropolis. See NOTICE.md. */
/*
 * Headless Playdate API emulation for scripted screenshot tests.
 *
 *   harness <bundle_dir> <raw_asset_dir> <data_dir> <script> <out_dir>
 *
 * Implements just enough of pd_api for the Micropolis shim: 1-bit graphics
 * with masks / patterns / draw modes / clip rects / offscreen contexts,
 * buttons + crank from a script, file I/O, menu items, silent sound.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <execinfo.h>
#include <mach-o/dyld.h>
#include <unistd.h>

#include "pd_api.h"

int eventHandler(PlaydateAPI *pd, PDSystemEvent event, uint32_t arg);

/* Game hooks for the `city` command */
typedef struct scene_t scene_t;
void game_start_embedded_city(const char *name);
scene_t *game_get_scene(void);
void scene_set(scene_t *scene);

/* ------------------------------------------------------------------ */
/* Bitmaps                                                             */

struct LCDBitmap {
    int w, h;
    uint8_t *px;   /* 0 black, 1 white */
    uint8_t *mask; /* 1 opaque; NULL = fully opaque */
};

static LCDBitmap s_frame;
static LCDBitmap *s_ctx_stack[16];
static int s_ctx_depth = 0;
static int s_clip_x0, s_clip_y0, s_clip_x1, s_clip_y1;
static bool s_clip_on = false;
static LCDBitmapDrawMode s_draw_mode = kDrawModeCopy;
static int s_draw_off_x = 0, s_draw_off_y = 0;

static const char *s_bundle_dir, *s_raw_dir, *s_data_dir;
static long s_frame_no = 0;

static LCDBitmap *target(void) { return s_ctx_depth ? s_ctx_stack[s_ctx_depth - 1] : &s_frame; }

static LCDBitmap *bmp_alloc(int w, int h, bool mask)
{
    LCDBitmap *b = calloc(1, sizeof(LCDBitmap));
    b->w = w;
    b->h = h;
    b->px = calloc((size_t)w * h, 1);
    if (mask) b->mask = calloc((size_t)w * h, 1);
    return b;
}

static bool clip_ok(LCDBitmap *t, int x, int y)
{
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return false;
    if (s_clip_on && (x < s_clip_x0 || y < s_clip_y0 || x >= s_clip_x1 || y >= s_clip_y1)) return false;
    return true;
}

static void put(LCDBitmap *t, int x, int y, int v)
{
    if (!clip_ok(t, x, y)) return;
    t->px[y * t->w + x] = (uint8_t)v;
    if (t->mask) t->mask[y * t->w + x] = 1;
}

static void put_color(int x, int y, LCDColor c)
{
    LCDBitmap *t = target();
    x += s_draw_off_x;
    y += s_draw_off_y;
    if (!clip_ok(t, x, y)) return;
    if (c == kColorBlack) put(t, x, y, 0);
    else if (c == kColorWhite) put(t, x, y, 1);
    else if (c == kColorClear) {
        if (t->mask) t->mask[y * t->w + x] = 0;
    } else if (c == kColorXOR) {
        t->px[y * t->w + x] ^= 1;
    } else {
        const uint8_t *p = (const uint8_t *)c;
        int bit = 0x80 >> (x & 7);
        if (!(p[8 + (y & 7)] & bit)) return;
        put(t, x, y, (p[y & 7] & bit) ? 1 : 0);
    }
}

/* ------------------------------------------------------------------ */
/* Graphics API                                                        */

static void g_clear(LCDColor c)
{
    LCDBitmap *t = target();
    bool clip = s_clip_on;
    s_clip_on = false;
    int ox = s_draw_off_x, oy = s_draw_off_y;
    s_draw_off_x = s_draw_off_y = 0;
    for (int y = 0; y < t->h; y++)
        for (int x = 0; x < t->w; x++) put_color(x, y, c);
    s_clip_on = clip;
    s_draw_off_x = ox;
    s_draw_off_y = oy;
}

static void g_fillRect(int x, int y, int w, int h, LCDColor c)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) put_color(i, j, c);
}

static void g_drawLine(int x1, int y1, int x2, int y2, int width, LCDColor c)
{
    (void)width;
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        put_color(x1, y1, c);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

static void g_drawRect(int x, int y, int w, int h, LCDColor c)
{
    if (w <= 0 || h <= 0) return;
    g_fillRect(x, y, w, 1, c);
    g_fillRect(x, y + h - 1, w, 1, c);
    g_fillRect(x, y + 1, 1, h - 2, c);
    g_fillRect(x + w - 1, y + 1, 1, h - 2, c);
}

static void g_setPixel(int x, int y, LCDColor c) { put_color(x, y, c); }

static void g_setClipRect(int x, int y, int w, int h)
{
    x += s_draw_off_x;
    y += s_draw_off_y;
    s_clip_on = true;
    s_clip_x0 = x;
    s_clip_y0 = y;
    s_clip_x1 = x + w;
    s_clip_y1 = y + h;
}

static void g_clearClipRect(void) { s_clip_on = false; }

static LCDBitmapDrawMode g_setDrawMode(LCDBitmapDrawMode m)
{
    LCDBitmapDrawMode old = s_draw_mode;
    s_draw_mode = m;
    return old;
}

static void g_setDrawOffset(int dx, int dy) { s_draw_off_x = dx; s_draw_off_y = dy; }

static void g_drawBitmap(LCDBitmap *b, int x, int y, LCDBitmapFlip flip)
{
    if (!b) return;
    LCDBitmap *t = target();
    x += s_draw_off_x;
    y += s_draw_off_y;
    /* Restrict the loop to the visible/clip window */
    int x0 = 0, y0 = 0, x1 = t->w, y1 = t->h;
    if (s_clip_on) {
        if (s_clip_x0 > x0) x0 = s_clip_x0;
        if (s_clip_y0 > y0) y0 = s_clip_y0;
        if (s_clip_x1 < x1) x1 = s_clip_x1;
        if (s_clip_y1 < y1) y1 = s_clip_y1;
    }
    if (x > x0) x0 = x;
    if (y > y0) y0 = y;
    if (x + b->w < x1) x1 = x + b->w;
    if (y + b->h < y1) y1 = y + b->h;

    for (int dy = y0; dy < y1; dy++) {
        for (int dx = x0; dx < x1; dx++) {
            int sx = dx - x, sy = dy - y;
            if (flip == kBitmapFlippedX || flip == kBitmapFlippedXY) sx = b->w - 1 - sx;
            if (flip == kBitmapFlippedY || flip == kBitmapFlippedXY) sy = b->h - 1 - sy;
            int i = sy * b->w + sx;
            if (b->mask && !b->mask[i]) continue;
            int v = b->px[i];
            int di = dy * t->w + dx;
            switch (s_draw_mode) {
            case kDrawModeCopy: t->px[di] = (uint8_t)v; break;
            case kDrawModeWhiteTransparent: if (v == 0) t->px[di] = 0; else continue; break;
            case kDrawModeBlackTransparent: if (v == 1) t->px[di] = 1; else continue; break;
            case kDrawModeFillWhite: t->px[di] = 1; break;
            case kDrawModeFillBlack: t->px[di] = 0; break;
            case kDrawModeXOR: t->px[di] ^= (uint8_t)v; break;
            case kDrawModeNXOR: t->px[di] ^= (uint8_t)(!v); break;
            case kDrawModeInverted: t->px[di] = (uint8_t)!v; break;
            }
            if (t->mask) t->mask[di] = 1;
        }
    }
}

static LCDBitmap *g_newBitmap(int w, int h, LCDColor bg)
{
    LCDBitmap *b = bmp_alloc(w, h, bg == kColorClear);
    if (bg == kColorWhite) memset(b->px, 1, (size_t)w * h);
    return b;
}

static void g_freeBitmap(LCDBitmap *b)
{
    if (!b) return;
    free(b->px);
    free(b->mask);
    free(b);
}

static void g_clearBitmap(LCDBitmap *b, LCDColor bg)
{
    memset(b->px, bg == kColorWhite ? 1 : 0, (size_t)b->w * b->h);
    if (b->mask) memset(b->mask, bg == kColorClear ? 0 : 1, (size_t)b->w * b->h);
}

static LCDBitmap *g_loadBitmap(const char *path, const char **err)
{
    char full[1024];
    snprintf(full, sizeof(full), "%s/%s.raw", s_raw_dir, path);
    FILE *f = fopen(full, "rb");
    if (!f) {
        if (err) *err = "file not found";
        return NULL;
    }
    int w, h, has_mask;
    if (fscanf(f, "%d %d %d", &w, &h, &has_mask) != 3) {
        fclose(f);
        if (err) *err = "bad header";
        return NULL;
    }
    fgetc(f);
    LCDBitmap *b = bmp_alloc(w, h, has_mask != 0);
    for (int i = 0; i < w * h; i++) {
        int v = fgetc(f);
        b->px[i] = (v == 1);
        if (b->mask) b->mask[i] = (v != 2);
    }
    fclose(f);
    return b;
}

static void g_getBitmapData(LCDBitmap *b, int *w, int *h, int *rb, uint8_t **mask, uint8_t **data)
{
    if (w) *w = b->w;
    if (h) *h = b->h;
    if (rb) *rb = (b->w + 7) / 8;
    if (mask) *mask = NULL;
    if (data) *data = NULL;
}

static void g_pushContext(LCDBitmap *b)
{
    s_ctx_stack[s_ctx_depth++] = b ? b : &s_frame;
}

static void g_popContext(void)
{
    if (s_ctx_depth) s_ctx_depth--;
}

static void g_drawScaledBitmap(LCDBitmap *b, int x, int y, float xs, float ys)
{
    if (xs == 1.0f && ys == 1.0f) {
        g_drawBitmap(b, x, y, kBitmapUnflipped);
        return;
    }
    LCDBitmap *t = target();
    int w = (int)(b->w * xs), h = (int)(b->h * ys);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int sx = (int)(i / xs), sy = (int)(j / ys);
            int si = sy * b->w + sx;
            if (b->mask && !b->mask[si]) continue;
            put(t, x + i, y + j, b->px[si]);
        }
}

static void g_display(void) {}
static void g_markUpdatedRows(int a, int b) { (void)a; (void)b; }

/* ------------------------------------------------------------------ */
/* System API                                                          */

static PDButtons s_held = 0, s_prev_held = 0;
static float s_crank_queue = 0.0f;
static int s_crank_frames = 0;
static int s_docked = 0;
static PDCallbackFunction *s_update_cb = NULL;
static void *s_update_ud = NULL;

typedef struct {
    char title[64];
    PDMenuItemCallbackFunction *cb;
    void *ud;
    int value;
    int options;
} menu_item_t;
static menu_item_t s_menu[8];
static int s_menu_count = 0;

static void *sys_realloc(void *p, size_t n)
{
    if (n == 0) {
        free(p);
        return NULL;
    }
    return realloc(p, n);
}

static void sys_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[pd] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void sys_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[pd ERROR] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

static unsigned int sys_ms(void) { return (unsigned int)(s_frame_no * 1000 / 30); }
static unsigned int sys_epoch(unsigned int *ms)
{
    if (ms) *ms = (unsigned int)((s_frame_no * 1000 / 30) % 1000);
    return 800000000u + (unsigned int)(s_frame_no / 30);
}
static float sys_elapsed(void) { return s_frame_no / 30.0f; }
static void sys_reset_elapsed(void) {}

static void sys_setUpdateCallback(PDCallbackFunction *cb, void *ud)
{
    s_update_cb = cb;
    s_update_ud = ud;
}

static void sys_getButtonState(PDButtons *cur, PDButtons *pushed, PDButtons *released)
{
    if (cur) *cur = s_held;
    if (pushed) *pushed = s_held & ~s_prev_held;
    if (released) *released = s_prev_held & ~s_held;
}

static float s_crank_this_frame = 0.0f;
static float sys_getCrankChange(void) { return s_crank_this_frame; }
static float sys_getCrankAngle(void) { return 0.0f; }
static int sys_isCrankDocked(void) { return s_docked; }
static int sys_setCrankSoundsDisabled(int f) { (void)f; return 0; }
static void sys_setAutoLockDisabled(int d) { (void)d; }
static void sys_drawFPS(int x, int y) { (void)x; (void)y; }

static PDMenuItem *add_menu(const char *title, PDMenuItemCallbackFunction *cb, void *ud, int options)
{
    menu_item_t *m = &s_menu[s_menu_count];
    strncpy(m->title, title, sizeof(m->title) - 1);
    m->cb = cb;
    m->ud = ud;
    m->options = options;
    return (PDMenuItem *)(intptr_t)(++s_menu_count);
}

static PDMenuItem *sys_addMenuItem(const char *t, PDMenuItemCallbackFunction *cb, void *ud) { return add_menu(t, cb, ud, 0); }
static PDMenuItem *sys_addCheckmarkMenuItem(const char *t, int v, PDMenuItemCallbackFunction *cb, void *ud)
{
    PDMenuItem *m = add_menu(t, cb, ud, 2);
    s_menu[s_menu_count - 1].value = v;
    return m;
}
static PDMenuItem *sys_addOptionsMenuItem(const char *t, const char **opts, int n, PDMenuItemCallbackFunction *cb, void *ud)
{
    (void)opts;
    return add_menu(t, cb, ud, n);
}
static int sys_getMenuItemValue(PDMenuItem *m) { return s_menu[(intptr_t)m - 1].value; }
static void sys_setMenuItemValue(PDMenuItem *m, int v) { s_menu[(intptr_t)m - 1].value = v; }
static void sys_removeAllMenuItems(void) { s_menu_count = 0; }
static void sys_setMenuImage(LCDBitmap *b, int x) { (void)b; (void)x; }
static int sys_getReduceFlashing(void) { return 0; }

/* ------------------------------------------------------------------ */
/* File API                                                            */

static void resolve(const char *name, FileOptions mode, char *out, size_t n)
{
    char data_path[1024], bundle_path[1024];
    snprintf(data_path, sizeof(data_path), "%s/%s", s_data_dir, name);
    snprintf(bundle_path, sizeof(bundle_path), "%s/%s", s_bundle_dir, name);
    struct stat st;
    if ((mode & kFileWrite) || (mode & kFileAppend)) {
        snprintf(out, n, "%s", data_path);
    } else if ((mode & kFileReadData) && stat(data_path, &st) == 0) {
        snprintf(out, n, "%s", data_path);
    } else if (mode & kFileRead) {
        snprintf(out, n, "%s", bundle_path);
    } else {
        snprintf(out, n, "%s", data_path);
    }
}

static SDFile *f_open(const char *name, FileOptions mode)
{
    char p[1024];
    resolve(name, mode, p, sizeof(p));
    const char *m = (mode & kFileWrite) ? "wb" : (mode & kFileAppend) ? "ab" : "rb";
    return (SDFile *)fopen(p, m);
}
static int f_close(SDFile *f) { return fclose((FILE *)f); }
static int f_read(SDFile *f, void *buf, unsigned int len) { return (int)fread(buf, 1, len, (FILE *)f); }
static int f_write(SDFile *f, const void *buf, unsigned int len) { return (int)fwrite(buf, 1, len, (FILE *)f); }
static int f_flush(SDFile *f) { return fflush((FILE *)f); }
static int f_tell(SDFile *f) { return (int)ftell((FILE *)f); }
static int f_seek(SDFile *f, int pos, int whence) { return fseek((FILE *)f, pos, whence); }
static const char *f_geterr(void) { return "harness file error"; }
static int f_stat(const char *name, FileStat *fs)
{
    char p[1024];
    struct stat st;
    snprintf(p, sizeof(p), "%s/%s", s_data_dir, name);
    if (stat(p, &st) != 0) {
        snprintf(p, sizeof(p), "%s/%s", s_bundle_dir, name);
        if (stat(p, &st) != 0) return -1;
    }
    memset(fs, 0, sizeof(*fs));
    fs->isdir = S_ISDIR(st.st_mode);
    fs->size = (unsigned int)st.st_size;
    return 0;
}
static int f_mkdir(const char *p) { (void)p; return 0; }
static int f_unlink(const char *name, int r)
{
    (void)r;
    char p[1024];
    snprintf(p, sizeof(p), "%s/%s", s_data_dir, name);
    return remove(p);
}

/* ------------------------------------------------------------------ */
/* Sound API (silent)                                                  */

static AudioSample *snd_load(const char *path) { (void)path; return (AudioSample *)(intptr_t)1; }
static void snd_free(AudioSample *s) { (void)s; }
static SamplePlayer *sp_new(void) { return (SamplePlayer *)(intptr_t)1; }
static void sp_free(SamplePlayer *p) { (void)p; }
static void sp_setSample(SamplePlayer *p, AudioSample *s) { (void)p; (void)s; }
static int sp_play(SamplePlayer *p, int r, float rate) { (void)p; (void)r; (void)rate; return 1; }
static int sp_isPlaying(SamplePlayer *p) { (void)p; return 0; }
static void sp_stop(SamplePlayer *p) { (void)p; }
static void sp_setVolume(SamplePlayer *p, float l, float r) { (void)p; (void)l; (void)r; }

static void disp_setRefreshRate(float r) { (void)r; }

/* ------------------------------------------------------------------ */

static struct playdate_graphics G;
static struct playdate_sys S;
static struct playdate_file F;
static struct playdate_sound SND;
static struct playdate_sound_sample SAMPLE;
static struct playdate_sound_sampleplayer SPLAYER;
static struct playdate_display D;
static PlaydateAPI API;

static void setup_api(void)
{
    G.clear = g_clear;
    G.fillRect = g_fillRect;
    G.drawRect = g_drawRect;
    G.drawLine = g_drawLine;
    G.setPixel = g_setPixel;
    G.setClipRect = g_setClipRect;
    G.clearClipRect = g_clearClipRect;
    G.setDrawMode = g_setDrawMode;
    G.setDrawOffset = g_setDrawOffset;
    G.drawBitmap = g_drawBitmap;
    G.drawScaledBitmap = g_drawScaledBitmap;
    G.newBitmap = g_newBitmap;
    G.freeBitmap = g_freeBitmap;
    G.clearBitmap = g_clearBitmap;
    G.loadBitmap = g_loadBitmap;
    G.getBitmapData = g_getBitmapData;
    G.pushContext = g_pushContext;
    G.popContext = g_popContext;
    G.display = g_display;
    G.markUpdatedRows = g_markUpdatedRows;

    S.realloc = sys_realloc;
    S.logToConsole = sys_log;
    S.error = sys_error;
    S.getCurrentTimeMilliseconds = sys_ms;
    S.getSecondsSinceEpoch = sys_epoch;
    S.getElapsedTime = sys_elapsed;
    S.resetElapsedTime = sys_reset_elapsed;
    S.setUpdateCallback = sys_setUpdateCallback;
    S.getButtonState = sys_getButtonState;
    S.getCrankChange = sys_getCrankChange;
    S.getCrankAngle = sys_getCrankAngle;
    S.isCrankDocked = sys_isCrankDocked;
    S.setCrankSoundsDisabled = sys_setCrankSoundsDisabled;
    S.setAutoLockDisabled = sys_setAutoLockDisabled;
    S.drawFPS = sys_drawFPS;
    S.addMenuItem = sys_addMenuItem;
    S.addCheckmarkMenuItem = sys_addCheckmarkMenuItem;
    S.addOptionsMenuItem = sys_addOptionsMenuItem;
    S.getMenuItemValue = sys_getMenuItemValue;
    S.setMenuItemValue = sys_setMenuItemValue;
    S.removeAllMenuItems = sys_removeAllMenuItems;
    S.setMenuImage = sys_setMenuImage;
    S.getReduceFlashing = sys_getReduceFlashing;

    F.open = f_open;
    F.close = f_close;
    F.read = f_read;
    F.write = f_write;
    F.flush = f_flush;
    F.tell = f_tell;
    F.seek = f_seek;
    F.stat = f_stat;
    F.geterr = f_geterr;
    F.mkdir = f_mkdir;
    F.unlink = f_unlink;

    SAMPLE.load = snd_load;
    SAMPLE.freeSample = snd_free;
    SPLAYER.newPlayer = sp_new;
    SPLAYER.freePlayer = sp_free;
    SPLAYER.setSample = sp_setSample;
    SPLAYER.play = sp_play;
    SPLAYER.isPlaying = sp_isPlaying;
    SPLAYER.stop = sp_stop;
    SPLAYER.setVolume = sp_setVolume;
    SND.sample = &SAMPLE;
    SND.sampleplayer = &SPLAYER;

    D.setRefreshRate = disp_setRefreshRate;

    API.graphics = &G;
    API.system = &S;
    API.file = &F;
    API.sound = &SND;
    API.display = &D;
}

static void run_frame(void)
{
    if (s_crank_frames > 0) {
        s_crank_this_frame = s_crank_queue / s_crank_frames;
        s_crank_queue -= s_crank_this_frame;
        s_crank_frames--;
    } else {
        s_crank_this_frame = 0.0f;
    }
    if (s_update_cb) s_update_cb(s_update_ud);
    s_prev_held = s_held;
    s_frame_no++;
}

static void shot(const char *out_dir, const char *name)
{
    char p[1024];
    snprintf(p, sizeof(p), "%s/%s.pgm", out_dir, name);
    FILE *f = fopen(p, "wb");
    if (!f) return;
    fprintf(f, "P5\n%d %d\n255\n", s_frame.w, s_frame.h);
    for (int i = 0; i < s_frame.w * s_frame.h; i++) fputc(s_frame.px[i] ? 255 : 0, f);
    fclose(f);
    fprintf(stderr, "shot %s (frame %ld)\n", name, s_frame_no);
}

static PDButtons button_of(const char *s)
{
    if (!strcmp(s, "a") || !strcmp(s, "A")) return kButtonA;
    if (!strcmp(s, "b") || !strcmp(s, "B")) return kButtonB;
    if (!strcmp(s, "up")) return kButtonUp;
    if (!strcmp(s, "down")) return kButtonDown;
    if (!strcmp(s, "left")) return kButtonLeft;
    if (!strcmp(s, "right")) return kButtonRight;
    fprintf(stderr, "unknown button %s\n", s);
    exit(2);
}

#include <sys/ucontext.h>
static void on_crash_info(int sig, siginfo_t *si, void *ctx)
{
    ucontext_t *uc = (ucontext_t *)ctx;
    fprintf(stderr, "FAULT pc=0x%llx lr=0x%llx addr=%p slide=0x%lx\n",
            (unsigned long long)uc->uc_mcontext->__ss.__pc,
            (unsigned long long)uc->uc_mcontext->__ss.__lr, si->si_addr,
            (long)_dyld_get_image_vmaddr_slide(0));
    (void)sig;
    _exit(1);
}

static void on_crash(int sig)
{
    void *bt[64];
    int n = backtrace(bt, 64);
    fprintf(stderr, "CRASH signal %d at frame %ld slide 0x%lx\n", sig, s_frame_no, (long)_dyld_get_image_vmaddr_slide(0));
    backtrace_symbols_fd(bt, n, 2);
    _exit(1);
}

int main(int argc, char **argv)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash_info;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    signal(SIGABRT, on_crash);
    if (argc < 6) {
        fprintf(stderr, "usage: harness bundle raw data script out\n");
        return 2;
    }
    s_bundle_dir = argv[1];
    s_raw_dir = argv[2];
    s_data_dir = argv[3];
    const char *out_dir = argv[5];

    s_frame.w = LCD_COLUMNS;
    s_frame.h = LCD_ROWS;
    s_frame.px = calloc(LCD_COLUMNS * LCD_ROWS, 1);

    setup_api();
    eventHandler(&API, kEventInit, 0);

    FILE *script = fopen(argv[4], "r");
    if (!script) {
        perror("script");
        return 2;
    }
    char line[256];
    while (fgets(line, sizeof(line), script)) {
        char cmd[32] = "", a1[64] = "", a2[64] = "";
        int n = sscanf(line, "%31s %63s %63s", cmd, a1, a2);
        if (n <= 0 || cmd[0] == '#') continue;
        if (!strcmp(cmd, "frames")) {
            int k = atoi(a1);
            for (int i = 0; i < k; i++) run_frame();
        } else if (!strcmp(cmd, "hold")) {
            s_held |= button_of(a1);
        } else if (!strcmp(cmd, "release")) {
            s_held &= ~button_of(a1);
        } else if (!strcmp(cmd, "tap")) {
            int times = n >= 3 ? atoi(a2) : 1;
            for (int t = 0; t < times; t++) {
                s_held |= button_of(a1);
                run_frame();
                s_held &= ~button_of(a1);
                run_frame();
                run_frame();
            }
        } else if (!strcmp(cmd, "crank")) {
            s_crank_queue += (float)atof(a1);
            s_crank_frames = n >= 3 ? atoi(a2) : 10;
            int k = s_crank_frames;
            for (int i = 0; i < k + 2; i++) run_frame();
        } else if (!strcmp(cmd, "dock")) {
            s_docked = atoi(a1);
        } else if (!strcmp(cmd, "menu")) {
            int idx = atoi(a1);
            if (idx >= 0 && idx < s_menu_count) {
                if (n >= 3) s_menu[idx].value = atoi(a2);
                if (s_menu[idx].cb) s_menu[idx].cb(s_menu[idx].ud);
            }
            run_frame();
        } else if (!strcmp(cmd, "city")) {
            /* city NAME.cty: load an embedded example city and enter the game */
            game_start_embedded_city(a1);
            scene_set(game_get_scene());
            run_frame();
        } else if (!strcmp(cmd, "env")) {
            setenv(a1, a2, 1);
        } else if (!strcmp(cmd, "shot")) {
            shot(out_dir, a1);
        } else {
            fprintf(stderr, "unknown command %s\n", cmd);
        }
    }
    fclose(script);
    return 0;
}
