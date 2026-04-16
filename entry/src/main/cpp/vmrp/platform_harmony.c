#include "platform_harmony.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "header/memory.h"

static void (*g_onDraw)(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) = NULL;
static void (*g_onTimerStart)(uint16_t t) = NULL;
static void (*g_onTimerStop)() = NULL;

void vmrp_setCallbacks(
    void (*onDraw)(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h),
    void (*onTimerStart)(uint16_t t),
    void (*onTimerStop)()
) {
    g_onDraw = onDraw;
    g_onTimerStart = onTimerStart;
    g_onTimerStop = onTimerStop;
}

static char *holdEditText = NULL;
static int32_t editMaxSize = 0;

void guiDrawBitmap(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (g_onDraw) {
        g_onDraw(bmp, x, y, w, h);
    }
}

int32_t timerStart(uint16_t t) {
    if (g_onTimerStart) {
        g_onTimerStart(t);
    }
    return MR_SUCCESS;
}

int32_t timerStop() {
    if (g_onTimerStop) {
        g_onTimerStop();
    }
    return MR_SUCCESS;
}

int32_t editCreate(const char *title, const char *text, int32_t type, int32_t max_size) {
    editMaxSize = max_size;
    printf("editCreate: title='%s', type=%d, max_size=%d\n", title, type, max_size);
    return 1234;
}

int32 editRelease(int32 edit) {
    if (holdEditText != NULL) {
        my_freeExt(holdEditText);
        holdEditText = NULL;
    }
    return MR_SUCCESS;
}

char *editGetText(int32 edit) {
    return holdEditText;
}
