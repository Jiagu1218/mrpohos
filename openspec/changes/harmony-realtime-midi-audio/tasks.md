## 1. 依赖与工程配置

- [ ] 1.1 确定 FluidSynth + 传递依赖的预编译产物来源（lycium/build-third-party 或自建），列出须打入包的 `.so` 清单
- [ ] 1.2 将头文件与按 `OHOS_ARCH` 分目录的库文件接入 `entry` 模块 CMake（`IMPORTED` 目标、`target_link_libraries` 含 `libohaudio.so` 与 `libfluidsynth.so` 等）
- [ ] 1.3 在模块元数据中声明 OHAudio / 多媒体相关能力（以当前 `compileSdkVersion` 模板为准）
- [ ] 1.4 预置或约定默认 `.sf2` 路径，并在启动或首次播放前完成加载与失败日志

## 2. 宿主音频引擎（FluidSynth + OHAudio）

- [ ] 2.1 实现初始化/反初始化：创建 `fluid_settings`/`fluid_synth`（或 player）、加载 SoundFont、配置输出采样格式与 OHAudio `AudioStreamBuilder` 参数一致
- [ ] 2.2 实现 MIDI 缓冲入队：从调用方接收 MIDI 字节拷贝与 `loop` 标志，在工作线程中喂给 FluidSynth
- [ ] 2.3 实现 PCM 环形缓冲：FluidSynth 写满策略与 OHAudio `OnWriteDataCallback` 消费策略对齐（含文档要求的 VALID/INVALID 语义）
- [ ] 2.4 实现 `play`/`stop` 与多实例策略（至少支持「同 type 停止后重播」；若选「新 play 打断旧 play」须在代码注释与行为上一致）
- [ ] 2.5 注册 OHAudio 中断/错误回调，处理焦点类事件至可接受状态（暂停或停止与规格一致）

## 3. bridge 接入

- [ ] 3.1 在 `bridge.c` 非 `__EMSCRIPTEN__` 分支调用宿主引擎：`MR_SOUND_MIDI` 走实时播放，其余 `type` 按规格返回 `MR_FAILED`（或已文档化的行为）
- [ ] 3.2 `br_mr_stopSound` 映射到宿主 `stop(MR_SOUND_MIDI)`；返回值与 `MR_SUCCESS`/`MR_FAILED` 语义对齐
- [ ] 3.3 确保 `getMrpMemPtr` 数据在投递前完成拷贝，且不在音频回调中访问 Unicorn 内存

## 4. 验证

- [ ] 4.1 真机或模拟器：播放短 MIDI、循环播放、播放中 `stop`、连续快速 `play`
- [ ] 4.2 无效 `dataLen`、缺失 SoundFont、缺依赖库时的错误路径无崩溃
