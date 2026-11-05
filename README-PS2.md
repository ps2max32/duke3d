# Duke Nukem 3D on maxsdk (PS2)

JFDuke3D (GPL), with JFBuild, JFMACT and JFAudioLib, built for the PS2 with `libs/maxsdk`
(clang, R5900) by `Makefile`; the upstream one is `Makefile.jf`. The port is in `ps2/`; the
changes to the game are under `__PS2__`, those to the submodules are in `ps2/patches/`
(`ps2/patches.sh`).

```
make submodules                      the submodules, with the patches applied (once, first)
make [FRAMES=n] [ARGS="-v1 -l1 -s2"] $(OUT)/duke3d.elf (build/apps/duke3d)
make run                             copies DUKE3D.GRP (DUKEDATA) to build/pcsx2-test/duke3d
                                     and runs it in PCSX2
```

The picture is the TV's mode, NTSC 640 x 448 or PAL 640 x 512 by the console's region, or
1080i; see "Video modes". It takes one frame a field: the program waits for the field
(`mx_frame_vsync(1)`), so 60 (PAL 50) is the most it shows and a frame over a field's time
costs a field.

`FRAMES=n` ends the program after n frames with a picture (`host:duke3d.tga`), the slowest
second and the fields dropped, then the test end marker. `ARGS` is added to the command line
(`-nosetup -v2 -l1 -s2`: a level straight away). Without it the game starts at its logos and
menu. Other variables: `PLAY=1` walks, turns, looks and fires by script, unhurt, and logs each
frame that took more than a field; `PLAY=2` presses X through the menus; `PLAY` also has a watchdog that ends the run with `status=4` and the addresses where it was when frames stop; `STATS=1` shows
libgl's counters (L3 + R3 too); `PROF=1` profiles the second half of a run, `PROF=n` from frame n
(`libs/maxsdk/tools/prof.py`); `MODE=n` sets libgl's mode whatever the settings say (1 NTSC, 2
PAL, 3 1080i, +256 beam racing, +512 24-bit); `KEYS=sdddx` presses pad keys by script from frame
300, one each 30 frames (s, x, o, u, d, l, r, `.` none).

## Video modes

Options, Video Settings has what Quake's Video Options has, each line set at once by X or
left/right:

- Resolution: the TV's mode or 1080i. Not kept in the config: a TV without 1080i would show
  nothing at the next start. 1080i is 16:9, by the engine's widescreen path: the view is wider,
  the status bar and the menus keep their shape, the game's border tile fills beside the bar.
  Without beam racing 1080i is 640 x 540 stretched to the screen.
- Beam racing (`MXGL_BEAM`), on by default: one buffer of a field's lines, which leaves GS memory
  for twice the textures. Its limit of a frame a field is this port's anyway, and it holds the
  rate where two buffers do not (below); Quake leaves it off, as it can go faster.
- Colour: 24-bit by default (`MXGL_RGB24`): no dithering, a 24-bit Z. It needs beam racing and
  the TV's mode; else 16-bit. It costs no fields in the levels tried.
- Textures: auto, or that many top levels left out always (`mxgl_texture_quality`).
- Brightness, filtering: the game's.

The config (`duke3d.cfg`, written on leaving this page, as a console is switched off and not
quit) keeps `PS2BeamRacing`, `PS2Colour24`, `PS2TexQuality`, and the limits of auto:
`PS2TexVramHigh`, `PS2TexVramLow`, `PS2TexSwapIns` (120, 80, 30).

640 x 448 and 640 x 512 fill a 4:3 TV, their pixels are not square: `setgamemode` takes that
into account (Build would call the first widescreen, the second tall). A mode change keeps the
textures: libgl sends them again.

## Data

`DUKE3D.GRP`: 1.3D or Atomic, or the shareware one. JFDuke3D plays no demos of the original
game (`BYTEVERSION`), so there is no timedemo: `PLAY=1` is what the rates below come from.

## How

