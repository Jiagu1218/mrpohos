#include "harmony_vibrator.h"
#include <string.h>

#ifdef __HARMONYOS__
#include <sensors/vibrator.h>
#endif

int32 harmony_vibrator_play_ms(int32_t duration_ms) {
#ifdef __HARMONYOS__
    Vibrator_Attribute attr;
    memset(&attr, 0, sizeof(attr));
    attr.vibratorId = 0;
    attr.usage = VIBRATOR_USAGE_PHYSICAL_FEEDBACK;

    int32_t ms = duration_ms;
    if (ms < 1) {
        ms = 80;
    }
    if (ms > 60000) {
        ms = 60000;
    }

    if (OH_Vibrator_PlayVibration(ms, attr) == 0) {
        return MR_SUCCESS;
    }
    return MR_FAILED;
#else
    (void)duration_ms;
    return MR_SUCCESS;
#endif
}

int32 harmony_vibrator_cancel(void) {
#ifdef __HARMONYOS__
    (void)OH_Vibrator_Cancel();
#endif
    return MR_SUCCESS;
}
