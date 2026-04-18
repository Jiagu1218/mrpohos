#include "platform_harmony.h"
#include <stdint.h>
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

extern void vmrp_harmony_post_message_ui(int32_t op, int32_t view_kind, int32_t dialog_type, const char *title_utf8,
    const char *text_utf8);

enum {
    HARMONY_MSGUI_OP_CLOSE = 0,
    HARMONY_MSGUI_OP_OPEN = 1,
    HARMONY_MSGUI_OP_REFRESH = 2,
};

enum {
    HARMONY_VIEW_DIALOG = 1,
    HARMONY_VIEW_TEXT = 2,
};

#define HARMONY_HANDLE_DIALOG 0xD101
#define HARMONY_HANDLE_TEXT 0xD102

static void harmony_emit_message_ui(int32 op, int32 view_kind, int32 dialog_type, const char *t8, const char *x8) {
    vmrp_harmony_post_message_ui(op, view_kind, dialog_type, t8 ? t8 : "", x8 ? x8 : "");
}

static int32 harmony_message_emit_ucs2(int32 op, int32 view_kind, int32 dialog_type, const char *title_ucs2be,
    const char *text_ucs2be) {
    char *t8 = NULL;
    char *x8 = NULL;
    if (title_ucs2be) {
        t8 = ucs2be_str_to_utf8((const unsigned char *)title_ucs2be, NULL);
    }
    if (text_ucs2be) {
        x8 = ucs2be_str_to_utf8((const unsigned char *)text_ucs2be, NULL);
    }
    harmony_emit_message_ui(op, view_kind, dialog_type, t8 ? t8 : "", x8 ? x8 : "");
    free(t8);
    free(x8);
    return MR_SUCCESS;
}

int32 harmony_dialog_create(const char *title_ucs2be, const char *text_ucs2be, int32 type) {
    harmony_message_emit_ucs2(HARMONY_MSGUI_OP_OPEN, HARMONY_VIEW_DIALOG, type, title_ucs2be, text_ucs2be);
    return HARMONY_HANDLE_DIALOG;
}

int32 harmony_dialog_refresh(int32 dialog, const char *title_ucs2be, const char *text_ucs2be, int32 type) {
    (void)dialog;
    harmony_message_emit_ucs2(HARMONY_MSGUI_OP_REFRESH, HARMONY_VIEW_DIALOG, type, title_ucs2be, text_ucs2be);
    return MR_SUCCESS;
}

int32 harmony_dialog_release(int32 dialog) {
    (void)dialog;
    harmony_emit_message_ui(HARMONY_MSGUI_OP_CLOSE, HARMONY_VIEW_DIALOG, 0, "", "");
    return MR_SUCCESS;
}

int32 harmony_text_create(const char *title_ucs2be, const char *text_ucs2be, int32 type) {
    harmony_message_emit_ucs2(HARMONY_MSGUI_OP_OPEN, HARMONY_VIEW_TEXT, type, title_ucs2be, text_ucs2be);
    return HARMONY_HANDLE_TEXT;
}

int32 harmony_text_refresh(int32 handle, const char *title_ucs2be, const char *text_ucs2be) {
    (void)handle;
    /* mr_textRefresh 无 type 参数，刷新时 dialog_type 传 -1 表示保持当前按键布局 */
    harmony_message_emit_ucs2(HARMONY_MSGUI_OP_REFRESH, HARMONY_VIEW_TEXT, -1, title_ucs2be, text_ucs2be);
    return MR_SUCCESS;
}

int32 harmony_text_release(int32 handle) {
    (void)handle;
    harmony_emit_message_ui(HARMONY_MSGUI_OP_CLOSE, HARMONY_VIEW_TEXT, 0, "", "");
    return MR_SUCCESS;
}

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
