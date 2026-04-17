## ADDED Requirements

### 需求:HarmonyOS 原生环境必须实时播放 MR_SOUND_MIDI

在 Unicorn/bridge 宿主路径下，当 MRP 调用 `mr_playSound`，且 `type` 为 `MR_SOUND_MIDI` 时，宿主必须将 `data` 指向的 MIDI 字节序列进行实时合成，并通过系统 OHAudio 输出可听 PCM。宿主必须支持 `mr_stopSound` 且 `type` 为 `MR_SOUND_MIDI` 时停止该 MIDI 播放。宿主不得在音频系统回调中直接读取 MRP 虚拟地址空间；必须在进入合成或播放管线前完成 MIDI 数据的拷贝或等价安全传递。

#### 场景:MIDI 播放成功

- **当** MRP 调用 `mr_playSound(MR_SOUND_MIDI, buf, len, loop)`，且 `buf` 指向有效 MIDI 数据、`len` 大于零、SoundFont 已成功加载
- **那么** 用户在设备上必须能听见与该 MIDI 一致的播放输出；若 `loop` 为非零，则宿主必须按循环语义持续播放直至被停止或新的同类型播放策略覆盖（行为须与实现文档一致）

#### 场景:MIDI 停止

- **当** MRP 在 MIDI 正在播放时调用 `mr_stopSound(MR_SOUND_MIDI)`
- **那么** 播放必须停止且不得继续输出可辨别的 MIDI 内容（允许短淡出若实现采用，但须在合理时间内静默）

#### 场景:非 MIDI 类型未实现时的返回值

- **当** MRP 调用 `mr_playSound` 且 `type` 不是 `MR_SOUND_MIDI`
- **那么** 宿主必须返回 `MR_FAILED` 或规格与 `design.md` 中明确记载的其他一致行为，且禁止在未实现路径上虚假返回成功（若规格选择「仅 MIDI」策略）

#### 场景:无效参数

- **当** `mr_playSound` 的 `dataLen` 为零，或 MIDI 缓冲无法从 MRP 地址空间安全读取
- **那么** 宿主必须返回 `MR_FAILED` 且不得崩溃

#### 场景:线程安全

- **当** Unicorn 扩展回调与 OHAudio 写数据回调并发执行
- **那么** 不得出现数据竞争导致的未定义行为；PCM 供数路径必须仅使用在 MRP 线程侧已提交的缓冲或同步原语保护的数据结构
