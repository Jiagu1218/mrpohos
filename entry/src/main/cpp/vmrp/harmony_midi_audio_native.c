/*
 * FluidSynth + OHAudio 实时 MIDI → PCM。
 * 仅当 CMake 定义 VMRP_HAVE_NATIVE_MIDI=1 且链接 libfluidsynth / libohaudio 时编译本文件。
 */

#include "harmony_midi_audio.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fluidsynth.h>
#include <hilog/log.h>
#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0001
#define LOG_TAG "harmony_midi"

#define VMRP_MIDI_SAMPLE_RATE 44100
#define VMRP_MIDI_STACK_FRAMES 2048

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static fluid_settings_t *g_settings;
static fluid_synth_t *g_synth;
static fluid_player_t *g_player;

static OH_AudioStreamBuilder *g_builder;
static OH_AudioRenderer *g_renderer;

static int g_inited;
static volatile int g_stop_player_req;
static uint8_t *g_midi_owned;

static void harmony_free_midi_owned(void) {
    if (g_midi_owned) {
        free(g_midi_owned);
        g_midi_owned = NULL;
    }
}

/**
 * 停止 fluid_player 后必须复位合成器：否则 fluid_synth_write_s16 仍会渲染已触发音符的
 * release/混响尾音，游戏内关 MIDI 后 OHAudio 会持续送出非零采样，表现为喇叭余音不断。
 */
static void harmony_synth_silence_locked(void) {
    if (g_synth) {
        fluid_synth_system_reset(g_synth);
    }
}

static void harmony_stop_player_locked(void) {
    if (g_player) {
        fluid_player_stop(g_player);
        delete_fluid_player(g_player);
        g_player = NULL;
    }
    harmony_free_midi_owned();
    harmony_synth_silence_locked();
}

static void harmony_drain_stop_request(void) {
    if (!g_stop_player_req) {
        return;
    }
    g_stop_player_req = 0;
    pthread_mutex_lock(&g_lock);
    harmony_stop_player_locked();
    pthread_mutex_unlock(&g_lock);
}

static void harmony_on_interrupt(OH_AudioRenderer *renderer, void *userData, OH_AudioInterrupt_ForceType type,
    OH_AudioInterrupt_Hint hint) {
    (void)renderer;
    (void)userData;
    (void)type;
    if (hint == AUDIOSTREAM_INTERRUPT_HINT_PAUSE || hint == AUDIOSTREAM_INTERRUPT_HINT_STOP) {
        g_stop_player_req = 1;
    }
}

static void harmony_on_error(OH_AudioRenderer *renderer, void *userData, OH_AudioStream_Result error) {
    (void)renderer;
    (void)userData;
    OH_LOG_WARN(LOG_APP, "OHAudio error %{public}d", (int)error);
}

static OH_AudioData_Callback_Result harmony_on_write_data(OH_AudioRenderer *renderer, void *userData, void *audioData,
    int32_t audioDataSize) {
    (void)renderer;
    (void)userData;
    if (audioDataSize <= 0 || (audioDataSize % 4) != 0) {
        return AUDIO_DATA_CALLBACK_RESULT_INVALID;
    }

    harmony_drain_stop_request();

    int16_t *interleaved = (int16_t *)audioData;
    const int frames = audioDataSize / 4;

    pthread_mutex_lock(&g_lock);
    if (!g_synth) {
        memset(audioData, 0, (size_t)audioDataSize);
        pthread_mutex_unlock(&g_lock);
        return AUDIO_DATA_CALLBACK_RESULT_VALID;
    }

    int16_t lbuf[VMRP_MIDI_STACK_FRAMES];
    int16_t rbuf[VMRP_MIDI_STACK_FRAMES];
    int done = 0;
    while (done < frames) {
        int chunk = frames - done;
        if (chunk > VMRP_MIDI_STACK_FRAMES) {
            chunk = VMRP_MIDI_STACK_FRAMES;
        }
        /* FluidSynth 3.x: loff/lincr, roff/rincr（立体声各一列 int16，步长 1） */
        fluid_synth_write_s16(g_synth, chunk, lbuf, 0, 1, rbuf, 0, 1);
        for (int i = 0; i < chunk; i++) {
            interleaved[(done + i) * 2] = lbuf[i];
            interleaved[(done + i) * 2 + 1] = rbuf[i];
        }
        done += chunk;
    }
    pthread_mutex_unlock(&g_lock);
    return AUDIO_DATA_CALLBACK_RESULT_VALID;
}

