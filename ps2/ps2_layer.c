/* ps2_layer.c: Build's base layer on maxsdk (sdlayer2.c's calls): main, the
   timer on the EE's counter, the screen (libgl: the TV's mode, NTSC 640 x 448
   or PAL 640 x 512, or 1080i, with or without beam racing, in 16 or 24-bit
   colour: the Video Settings menu), pad 0 as the keyboard and mouse the game
   knows. */

#include "build.h"
#include "baselayer.h"
#include "engine_priv.h"
#include "baselayer_priv.h"
#include "cache1d.h"
#include "pragmas.h"
#include "a.h"
#include "osd.h"
#include "glbuild_priv.h"
#include "polymost_priv.h"

#include <mx/mx.h>
#include <mx/frame.h>
#include <mx/gs.h>
#include <mx/hires.h>
#include <mx/vm.h>
#include <mx/pad.h>
#include <mx/prof.h>
#include <mxgl.h>
#include <stdarg.h>
#include <unistd.h>

#include "runcfg.h"
#include "ps2_port.h"

void mx_exit(int status) __attribute__((noreturn));

void ps2_end(int status) {
  mx_printf("@@PS2TEST_END@@ status=%d\n", status);
  mx_exit(status);
}

/* ---- time: the EE's counter, wrapped into 64 bits ------------------------ */

static u64 cycles(void) {
  static u32 last, high;
  u32 c = mx_count();
  if (c < last)
    high++;
  last = c;
  return (u64)high << 32 | c;
}

static void (*usertimer)(void);
static unsigned timerrate;
static u64 timerlast;

int inittimer(int rate, void (*cb)(void)) {
  timerrate = rate;
  usertimer = cb;
  timerlast = cycles() * rate / MX_EE_HZ;
  return 0;
}
void uninittimer(void) { timerrate = 0; }
void sampletimer(void) {
  if (!timerrate)
    return;
  u64 now = cycles() * timerrate / MX_EE_HZ;
  int n = (int)(now - timerlast);
  if (n > 0) {
    totalclock += n;
    timerlast = now;
    if (usertimer)
      for (; n > 0; n--)
        usertimer();
  }
}
unsigned int getticks(void) { return (unsigned)(cycles() / (MX_EE_HZ / 1000)); }
unsigned int getusecticks(void) { return (unsigned)(cycles() / (MX_EE_HZ / 1000000)); }
int gettimerfreq(void) { return timerrate; }

/* ---- the system ----------------------------------------------------------- */

#if PS2_PLAY
#include <mx/timer.h>
static void watch(void *arg);
#endif
int initsystem(void) {
  glunavailable = 0;
  mxgl_init();
  mxgl_show_stats = PS2_STATS;
#if PS2_PLAY
  mx_timer_periodic(0, 57600, watch, NULL); /* 100 ms */
#endif
  return 0;
}
void uninitsystem(void) {}
void initputs(const char *s) { mx_printf("%s", s); }
void debugprintf(const char *f, ...) {}
int wm_msgbox(const char *name, const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  mx_printf("%s: %s\n", name ? name : "msg", buf);
  return 1;
}
int wm_ynbox(const char *name, const char *fmt, ...) { return 1; }
int wm_filechooser(const char *a, const char *b, const char *c, int d, char **e) { return -1; }
int wm_idle(void *p) { return 0; }
void wm_setapptitle(const char *n) {}
void wm_setwindowtitle(const char *n) {}
void wm_allowbackgroundidle(int onf) {}
void wm_allowtaskswitching(int onf) {}
int startwin_open(void) { return 0; }
int startwin_close(void) { return 0; }
int startwin_puts(const char *s) { return 0; }
int startwin_settitle(const char *s) { return 0; }
int startwin_idle(void *p) { return 0; }
int startwin_run(struct startwin_settings *s) { return STARTWIN_RUN; }

/* ---- the screen ----------------------------------------------------------- */

