#include "platform_harmony.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbk_conv.h"
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
/** mr_editGetText 需返回客体内存可读的 UCS-2（与 mythroad 一致），由 UTF-8 经 GB 表转写。 */
static char *s_editResultUcs = NULL;
static int32_t editMaxSize = 0;

static void harmony_free_edit_result_ucs(void) {
    if (s_editResultUcs != NULL) {
        my_freeExt(s_editResultUcs);
        s_editResultUcs = NULL;
    }
}

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
    char *t8 = NULL;
    char *x8 = NULL;

    editMaxSize = max_size;
    harmony_free_edit_result_ucs();
    if (holdEditText != NULL) {
        my_freeExt(holdEditText);
        holdEditText = NULL;
    }
    if (g_onEditRequest) {
        /* bridge 直接把客体内 UCS-2 指针交给平台；Mythroad 为「unicode 网络字节序」= 每字符高字节在前，用 ucs2be_str_to_utf8。 */
        t8 = title ? ucs2be_str_to_utf8((const unsigned char *)title, NULL) : NULL;
        x8 = text ? ucs2be_str_to_utf8((const unsigned char *)text, NULL) : NULL;
        g_onEditRequest(t8 ? t8 : "", x8 ? x8 : "", type, max_size);
    } else {
        printf("editCreate: no JS onEditRequest, title ptr=%p, type=%d, max_size=%d\n", (void *)title, type,
            max_size);
    }
    free(t8);
    free(x8);
    return 1234;
}

int32 editRelease(int32 edit) {
    harmony_free_edit_result_ucs();
    if (holdEditText != NULL) {
        my_freeExt(holdEditText);
        holdEditText = NULL;
    }
    return MR_SUCCESS;
}

char *editGetText(int32 edit) {
    char *gb = NULL;
    unsigned short *ucs = NULL;
    size_t n = 0;
    uint32 bytes;

    harmony_free_edit_result_ucs();
    if (holdEditText == NULL) {
        return NULL;
    }
    gb = utf8_str_to_gb((const unsigned char *)holdEditText, NULL);
    if (gb == NULL) {
        return NULL;
    }
    ucs = gb_str_to_ucs2be((const unsigned char *)gb, NULL);
    free(gb);
    if (ucs == NULL) {
        return NULL;
    }
    while (ucs[n] != 0) {
        n++;
    }
    bytes = (uint32)((n + 1u) * sizeof(unsigned short));
    s_editResultUcs = (char *)my_mallocExt(bytes);
    if (s_editResultUcs == NULL) {
        free(ucs);
        return NULL;
    }
    memcpy(s_editResultUcs, ucs, bytes);
    free(ucs);
    return s_editResultUcs;
}

void harmony_edit_set_result_utf8(const char *utf8) {
    harmony_free_edit_result_ucs();
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
