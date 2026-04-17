#ifndef __PLATFORM_HARMONY_H__
#define __PLATFORM_HARMONY_H__

#include "types.h"

void guiDrawBitmap(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen);
int32_t timerStart(uint16_t t);
int32_t timerStop();
int32_t editCreate(const char *title, const char *text, int32_t type, int32_t max_size);
int32 editRelease(int32 edit);
char *editGetText(int32 edit);

void vmrp_setCallbacks(
    void (*onDraw)(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
        int32_t srcFromFullScreen),
    void (*onTimerStart)(uint16_t t),
    void (*onTimerStop)()
);

#endif