void getvalidmodes(void) {
  static int done;
  if (done)
    return;
  done = 1;
  validmodecnt = 0;
  addvalidmode(640, 448, 32, 1, 0, 60, 0);
  addvalidmode(640, 512, 32, 1, 0, 50, 0);
  addvalidmode(1920, 1080, 32, 1, 0, 60, 0);
  sortvalidmodes();
}
const char *getdisplayname(int display) { return "PS2"; }

/* beam racing is the default, not as in Quake: its limit of a frame a field is this
   port's anyway, and it holds the rate where two buffers do not (README-PS2.md) */
int ps2_vid_hd, ps2_vid_beam = 1, ps2_vid_rgb24 = 1;
int ps2_tex_quality = MXGL_TEX_AUTO, ps2_tex_limits[3] = {120, 80, 30};
static int field_rate = 60, video_pending;

void ps2_video_change(void) { video_pending = 1; }
int ps2_video_flags(void) { return mxgl_mode_flags(); }
const char *ps2_video_name(void) {
  return mxgl_mode() == MXGL_1080I ? "1920 X 1080" : mxgl_mode() == MXGL_PAL ? "PAL 640 X 512" : "NTSC 640 X 448";
}
void ps2_tex_apply(void) {
  mxgl_texture_quality(ps2_tex_quality);
  mxgl_texture_limits(ps2_tex_limits[0], ps2_tex_limits[1], ps2_tex_limits[2]);
}

/* the textures stay until something frees them, and nothing else does between
   levels: level after level they would fill the memory. libgl frees what the
   frames in flight may read after those frames, so a few go by. */
void ps2_oom(unsigned bytes) {
  static int n;
  if (n++ < 5) {
    u8 *lo = (u8 *)mx_heap_start();
    mx_printf("ps2: out of memory: %u bytes, heap %u of %u KB\n", bytes,
              (u32)((u8 *)mx_sbrk(0) - lo) >> 10, (u32)((u8 *)mx_heap_end() - lo) >> 10);
  }
}
void PTClear(void);
void ps2_level_loading(void) {
  polymost_glreset();
  PTClear();
  for (int i = 0; i < 3; i++) {
    mxgl_end_frame();
    mxgl_frame_done();
  }
}
void ps2_level_loaded(void) {
  extern int ps2_tex_failed;
  u8 *lo = (u8 *)mx_heap_start();
  mx_printf("ps2: level loaded: heap %u of %u KB, %d textures left out\n",
            (u32)((u8 *)mx_sbrk(0) - lo) >> 10, (u32)((u8 *)mx_heap_end() - lo) >> 10, ps2_tex_failed);
}

/* the screen: the mode of the settings (MODE: a test's, whatever they say) */
int setvideomode(int x, int y, int c, int fs) {
  static int first = 1;
  if (baselayer_videomodewillchange)
    baselayer_videomodewillchange();
  int mode = (ps2_vid_hd ? MXGL_1080I : MXGL_SD) | (ps2_vid_beam ? MXGL_BEAM : 0) |
             (ps2_vid_rgb24 ? MXGL_RGB24 : 0);
#if PS2_MODE >= 0
  mode = PS2_MODE;
#endif
  if (mxgl_set_mode(mode) < 0) {
    mx_printf("ps2: beam racing: not enough memory\n");
    ps2_vid_beam = 0;
  }
  /* a frame a field: the field rate, no faster (the mode set it off) */
  mx_frame_vsync(1);
  field_rate = mxgl_mode() == MXGL_PAL ? 50 : 60;
  mxgl_screen(&xres, &yres);
  bpp = 32, fullscreen = 1;
  frameplace = 0, bytesperline = 0, imageSize = 0, numpages = 127;
  glbuild_init();
  if (first) {
    first = 0;
    ps2_tex_apply();
    polymost_glreset();
    polymost_glinit();
  }
  if (baselayer_videomodedidchange)
    baselayer_videomodedidchange();
  OSD_ResizeDisplay(xres, yres);
  setpalettefade(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);
  return 0;
}