int32 harmony_midi_init(const char *sf2_path) {
    pthread_mutex_lock(&g_lock);
    if (g_inited) {
        pthread_mutex_unlock(&g_lock);
        return MR_SUCCESS;
    }

    if (!sf2_path || sf2_path[0] == '\0' || access(sf2_path, R_OK) != 0) {
        OH_LOG_ERROR(LOG_APP, "harmony_midi_init: bad sf2 path errno=%{public}d", errno);
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }

    g_settings = new_fluid_settings();
    if (!g_settings) {
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }
    fluid_settings_setnum(g_settings, "synth.sample-rate", (double)VMRP_MIDI_SAMPLE_RATE);

    g_synth = new_fluid_synth(g_settings);
    if (!g_synth) {
        delete_fluid_settings(g_settings);
        g_settings = NULL;
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }

    if (fluid_synth_sfload(g_synth, sf2_path, 1) == FLUID_FAILED) {
        OH_LOG_ERROR(LOG_APP, "harmony_midi_init: sfload failed for %{public}s", sf2_path);
        delete_fluid_synth(g_synth);
        g_synth = NULL;
        delete_fluid_settings(g_settings);
        g_settings = NULL;
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }

    if (OH_AudioStreamBuilder_Create(&g_builder, AUDIOSTREAM_TYPE_RENDERER) != 0) {
        goto fail_after_synth;
    }
    OH_AudioStreamBuilder_SetSamplingRate(g_builder, VMRP_MIDI_SAMPLE_RATE);
    OH_AudioStreamBuilder_SetChannelCount(g_builder, 2);
    OH_AudioStreamBuilder_SetSampleFormat(g_builder, AUDIOSTREAM_SAMPLE_S16LE);
    OH_AudioStreamBuilder_SetEncodingType(g_builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    OH_AudioStreamBuilder_SetRendererInfo(g_builder, AUDIOSTREAM_USAGE_GAME);

    OH_AudioRenderer_OnWriteDataCallback wcb = harmony_on_write_data;
    OH_AudioStreamBuilder_SetRendererWriteDataCallback(g_builder, wcb, NULL);

    OH_AudioRenderer_OnInterruptCallback icb = harmony_on_interrupt;
    OH_AudioStreamBuilder_SetRendererInterruptCallback(g_builder, icb, NULL);

    OH_AudioRenderer_OnErrorCallback ecb = harmony_on_error;
    OH_AudioStreamBuilder_SetRendererErrorCallback(g_builder, ecb, NULL);

    if (OH_AudioStreamBuilder_GenerateRenderer(g_builder, &g_renderer) != 0) {
        OH_AudioStreamBuilder_Destroy(g_builder);
        g_builder = NULL;
        goto fail_after_synth;
    }
    OH_AudioStreamBuilder_Destroy(g_builder);
    g_builder = NULL;

    if (OH_AudioRenderer_Start(g_renderer) != 0) {
        OH_LOG_ERROR(LOG_APP, "harmony_midi_init: AudioRenderer_Start failed");
        OH_AudioRenderer_Release(g_renderer);
        g_renderer = NULL;
        goto fail_after_synth;
    }

    g_player = NULL;
    g_inited = 1;
    pthread_mutex_unlock(&g_lock);
    OH_LOG_INFO(LOG_APP, "harmony_midi_init ok sf2=%{public}s", sf2_path);
    return MR_SUCCESS;

fail_after_synth:
    delete_fluid_synth(g_synth);
    g_synth = NULL;
    delete_fluid_settings(g_settings);
    g_settings = NULL;
    pthread_mutex_unlock(&g_lock);
    return MR_FAILED;
}

void harmony_midi_shutdown(void) {
    pthread_mutex_lock(&g_lock);
    harmony_stop_player_locked();
    if (g_renderer) {
        OH_AudioRenderer_Stop(g_renderer);
        OH_AudioRenderer_Release(g_renderer);
        g_renderer = NULL;
    }
    if (g_builder) {
        OH_AudioStreamBuilder_Destroy(g_builder);
        g_builder = NULL;
    }
    if (g_synth) {
        delete_fluid_synth(g_synth);
        g_synth = NULL;
    }
    if (g_settings) {
        delete_fluid_settings(g_settings);
        g_settings = NULL;
    }
    g_inited = 0;
    pthread_mutex_unlock(&g_lock);
}

int32 harmony_midi_play(const uint8_t *data, uint32 len, int32 loop) {
    if (!data || len == 0) {
        return MR_FAILED;
    }
    if (!g_inited) {
        return MR_FAILED;
    }

    uint8_t *copy = (uint8_t *)malloc(len);
    if (!copy) {
        return MR_FAILED;
    }
    memcpy(copy, data, len);

    pthread_mutex_lock(&g_lock);
    if (!g_synth) {
        free(copy);
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }

    harmony_stop_player_locked();

    g_player = new_fluid_player(g_synth);
    if (!g_player) {
        free(copy);
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }

    if (fluid_player_add_mem(g_player, copy, len) != FLUID_OK) {
        delete_fluid_player(g_player);
        g_player = NULL;
        free(copy);
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }
    g_midi_owned = copy;

    /*
     * FluidSynth：fluid_player_set_loop 的 loop 是「剩余循环次数」，-1 才表示无限循环；
     * 传 1 只会在首尾再接播约一遍后停止，与 mythroad mr_playSound(loop=1) 期望的持续循环不一致。
     */
    {
        const int fluid_loop = (loop != 0) ? -1 : 0;
        if (fluid_player_set_loop(g_player, fluid_loop) != FLUID_OK) {
            OH_LOG_WARN(LOG_APP, "fluid_player_set_loop(%{public}d) failed", fluid_loop);
        }
    }
    if (fluid_player_play(g_player) != FLUID_OK) {
        harmony_stop_player_locked();
        pthread_mutex_unlock(&g_lock);
        return MR_FAILED;
    }

    pthread_mutex_unlock(&g_lock);
    return MR_SUCCESS;
}

int32 harmony_midi_stop(void) {
    pthread_mutex_lock(&g_lock);
    harmony_stop_player_locked();
    pthread_mutex_unlock(&g_lock);
    return MR_SUCCESS;
}
