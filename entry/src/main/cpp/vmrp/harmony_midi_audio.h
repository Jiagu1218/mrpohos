#ifndef VMRP_HARMONY_MIDI_AUDIO_H
#define VMRP_HARMONY_MIDI_AUDIO_H

#include "header/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 加载 SoundFont 并初始化音频输出（stub 下忽略路径且恒成功）。Native 下失败返回 MR_FAILED。 */
int32 harmony_midi_init(const char *sf2_path);
void harmony_midi_shutdown(void);

/**
 * 播放一帧 MIDI 字节（SMF）。在返回前会拷贝 data，可安全传入 getMrpMemPtr 结果。
 * loop：0 单次；非 0 时映射为 FluidSynth 无限循环（fluid_player_set_loop(..., -1)）。
 * 新 play 会打断当前 MIDI（与 design 一致）。
 * stub 下恒返回 MR_FAILED。
 */
int32 harmony_midi_play(const uint8_t *data, uint32 len, int32 loop);

/** 停止当前 MIDI；stub 下为 no-op 成功。 */
int32 harmony_midi_stop(void);

#ifdef __cplusplus
}
#endif

#endif