#if !PS2_PLAY
static int frames;
#endif
static u64 sec_at;
static int sec_frames;
int ps2_fps_min = 1000, ps2_dropped;
#if PS2_PLAY
/* the test script's watchdog: where the program is when frames stop */
#include <mx/timer.h>
static int frames;
static void watch(void *arg) {
  extern volatile u32 mx__irq_epc;
  static int last, idle;
  if (frames != last || frames < 100) {
    last = frames, idle = 0;
    return;
  }
  static u32 at[10];
  if (++idle >= 20 && idle < 30)
    at[idle - 20] = mx__irq_epc;
  if (idle == 30) {
    for (int i = 0; i < 10; i++)
      mx_printf("stuck at %08x\n", at[i]);
    mx_printf("@@PS2TEST_END@@ status=4\n");
  }
}
#endif
int ps2_god = PS2_PLAY == 1;

unsigned ps2_count(void) { return mx_count(); }
/* 1 when a frame is too far into its field for a new texture (polymosttex.c),
   but not for more than a few frames in a row: a tile always gets its turn */
int ps2_tex_late(void) {
  static int waited;
  if (mx_count() - mx_gs.vsync_at < MX_EE_HZ / 125 || ++waited > 8) {
    waited = 0;
    return 0;
  }
  return 1;
}
void showframe(void) {
  extern unsigned ps2_tex_time;
  ps2_tex_time = 0;
  mxgl_end_frame();
  mxgl_frame_done();
  frames++;
#if PS2_PLAY
  {
    static u32 last;
    if (frames > 240 && mx_gs.vsync - last > 1)
      mx_printf("ps2: frame %d took %u fields\n", frames, mx_gs.vsync - last);
    last = mx_gs.vsync;
  }
#endif
  /* a change from the menu, between frames */
  if (video_pending) {
    video_pending = 0;
    ps2_video_changed();
    sec_at = 0;
    sec_frames = 0;
  }
  /* frames in each second's fields: the field rate when none was dropped (a
     whole second by the GS's field counter, not the clock, so no rounding) */
  sec_frames++;
  u32 v = mx_gs.vsync;
  if (!sec_at)
    sec_at = v;
  else if (v - (u32)sec_at >= (u32)field_rate) {
    mx_printf("ps2: frame %d: %d fps\n", frames, sec_frames * field_rate / (int)(v - (u32)sec_at));
    if (frames > 240) {
      int f = sec_frames * field_rate / (int)(v - (u32)sec_at);
      if (f < ps2_fps_min)
        ps2_fps_min = f;
      ps2_dropped += (int)(v - (u32)sec_at) - sec_frames;
    }
    sec_at = v;
    sec_frames = 0;
  }
#if PS2_PROF
  /* PROF=1: the second half of the run; PROF=n: from frame n */
  if (PS2_FRAMES && frames == (PS2_PROF > 1 ? PS2_PROF : PS2_FRAMES / 2))
    mx_prof_start(2000);
#endif
  if (PS2_FRAMES && frames >= PS2_FRAMES) {
#if PS2_PROF
    mx_prof_stop();
    mx_prof_report(4000);
#endif
    int r = mxgl_screenshot("host:duke3d.tga");
    mx_printf("ps2: %d frames, slowest second %d fps, %d fields dropped, picture %d\n", frames, ps2_fps_min, ps2_dropped, r);
    mx_printf("ps2: GS late %u, EE late %u fields\n", mx_gs.missed, mx_gs.starved);
    if (mxgl_mode_flags() & MXGL_BEAM)
      mx_printf("ps2: beam: %u fields, %u passes, %u late, %u held, %u shown, %u too large, longest %u %u %u %u us\n",
                mx_hires_stats.fields, mx_hires_stats.passes, mx_hires_stats.late, mx_hires_stats.held,
                mx_hires_stats.shown, mx_hires_stats.rec_full, mx_hires_stats.pass_us[0],
                mx_hires_stats.pass_us[1], mx_hires_stats.pass_us[2], mx_hires_stats.pass_us[3]);
    ps2_end(0);
  }
}

