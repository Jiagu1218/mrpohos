# FluidSynth / OHAudio（实时 MIDI）可选集成

## 1.1 预编译产物与 `.so` 清单（任务 1.1）

使用 [OpenHarmony lycium / tpc_c_cplusplus](https://gitcode.com/openharmony-sig/tpc_c_cplusplus) 或 [build-third-party fluidsynth 说明](https://gitee.com/A00LiuBoyan/build-third-party/raw/master/fluidsynth/README.md) 交叉编译后，除 **`libfluidsynth.so`** 外，**务必核对 `readelf -d libfluidsynth.so` 的 `NEEDED`**，把下列依赖一并打入 HAP 或 `libs/<ABI>/`（缺任一常见表现为启动或首播闪退）。**lycium 常见布局**下 **`libinstpatch-1.0.so.*` 在 `lib64/`** 而不在 `lib/`，CMake 会同时从 **`lib/` 与 `lib64/`** 收集 `*.so*` 拷进包；若你手工整理目录，请别把 `lib64` 漏掉。

- 常见：`glib-2.0` 相关 `.so`、`gthread`（具体以你编译 FluidSynth 时的链接为准）。
- 播放链路：**`libohaudio.so`**（OHAudio，系统侧；CMake 中显式 `target_link_libraries`）。

头文件目录通常为 `${VMRP_FLUIDSYNTH_ROOT}/include`，需包含 **`fluidsynth.h`**。

## 1.2 CMake 打开方式

在 DevEco/CMake 配置中传入：

- `-DVMRP_NATIVE_MIDI=ON`
- `-DVMRP_FLUIDSYNTH_ROOT=<前缀目录>`，目录结构示例：

**本仓库已支持**将 lycium 产物放在 **`entry/src/main/cpp/thirdparty/fluidsynth/<ABI>/`**，例如：

```text
thirdparty/fluidsynth/arm64-v8a/
  include/fluidsynth.h
  lib/libfluidsynth.so
  lib/libglib-2.0.so …（readelf NEEDED 中的其它 .so，需一并随包）
```

CMake 在 **`-DVMRP_NATIVE_MIDI=ON`** 且未手动设置 `VMRP_FLUIDSYNTH_ROOT` 时，会自动指向上述目录（与当前 `OHOS_ARCH` 一致）。**`x86_64` 构建**若该目录不存在，会 **自动回退 stub** 并打印 warning，避免模拟器编不过。

链接原生 MIDI 时，**`CMakeLists.txt` 会在每次 `entry` 目标构建后**，把 **`${VMRP_FLUIDSYNTH_ROOT}/lib` 下所有 `*.so` / `*.so.*`** 复制到 **`libentry.so` 的 CMake 输出目录**（`$<TARGET_FILE_DIR:entry>`，即 `.cxx/.../<ABI>/`），与 **`libentry.so` 同目录** 供 hvigor 一并打进 HAP（设备上多为 **`bundle/libs/arm64/`**）。仅复制到 `src/main/libs/<ABI>/` 在部分 DevEco 链路上**不会**与 CMake 产物合并，仍可能出现 **`libfluidsynth.so.3` 找不到**。

亦可继续使用自定义前缀（旧布局）：

```text
<VMRP_FLUIDSYNTH_ROOT>/
  include/fluidsynth.h
  lib/arm64-v8a/libfluidsynth.so
```

`OHOS_ARCH` 由工程注入（如 `arm64-v8a`）；若未定义，脚本默认 `arm64-v8a`。

默认 **`VMRP_NATIVE_MIDI=OFF`**：仅编译 `harmony_midi_audio_stub.c`，`MR_SOUND_MIDI` 在 `bridge` 中返回 `MR_FAILED`，工程可不携带 FluidSynth。

## 1.3 权限与能力（任务 1.3）

本路径仅 **播放 PCM 输出**，使用 OHAudio **一般不需要**在 `module.json5` 声明麦克风权限。若后续增加录音或其它媒体能力，再按官方清单补充。

## 1.4 SoundFont 路径（任务 1.4）

`napi_init` 在获得沙箱路径后会调用 `harmony_midi_init("<sandbox>/vmrp/default.sf2")`。

请将 **`default.sf2`** 放到应用沙箱下的 **`vmrp/`** 目录（与 `fileLib` 沙箱根拼接）。未放置文件时，**Native 全量构建**下 `harmony_midi_init` 返回 `MR_FAILED`，应用仍启动，但 MIDI 播放失败；**stub 构建**下初始化恒成功且不读文件。

---

## Windows 本机直接编 FluidSynth（PowerShell）

**事实说明：** lycium 主体脚本是 Shell，官方也更常写「Linux / WSL」。在 **纯 Windows** 上，你仍可用 **鸿蒙 Native 自带的 CMake + `ohos.toolchain.cmake` + Ninja** 去编 FluidSynth；但 **FluidSynth 强制依赖 GLib2**，GLib 的 OHOS 交叉编译通常仍需 **Git Bash + lycium**（或现成二进制）先准备好，**单靠本脚本无法从零生成 GLib**。

### 1）准备 OHOS Native 路径

从「鸿蒙命令行工具」或 DevEco 安装目录找到：

`...\command-line-tools\sdk\default\openharmony\native`

设环境变量（示例）：

```powershell
$env:OHOS_SDK_NATIVE = "D:\Huawei\CommandLineTools\sdk\default\openharmony\native"
```

确认存在：`%OHOS_SDK_NATIVE%\build\cmake\ohos.toolchain.cmake` 与 `%OHOS_SDK_NATIVE%\build-tools\cmake\bin\cmake.exe`。

### 2）准备 GLib 的 OHOS 安装前缀（GLIB_OHOS_PREFIX）

目录内需有：`lib\pkgconfig\glib-2.0.pc`（以及 gthread 等）。

- 推荐：在 **Git Bash** 下按 [lycium fluidsynth / glib 脚本](https://gitee.com/han_jin_fei/lycium/tree/master/main/fluidsynth) 或 tpc 文档先编依赖，把 **install 前缀** 路径赋给：

```powershell
$env:GLIB_OHOS_PREFIX = "D:\ohos-cross\glib-install-arm64"
```

### 3）运行本仓库脚本

在 `entry\src\main\cpp\third_party_fluidsynth\` 下执行：

```powershell
.\build-fluidsynth-ohos-windows.ps1
```

可选：`$env:FLUIDSYNTH_SRC` 指向已有 FluidSynth 源码；默认会克隆 `v2.3.4` 到同目录 `fluidsynth-src`。

成功后产物在脚本同级的 `install-fluidsynth-ohos-arm64-v8a\`（与 `OHOS_ARCH` 一致），再按上文 **VMRP_FLUIDSYNTH_ROOT** 目录结构拷贝/整理，并 `readelf -d libfluidsynth.so` 核对 **NEEDED** 依赖一并打入 HAP。

### 4）若 CMake 报找不到 GLib

说明 `GLIB_OHOS_PREFIX` 未包含正确的 **OHOS 目标** pkg-config 文件；请回到步骤 2 用 lycium/现成包重新生成 **目标机** 的 glib，而不是 Windows 主机版 glib。
