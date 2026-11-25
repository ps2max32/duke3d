/* ps2_audio.c: JFAudioLib's PCM driver on maxsdk's stream (mx/snd.h, mxsnd),
   and its music on maxsdk's MIDI player (mx/midi.h: on the IOP, not
   JFAudioLib's sequencer; the bank gm.bnk in the game's directory, made by
   maxsdk's tools/sf2bank).
   The mixer fills one division of its buffer per call of its callback; there
   are no threads, so ps2_audio_pump (from handleevents) calls it as long as
   the stream has room for a division. */

#include <mx/debug.h>
#include <mx/midi.h>
#include <mx/snd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps2_port.h"
#include "driver_ps2.h"

static char *buf;
static int bufsize, ndiv, div_at, playing;
static void (*callback)(void);

int PS2Drv_GetError(void) { return 0; }
const char *PS2Drv_ErrorString(int e) { return "mxsnd"; }

int PS2Drv_PCM_Init(int *rate, int *channels, int *bits, void *initdata) {
  if (mx_snd_init() < 0)
    return -1;
  *rate = 22050, *channels = 2, *bits = 16;
  return mx_snd_stream_format(22050, 16, 2) < 0 ? -1 : 0;
}
void PS2Drv_PCM_Shutdown(void) { mx_snd_stream_stop(); }
int PS2Drv_PCM_BeginPlayback(char *start, int size, int n, void (*cb)(void)) {
  buf = start, bufsize = size, ndiv = n, callback = cb, div_at = 0, playing = 1;
  return 0;
}
void PS2Drv_PCM_StopPlayback(void) { playing = 0; }
void PS2Drv_PCM_Lock(void) {}
void PS2Drv_PCM_Unlock(void) {}

/* ---- music ------------------------------------------------------------------ */

static void *bank;

int PS2Drv_Music_Init(void) {
  PS2Drv_Music_Shutdown();
  FILE *fp = fopen("gm.bnk", "rb");
  if (!fp || mx_snd_init() < 0) {
    if (fp)
      fclose(fp);
    mx_printf("ps2: no gm.bnk, no music\n");
    return -1;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  bank = size > 0 ? malloc((size_t)size) : 0; /* in use while loaded (VM: paged out) */
  int ok = bank && fread(bank, 1, (size_t)size, fp) == (size_t)size && mx_midi_load(bank, (u32)size) == 0;
  fclose(fp);
  if (!ok) {
    free(bank);
    bank = 0;
    mx_printf("ps2: gm.bnk is not a bank or does not fit, no music\n");
    return -1;
  }
  return 0;
}
void PS2Drv_Music_Shutdown(void) {
  if (bank) {
    mx_midi_free();
    free(bank);
    bank = 0;
  }
}
int PS2Drv_Music_Play(const void *song, unsigned int length, int loop) {
  if (!bank)
    return 0; /* no music is not an error */
  if (mx_midi_song(song, length) < 0 || mx_midi_play(loop) < 0) {
    mx_printf("ps2: a song of %u bytes is not played\n", length);
    return -1;
  }
  return 0;
}
void PS2Drv_Music_Stop(void) {
  if (bank)
    mx_midi_stop();
}
void PS2Drv_Music_Pause(int on) {
  if (bank)
    mx_midi_pause(on);
}
void PS2Drv_Music_Volume(int volume) { /* 0 .. 255 */
  if (bank)
    mx_midi_volume(volume + (volume >> 7));
}
int PS2Drv_Music_Playing(void) { return bank && mx_midi_playing(); }

void ps2_audio_pump(void) {
  mx_snd_commit(); /* also takes the sound back after Home, the music with it */
  if (!playing)
    return;
  while (mx_snd_stream_room() >= (unsigned)bufsize) {
    callback();
    mx_snd_stream_queue(buf + div_at * bufsize, (u32)bufsize);
    div_at = (div_at + 1) % ndiv;
  }
}
