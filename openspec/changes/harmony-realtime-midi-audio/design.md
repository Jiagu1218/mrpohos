## 上下文

- vmrp 在 Unicorn 模式下通过 `bridge.c` 的 `br_mr_playSound` / `br_mr_stopSound` 承接 MRP 的 `mr_playSound` / `mr_stopSound`；非 Emscripten 构建当前为无操作且返回成功。
- MRP 侧 `MR_SOUND_MIDI` 传入的是 **MIDI 字节缓冲**与 **loop** 标志；宿主需要 **实时** 合成并输出，与此前讨论的离线文件渲染不同。
- 鸿蒙官方 Native 播放 PCM 的推荐路径为 **OHAudio（`libohaudio.so`、`OH_AudioRenderer`）**；MIDI 解码/合成由 **FluidSynth** 承担需单独引入二进制与 SoundFont。

## 目标 / 非目标

**目标：**

- 在 HarmonyOS 原生构建下，使 `MR_SOUND_MIDI` 的 `mr_playSound` 调用可听见、可被 `mr_stopSound(MR_SOUND_MIDI)` 停止。
- `br_mr_playSound` 内仅做轻量工作：校验参数、将 MRP 缓冲 **拷贝** 后投递到宿主音频引擎；**禁止**在 Unicorn 回调线程内执行重计算或阻塞式音频 API。
- 使用 **FluidSynth** 实时生成与 OHAudio 配置一致的 **S16 PCM**，经 **环形缓冲** 在 `OH_AudioRenderer_OnWriteDataCallback` 中供数。
- SoundFont 由宿主在启动或首次播放前加载；路径可通过现有沙箱/资源机制配置。

**非目标：**

- 本变更不要求一次实现 `MR_SOUND_WAV` / `MR_SOUND_MP3` / `MR_MEDIA_*` 全链路。
- 不要求与 ArkTS `AudioRenderer`/`SoundPool` 混用；Native OHAudio 为默认实现面。
- 不要求与 Emscripten / `js_playSound` 行为字节级一致；仅要求 Harmony 侧功能可用。

## 决策

**决策 1：合成与播放分层**

- **FluidSynth** 仅在宿主控制线程（或专用工作线程）中推进：`fluid_player` 或等价 API 加载 MIDI 内存、处理 `loop`；每帧调用 `fluid_synth_write_s16`（或文档推荐之实时写接口）写入 **PCM 环形缓冲**。
- **OHAudio** 仅在系统音频回调中从环形缓冲 **消费** PCM，填满 `audioData` 或按文档返回 `INVALID`；采样率/声道/format 与 FluidSynth 输出 **严格一致**。

**决策 2：`br_mr_playSound` 与 `br_mr_stopSound` 的契约**

- `type != MR_SOUND_MIDI`：返回 `MR_FAILED` 或明确文档化的忽略策略（与规格一致）；禁止假成功若规格要求诚实返回。
- `dataLen == 0` 或 `getMrpMemPtr` 无效：返回 `MR_FAILED`。
- `loop` 语义在 FluidSynth 层实现（循环播放同一缓冲或 player loop），不在 OHAudio 层隐式假设。

**决策 3：SoundFont 来源**

- 默认：**随应用打包的 `.sf2`**，首次运行复制到沙箱或直接使用 `resources`/`rawfile` 解析出的只读路径；**不**从 `mr_playSound` 的 `data` 参数传入 SF2。

**决策 4：依赖交付**

- 使用 **lycium / build-third-party** 或项目内预编译产物；`libfluidsynth.so` 与传递依赖（如 glib 等，以实际编译清单为准）全部进入打包或 `libs/<arch>`，并在 README/任务中列出清单，避免缺库闪退。

## 风险 / 权衡

- **[风险] 音频回调线程与 Unicorn 线程竞态** → **缓解**：MRP 缓冲只读拷贝后入队；回调内只读环形缓冲、无 Unicorn API。
- **[风险] PCM 欠载导致杂音** → **缓解**：足够深度的环形缓冲；FluidSynth 线程优先级与调度；回调返回规则遵守 OHAudio 文档。
- **[风险] `OH_AudioRenderer_Stop` 等阻塞** → **缓解**：不在 UI 主线程调用；Stop 放在工作线程或异步封装。
- **[风险] 传递依赖缺失** → **缓解**：集成检查脚本或启动自检日志；文档列出完整 `.so` 列表。
- **[权衡] 二进制体积与许可** → FluidSynth 及依赖以 LGPL/GPL 等为准，需在 `NOTICE`/开源声明中体现（具体以选用版本为准）。

## 迁移计划

1. 引入预编译库与头文件 → CMake `IMPORTED` + `target_link_libraries`。
2. 实现 `harmony_midi_*`（命名可调整）初始化：加载 SF2、创建 OHAudio renderer、启动控制线程。
3. 修改 `bridge.c` `#else` 分支调用上述模块；保留 wasm 路径不变。
4. 真机验证：播放、循环、停止、快速连续 `play`、低内存场景。
5. 若失败回滚：恢复 `#else` 中空实现提交；库文件可从包中移除。

## 开放问题

- 默认 SoundFont 体积与授权是否接受随应用分发。
- 同一 `type` 下并发多次 `mr_playSound` 的 MRP 语义：截断前播 / 排队 / 混音 —— 需对照 `mythroad` 行为定一条默认策略。
- 最低支持的 Harmony API Level 与 OHAudio API 12+ 回调是否可作为硬前提。
