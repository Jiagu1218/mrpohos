#ifndef VMRP_HARMONY_VIBRATOR_H
#define VMRP_HARMONY_VIBRATOR_H

#include "header/types.h"

/**
 * 按毫秒触发一次固定时长振动（对应 MRP mr_startShake(ms)）。
 * 需 ohos.permission.VIBRATE；失败时返回 MR_FAILED。
 */
int32 harmony_vibrator_play_ms(int32_t duration_ms);

/** 停止当前振动（对应 mr_stopShake）；内部调用 OH_Vibrator_Cancel，恒返回 MR_SUCCESS。 */
int32 harmony_vibrator_cancel(void);

#endif