int setpalette(int start, int num, unsigned char *dapal) { return 0; }
int setsysgamma(float shadergamma, float sysgamma) { return 0; }
int loadgldriver(const char *soname) { return 0; }
int unloadgldriver(void) { return 0; }
void *getglprocaddress(const char *name, int ext) { return (void *)mxglGetProcAddress(name); }

/* ---- input: pad 0 --------------------------------------------------------- */

static const struct { u32 bit; unsigned char sc; } pad_keys[] = {
  {MX_PAD_UP, 0xc8},       /* up */
  {MX_PAD_DOWN, 0xd0},     /* down */
  {MX_PAD_LEFT, 0xcb},     /* left */
  {MX_PAD_RIGHT, 0xcd},    /* right */
  {MX_PAD_R1, 0x1d},       /* fire: left ctrl */
  {MX_PAD_R2, 0x1a},       /* inventory left: [ */
  {MX_PAD_L1, 0x39},       /* open: space */
  {MX_PAD_L2, 0x1b},       /* inventory right: ] */
  {MX_PAD_CROSS, 0x1e},    /* jump: A; in menus Enter, below */
  {MX_PAD_CIRCLE, 0x2c},   /* crouch: Z */
  {MX_PAD_SQUARE, 0x1c},   /* use the item: Enter */
  {MX_PAD_TRIANGLE, 0x28}, /* next weapon: bound to ' */
  {MX_PAD_START, 0x01},    /* menu: Escape */
  {MX_PAD_SELECT, 0x0f},   /* map: Tab */
};
static int pad_ok;

static void ps2_key(int sc, int down) {
  keystatus[sc] = down;
  keyfifo[keyfifoend] = sc;
  keyfifo[(keyfifoend + 1) & (KEYFIFOSIZ - 1)] = down;
  keyfifoend = (keyfifoend + 2) & (KEYFIFOSIZ - 1);
}

int initinput(void) {
  pad_ok = mx_pad_init() >= 0;
  inputdevices = 1 | 2;
  return 0;
}
void uninitinput(void) {}
const char *getkeyname(int num) { return ""; }
const char *getjoyname(int what, int num) { return ""; }
int initmouse(void) { return 0; }
void uninitmouse(void) {}
void grabmouse(int a) {}
void readmousexy(int *x, int *y) { *x = mousex, *y = mousey, mousex = mousey = 0; }
void readmousebstatus(int *b) { *b = mouseb; }
void releaseallbuttons(void) {}

/* KEYS: from frame 300 a key each 30 frames, for 3: s(tart), x, o, u, d, l, r
   (the d-pad) or . for none */
static u32 script_keys(void) {
  static const char keys[] = PS2_KEYS;
  int i = (frames - 300) / 30;
  if (frames < 300 || i >= (int)sizeof keys - 1 || (frames - 300) % 30 >= 3)
    return 0;
  switch (keys[i]) {
  case 's': return MX_PAD_START;
  case 'x': return MX_PAD_CROSS;
  case 'o': return MX_PAD_CIRCLE;
  case 'u': return MX_PAD_UP;
  case 'd': return MX_PAD_DOWN;
  case 'l': return MX_PAD_LEFT;
  case 'r': return MX_PAD_RIGHT;
  }
  return 0;
}

/* the keys held by the test script: a walk through the level */
static u32 script_buttons(int *turn) {
#if PS2_PLAY == 2
  /* through the menus: X now and then */
  *turn = 0;
  return frames > 300 && frames % 60 < 8 ? MX_PAD_CROSS : 0;
#elif PS2_PLAY
  int s = frames / 60;
  *turn = (s % 3 == 2) ? ((s / 3) & 1 ? 30 : -30) : 0;
  return MX_PAD_UP | (s % 4 == 3 ? MX_PAD_R1 : 0) | (s % 5 == 4 ? MX_PAD_L1 : 0) |
         (s % 7 == 6 ? MX_PAD_CROSS : 0);
#else
  *turn = 0;
  return 0;
#endif
}

