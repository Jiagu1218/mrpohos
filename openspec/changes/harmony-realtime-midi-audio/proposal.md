## 为什么

HarmonyOS 原生构建下，`bridge.c` 的 `br_mr_playSound` / `br_mr_stopSound` 在非 Emscripten 路径中直接返回成功且不输出音频，导致 MRP 应用在真机/模拟器上无 MIDI/音效。MRP 运行时需要**实时**将 MIDI 数据合成为 PCM 并送入系统音频，而不是仅离线渲染文件。

## 变更内容

- 在 Native 侧引入 **FluidSynth**（或等价软合成）将 `MR_SOUND_MIDI` 缓冲实时转为 PCM；使用 **OHAudio（`OH_AudioRenderer`）** 将 PCM 输出到设备。
- 在 `br_mr_playSound` / `br_mr_stopSound` 的 `#else` 分支接入上述管线，保持 Emscripten 路径不变。
- 明确 **SoundFont（.sf2）** 的加载路径与资源交付方式；明确 **Unicorn 回调线程** 与 **音频回调线程** 之间的队列/环形缓冲边界，避免在音频回调中访问 MRP 虚拟内存。
- 工程上集成 **libfluidsynth** 及其传递依赖、**libohaudio.so**，并更新 CMake / 模块依赖声明。

## 功能 (Capabilities)

### 新增功能

- `harmony-midi-playback`: 在 HarmonyOS 原生环境下，当 MRP 通过桥调用 `mr_playSound(MR_SOUND_MIDI, …)` 时，将 MIDI 字节缓冲与循环标志交给宿主实时合成并播放；`mr_stopSound(MR_SOUND_MIDI)` 可停止该路径播放。非 MIDI 类型可在本变更中明确为不支持或占位，但须在规格中写清行为。

### 修改功能

（无 — 现有主规格 `path-encoding` 与音频无关，本变更不修改其需求。）

## 影响

- `entry/src/main/cpp/vmrp/bridge.c` — `br_mr_playSound`、`br_mr_stopSound` 非 wasm 分支。
- 新增或扩展 `platform_harmony.*`（或独立 `harmony_midi_audio.*`）封装 FluidSynth + OHAudio 生命周期与线程安全队列。
- `entry` 模块 CMake、可能的 `module.json5` / SDK 能力声明；预置或首次下发的 `.sf2` 资源与沙箱路径。
- 二进制依赖：`libfluidsynth.so` 及 lycium 编译所要求的传递库（须随包或文档列出，避免运行时缺库闪退）。
