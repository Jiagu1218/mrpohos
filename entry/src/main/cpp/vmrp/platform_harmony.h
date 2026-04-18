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

/** title/text 为 UTF-8（与 DSM 的 FLAG_USE_UTF8_EDIT 一致），在 UI 线程弹出编辑框后由 JS 调用 setEditResult 写入。 */
void vmrp_setEditRequestCallback(void (*cb)(const char *title, const char *text, int32_t type, int32_t max_size));

/** 在 MR_DIALOG_KEY_OK 之前调用，将 UTF-8 内容交给 mr_editGetText；传 NULL 或空串表示无内容。 */
void harmony_edit_set_result_utf8(const char *utf8);

int32 harmony_dialog_create(const char *title_ucs2be, const char *text_ucs2be, int32 type);
int32 harmony_dialog_refresh(int32 dialog, const char *title_ucs2be, const char *text_ucs2be, int32 type);
int32 harmony_dialog_release(int32 dialog);

int32 harmony_text_create(const char *title_ucs2be, const char *text_ucs2be, int32 type);
int32 harmony_text_refresh(int32 handle, const char *title_ucs2be, const char *text_ucs2be);
int32 harmony_text_release(int32 handle);

#endif