| Part | How |
|---|---|
| Renderer | Polymost (JFBuild's GL renderer) on `libs/libgl`: `polymost_drawpoly_glcall` and `polymost_drawaux_glcall` as fixed function GL in place of its GLSL. The fullbright texels, the shader's glow texture, are a second pass |
| Shading | Polymost's depth shading is `GL_EXP2` fog: libgl's, the GS's |
| Sky | Drawn at the far end of the depth range (`ps2_skydepth`): its plane is not behind a sprite after libgl's depth, and sprites before the sky would not show |
| Textures | ART tiles are 8-bit and stay so: bytes of the palette straight to libgl (`GL_COLOR_INDEX`, the shared palette with 255 transparent), T8 in RAM and GS memory; `ps2_load_art` in `polymosttex.c`. The levels are libgl's. No hightile, models or texture cache. A texture first used in play is made in a frame with time left (`PT_GetHead`), the level's own at its start, with their palettes, the sky's strips and each frame of an animation (`cacheit`) |
| Float | `double` is `float` in Polymost (`ps2/ps2_float.h`): the FPU has no doubles. The GL headers come before that define: their `GLclampd` must stay double. Walls are clipped to the view's sides as to the close plane, so their planes come from points on the screen, where float is enough; the sector scan is in 64-bit integers. `engine.c` keeps double: its tables are checked. `ps2/ps2_softfp.c` is what double costs elsewhere: compiler-rt's calls for the usual case, the same results |
| Hangs | A float too large for an `int` becomes `INT_MAX` on the R5900 (`INT_MIN` on x86): `drawpoly` checks the range of its texture repeats, whose loop would never pass it |
| Culling | Walls outside the view or behind what is drawn are left before their planes; the sky's strips are those in view; `domost` stops at the end of a wall's span, and joins only the strips it touched (the list is sorted by x) |
| Memory | All in physical memory (`MX_MEMORY=phys`), as PCSX2's recompiler has no virtual memory: the program 11.6 MB, the tile cache 6 MB (`MAXCACHE1DSIZE`, 16 in the original), libgl's textures about 6 to 9 MB in the levels tried. Beam racing takes 2 MiB more: the fullest level, E3L8, has about 0.7 MB left. Textures are freed at each level's start (`ps2_level_loading`), or they would add up level after level; a texture's header has no hightile path, a level has thousands. Out of memory a texture shows white; `ps2: out of memory` and `textures left out` say so |
| Files | POSIX on the IOP file server (`ps2/ps2_posix.c`, Half-Life's); `/` is `host:`, the game's directory `/duke3d` |
| Input | pad 0 as the keyboard: d-pad or left stick up/down move, left/right turn (d-pad) or strafe (left stick), right stick turns and looks, R1 fire, L1 open, X jump (Enter in the menus), O crouch, square use the item, triangle next weapon, R2/L2 inventory, START menu, SELECT map, R3 the counters |
| Sound | JFAudioLib's mixer at 22050 Hz 16-bit stereo, a `PS2Drv` PCM driver on maxsdk's stream (`mx/snd.h`, audsrv), filled from `handleevents` (no threads) |

Not done yet: music (MIDI: JFAudioLib has no synthesiser for the PS2, the driver is the null
one), multiplayer, the OSD console's keys, analog movement. The menus take 30 ms a frame on a
real PS2, EE time, so they run at about 29 frames a second over a level.

## Frame rate

PCSX2 does not model the GS's time and hides the R5900's address errors: rates, and whether the
game runs at all, are a real PS2's to say.

A PAL PS2, beam racing, 16-bit. `PLAY=1`, 1200 frames of each level, counted
by the GS's field counter from frame 240 on: the boot through the logos and menus and all 29
levels of the three episodes of 1.3D run through, no texture left out, no pass late or held,
the longest pass 2 ms. 20 levels hold 50 frames a second (49 at worst, no field dropped). The
others drop 3 to 49 fields of about 1000 in a few heavy views, the slowest second 41 to 47; the
worst are E2L10 at 36 and E3L9 at 25.

24-bit, the default, in the same walks: E1L1 1 field dropped, E2L7 50, E2L10 32, E3L8 none,
E3L9 26, the slowest second 29: as 16-bit or better.

Beam racing against two buffers, in the same walks: E1L1 in PAL 0 fields dropped against 44, E3L6
20 against 120. With two buffers the time goes on the EE's side (515 fields late against 1 for
the GS), though the game does the same work.

NTSC has a field of 16.7 ms and not 20: E1L1 drops 36 fields of about 1000 with beam racing, and
the levels have not been measured there, nor 1080i but for E1L1 (64). On PCSX2 all modes hold
the field rate.
