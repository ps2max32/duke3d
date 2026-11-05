/* ps2_audio.c: JFAudioLib's PCM driver on maxsdk's stream (mx/snd.h, audsrv).
   The mixer fills one division of its buffer per call of its callback; there
   are no threads, so ps2_audio_pump (from handleevents) calls it as long as
   the stream has room for a division. */

#include <mx/snd.h>
#include "ps2_port.h"
#include "driver_ps2.h"

static char *buf;
static int bufsize, ndiv, div_at, playing;
static void (*callback)(void);

int PS2Drv_GetError(void) { return 0; }
const char *PS2Drv_ErrorString(int e) { return "audsrv"; }

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

void ps2_audio_pump(void) {
  if (!playing)
    return;
  while (mx_snd_stream_room() >= (unsigned)bufsize) {
    callback();
    mx_snd_stream_queue(buf + div_at * bufsize, (u32)bufsize);
    div_at = (div_at + 1) % ndiv;
  }
}
