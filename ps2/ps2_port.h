/* ps2_port.h: what the port's own files share */
#ifndef PS2_PORT_H
#define PS2_PORT_H
void ps2_audio_pump(void);
void ps2_end(int status);

/* the video settings, those of Quake's menu (ps2_layer.c). The menu sets them,
   the config keeps all but the resolution, a mode change comes at the next
   frame: ps2_video_change asks for it, ps2_video_changed() in the game then
   calls setgamemode. */
extern int ps2_vid_hd;       /* 0: the TV's mode (NTSC or PAL), 1: 1080i */
extern int ps2_vid_beam;     /* beam racing (MXGL_BEAM) */
extern int ps2_vid_rgb24;    /* 24-bit colour (MXGL_RGB24): beam racing in the TV's mode */
extern int ps2_tex_quality;  /* the textures' top levels left out: -1 auto, 0, 1, 2 */
extern int ps2_tex_limits[3]; /* auto's limits: GS memory high, low (%), swap-ins a second */
void ps2_video_change(void);
void ps2_video_changed(void);
void ps2_tex_apply(void);
/* around a level's loading: the textures of the level before go first */
void ps2_level_loading(void);
void ps2_level_loaded(void);
int ps2_video_flags(void);   /* those in effect: MXGL_BEAM, MXGL_RGB24 */
const char *ps2_video_name(void);
#endif
