int PS2Drv_GetError(void);
const char *PS2Drv_ErrorString(int e);
int PS2Drv_PCM_Init(int *rate, int *channels, int *bits, void *initdata);
void PS2Drv_PCM_Shutdown(void);
int PS2Drv_PCM_BeginPlayback(char *start, int size, int n, void (*cb)(void));
void PS2Drv_PCM_StopPlayback(void);
void PS2Drv_PCM_Lock(void);
void PS2Drv_PCM_Unlock(void);
