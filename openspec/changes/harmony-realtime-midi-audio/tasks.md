## 1. 依赖与工程配置

- [x] 1.1 确定 FluidSynth + 传递依赖的预编译产物来源（lycium/build-third-party 或自建），列出须打入包的 `.so` 清单
- [x] 1.2 将头文件与按 `OHOS_ARCH` 分目录的库文件接入 `entry` 模块 CMake（`IMPORTED` 目标、`target_link_libraries` 含 `libohaudio.so` 与 `libfluidsynth.so` 等）
- [x] 1.3 在模块元数据中声明 OHAudio / 多媒体相关能力（以当前 `compileSdkVersion` 模板为准）
- [x] 1.4 预置或约定默认 `.sf2` 路径，并在启动或首次播放前完成加载与失败日志

## 2. 宿主音频引擎（FluidSynth + OHAudio）

- [x] 2.1 实现初始化/反初始化：创建 `fluid_settings`/`fluid_synth`（或 player）、加载 SoundFont、配置输出采样格式与 OHAudio `AudioStreamBuilder` 参数一致
- [x] 2.2 实现 MIDI 缓冲入队：从调用方接收 MIDI 字节拷贝与 `loop` 标志，在工作线程中喂给 FluidSynth
- [x] 2.3 实现 PCM 环形缓冲：FluidSynth 写满策略与 OHAudio `OnWriteDataCallback` 消费策略对齐（含文档要求的 VALID/INVALID 语义）
- [x] 2.4 实现 `play`/`stop` 与多实例策略（至少支持「同 type 停止后重播」；若选「新 play 打断旧 play」须在代码注释与行为上一致）
- [x] 2.5 注册 OHAudio 中断/错误回调，处理焦点类事件至可接受状态（暂停或停止与规格一致）

## 3. bridge 接入

- [x] 3.1 在 `bridge.c` 非 `__EMSCRIPTEN__` 分支调用宿主引擎：`MR_SOUND_MIDI` 走实时播放，其余 `type` 按规格返回 `MR_FAILED`（或已文档化的行为）
- [x] 3.2 `br_mr_stopSound` 映射到宿主 `stop(MR_SOUND_MIDI)`；返回值与 `MR_SUCCESS`/`MR_FAILED` 语义对齐
- [x] 3.3 确保 `getMrpMemPtr` 数据在投递前完成拷贝，且不在音频回调中访问 Unicorn 内存

## 4. 验证

- [ ] 4.1 真机或模拟器：播放短 MIDI、循环播放、播放中 `stop`、连续快速 `play`
- [ ] 4.2 缺 SF2 / 缺依赖库路径的手测（代码侧已对 `dataLen<=0`、`init` 失败打日志；全量 Native 需自备 `default.sf2` 与 lycium 产物）

### 4.1 手测步骤（通过后再勾选）

前置：`VMRP_NATIVE_MIDI=ON` + `VMRP_FLUIDSYNTH_ROOT` 配好；沙箱内存在 `<sandbox>/vmrp/default.sf2`；使用能触发 `mr_playSound(MIDI)` 的 MRP。

1. **短 MIDI**：进入游戏/菜单触发一次 BGM 或音效，确认能听到且无明显爆音/卡顿。
2. **循环**：触发 `loop!=0` 的播放，持续若干秒，确认循环不断且可再次 `stop` 静音。
3. **播放中 `stop`**：在 MIDI 播放过程中触发 `mr_stopSound(MR_SOUND_MIDI)`（或游戏内停止音效），确认很快静音且无崩溃。
4. **连点 `play`**：快速连续触发多次 MIDI 播放（新播打断旧播），确认不崩溃、最后一次或策略符合预期（与 `harmony_midi_audio_native.c`「新 play 打断旧 play」一致）。

### 4.2 手测步骤（通过后再勾选）

1. **缺 SF2**：删除或重命名沙箱内 `vmrp/default.sf2`，冷启动应用；看 hilog 中 `harmony_midi_init` 失败日志；进入 MRP 触发 MIDI，应 **`MR_FAILED` / 无声** 且 **不闪退**。
2. **缺依赖 `.so`**：故意从包中去掉某一 `readelf -d libfluidsynth.so` 所示 `NEEDED` 库（仅测试包），安装启动；确认启动或首播是否闪退，并记录（用于核对 README 依赖清单是否完整）。
3. **`dataLen<=0`**：若 MRP 侧可构造，确认不崩溃；否则依赖代码审查 + 日志中无异常栈。
