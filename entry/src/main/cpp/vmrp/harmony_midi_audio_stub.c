/* 默认构建：无 FluidSynth/OHAudio 链接；bridge 仍可按规格对非 MIDI 返回 MR_FAILED，MIDI 返回 MR_FAILED 直至接入预编译库。 */

#include "harmony_midi_audio.h"

int32 harmony_midi_init(const char *sf2_path) {
    (void)sf2_path;
    return MR_SUCCESS;
}

void harmony_midi_shutdown(void) {}

int32 harmony_midi_play(const uint8_t *data, uint32 len, int32 loop) {
    (void)data;
    (void)len;
    (void)loop;
    return MR_FAILED;
}

int32 harmony_midi_stop(void) {
    return MR_SUCCESS;
}
