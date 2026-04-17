#include "platform_harmony.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "header/memory.h"

static void (*g_onDraw)(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen) = NULL;
static void (*g_onTimerStart)(uint16_t t) = NULL;
static void (*g_onTimerStop)() = NULL;
static void (*g_onEditRequest)(const char *title, const char *text, int32_t type, int32_t max_size) = NULL;

void vmrp_setCallbacks(
    void (*onDraw)(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
        int32_t srcFromFullScreen),
    void (*onTimerStart)(uint16_t t),
    void (*onTimerStop)()
) {
    g_onDraw = onDraw;
    g_onTimerStart = onTimerStart;
    g_onTimerStop = onTimerStop;
}

void vmrp_setEditRequestCallback(void (*cb)(const char *title, const char *text, int32_t type, int32_t max_size)) {
    g_onEditRequest = cb;
}

static char *holdEditText = NULL;
static int32_t editMaxSize = 0;

void guiDrawBitmap(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen) {
    if (g_onDraw) {
        g_onDraw(bmp, x, y, w, h, srcPitch, srcFromFullScreen);
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
    if (holdEditText != NULL) {
        my_freeExt(holdEditText);
        holdEditText = NULL;
    }
    if (g_onEditRequest) {
        g_onEditRequest(title ? title : "", text ? text : "", type, max_size);
    } else {
        printf("editCreate: no JS onEditRequest, title='%s', type=%d, max_size=%d\n", title ? title : "", type,
            max_size);
    }
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

void harmony_edit_set_result_utf8(const char *utf8) {
    if (holdEditText != NULL) {
        my_freeExt(holdEditText);
        holdEditText = NULL;
    }
    if (utf8 == NULL || utf8[0] == '\0') {
        holdEditText = (char *)my_mallocExt(1);
        if (holdEditText) {
            holdEditText[0] = '\0';
        }
        return;
    }
    size_t n = strlen(utf8) + 1;
    holdEditText = (char *)my_mallocExt(n);
    if (holdEditText) {
        memcpy(holdEditText, utf8, n);
    }
}