static int script_look(void) {
#if PS2_PLAY
  int s = frames / 60;
  return s % 9 == 3 ? -1 : s % 9 == 7 ? 1 : 0;
#else
  return 0;
#endif
}

int handleevents(void) {
  static u32 last;
  static int left_y, left_x, right_y;
  int turn = 0;
  u32 held = pad_ok ? mx_pad_buttons(0) : 0;
  held |= script_buttons(&turn) | script_keys();
  for (unsigned i = 0; i < sizeof pad_keys / sizeof pad_keys[0]; i++)
    if ((held ^ last) & pad_keys[i].bit)
      ps2_key(pad_keys[i].sc, (held & pad_keys[i].bit) != 0);
  /* X is Enter too: the menus' */
  if ((held ^ last) & MX_PAD_CROSS)
    ps2_key(0x1c, (held & MX_PAD_CROSS) != 0);
  if (held & ~last & MX_PAD_R3)
    mxgl_show_stats = !mxgl_show_stats;
  last = held;
  u8 st[4];
  int look = script_look();
  mousex += turn;
  if (pad_ok && mx_pad_sticks(0, st)) {
    int rx = st[0] - 128, ry = st[1] - 128, lx = st[2] - 128, ly = st[3] - 128;
    if (rx > 24 || rx < -24)
      mousex += rx / 6;
    if (ry < -48 || ry > 48)
      look = ry < 0 ? -1 : 1;
    int y = ly < -48 ? -1 : ly > 48 ? 1 : 0, x = lx < -48 ? -1 : lx > 48 ? 1 : 0;
    if (y != left_y) {
      if (left_y)
        ps2_key(left_y < 0 ? 0xc8 : 0xd0, 0);
      if (y)
        ps2_key(y < 0 ? 0xc8 : 0xd0, 1);
      left_y = y;
    }
    if (x != left_x) {
      if (left_x)
        ps2_key(left_x < 0 ? 0x33 : 0x34, 0);
      if (x)
        ps2_key(x < 0 ? 0x33 : 0x34, 1);
      left_x = x;
    }
  }
  /* the right stick up and down: Look_Up, Look_Down (PgUp, PgDn) */
  if (look != right_y) {
    if (right_y)
      ps2_key(right_y < 0 ? 0xc9 : 0xd1, 0);
    if (look)
      ps2_key(look < 0 ? 0xc9 : 0xd1, 1);
    right_y = look;
  }
  sampletimer();
  ps2_audio_pump();
  return 0;
}

/* ---- main ----------------------------------------------------------------- */

int main(int argc, char **argv) {
  static char line[256] = "duke3d " PS2_ARGS;
  static const char *args[32];
  int n = 0;
  for (char *p = strtok(line, " "); p && n < 31; p = strtok(NULL, " "))
    args[n++] = p;
  chdir("/duke3d");
  _buildargc = n;
  _buildargv = args;
  baselayer_init();
  int r = app_main(n, args);
  ps2_end(r);
  return r;
}

/* ---- libc odds ------------------------------------------------------------ */

#include <dirent.h>
#include <sys/stat.h>

int displaycnt = 1;
const char *build_version = "ps2";
const char *build_date = __DATE__;
const char *build_time = __TIME__;

/* Bgetsysmemsize: the PS2's 32 MiB */
long sysconf(int name) {
  return name == _SC_PAGE_SIZE ? 4096 : name == _SC_PHYS_PAGES ? (32 << 20) / 4096 : -1;
}
/* compat.c's directory listing: relative to the current directory */
int dirfd(DIR *d) { return 0; }
int fstatat(int fd, const char *p, struct stat *st, int flags) { return stat(p, st); }
