#Requires -Version 5.1
<#
.SYNOPSIS
  在 Windows 本机用鸿蒙 Native SDK 的 CMake 工具链交叉编译 FluidSynth（arm64-v8a）。

.DESCRIPTION
  前置条件（缺一不可）：
  1) 已安装「鸿蒙命令行工具 / DevEco 自带」的 native SDK，并设置环境变量 OHOS_SDK_NATIVE 指向其中的 native 目录，例如：
       ...\command-line-tools\sdk\default\openharmony\native
     该目录下应存在：
       build\cmake\ohos.toolchain.cmake
       build-tools\cmake\bin\cmake.exe
       llvm\bin\aarch64-unknown-linux-ohos-clang.cmd（或 .exe）

  2) FluidSynth 依赖 GLib2（CMake 里为 REQUIRED）。你必须先准备好「已为 OHOS/arm64 编好的」GLib 安装前缀，并设置：
       GLIB_OHOS_PREFIX
     其下应有 lib\pkgconfig\glib-2.0.pc、gthread-2.0.pc 等。
     若还没有：在 Windows 上最省事仍是用 Git Bash 跑 lycium 先编 glib（或整包 fluidsynth），再把产物目录填给 GLIB_OHOS_PREFIX；
     纯 PowerShell 不借助其它环境去编 glib 成本极高，本脚本不覆盖。

  用法（PowerShell）：
    $env:OHOS_SDK_NATIVE = "D:\...\openharmony\native"
    $env:GLIB_OHOS_PREFIX = "D:\ohos-deps\glib-arm64-install"
    .\build-fluidsynth-ohos-windows.ps1

  可选环境变量：
    FLUIDSYNTH_SRC   FluidSynth 源码目录（默认：与本脚本同级的 fluidsynth-src）
    OHOS_ARCH        默认 arm64-v8a
    INSTALL_PREFIX   默认：与本脚本同级的 install-fluidsynth-ohos-arm64
#>

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FluidSrc = if ($env:FLUIDSYNTH_SRC) { $env:FLUIDSYNTH_SRC } else { Join-Path $ScriptDir "fluidsynth-src" }
$OhosArch = if ($env:OHOS_ARCH) { $env:OHOS_ARCH } else { "arm64-v8a" }
$InstallPrefix = if ($env:INSTALL_PREFIX) { $env:INSTALL_PREFIX } else { Join-Path $ScriptDir "install-fluidsynth-ohos-$OhosArch" }
$BuildDir = Join-Path $ScriptDir "build-fluidsynth-ohos-$OhosArch"

if (-not $env:OHOS_SDK_NATIVE) {
    Write-Error "请设置环境变量 OHOS_SDK_NATIVE 为鸿蒙 native 目录（含 build\cmake\ohos.toolchain.cmake）。"
}
$Native = $env:OHOS_SDK_NATIVE.TrimEnd('\', '/')
$Toolchain = Join-Path $Native "build\cmake\ohos.toolchain.cmake"
$CmakeBin = Join-Path $Native "build-tools\cmake\bin\cmake.exe"
if (-not (Test-Path $Toolchain)) { Write-Error "找不到工具链文件: $Toolchain" }
if (-not (Test-Path $CmakeBin)) { Write-Error "找不到 OHOS 自带 cmake: $CmakeBin" }

if (-not $env:GLIB_OHOS_PREFIX) {
    Write-Error @"
未设置 GLIB_OHOS_PREFIX。FluidSynth 强制依赖 GLib2，请先准备 OHOS 版 GLib 安装前缀（含 lib\pkgconfig\glib-2.0.pc）。
可用方式示例：
  - 在 Git Bash 中按 lycium 文档先编 glib，再把 install 前缀路径填到 GLIB_OHOS_PREFIX；
  - 或使用他人提供的、与当前 API/SDK 匹配的 OHOS arm64 预编译包。
"@
}
$GlibPrefix = $env:GLIB_OHOS_PREFIX.TrimEnd('\', '/')
$GlibPc = Join-Path $GlibPrefix "lib\pkgconfig\glib-2.0.pc"
if (-not (Test-Path $GlibPc)) {
    Write-Error "GLIB_OHOS_PREFIX 下未找到 lib\pkgconfig\glib-2.0.pc: $GlibPc"
}

if (-not (Test-Path (Join-Path $FluidSrc "CMakeLists.txt"))) {
    Write-Host "未找到 FluidSynth 源码，正在克隆 v2.3.4 到: $FluidSrc"
    New-Item -ItemType Directory -Force -Path (Split-Path $FluidSrc) | Out-Null
    if (Test-Path $FluidSrc) { Remove-Item -Recurse -Force $FluidSrc }
    git clone --depth 1 --branch v2.3.4 "https://github.com/FluidSynth/fluidsynth.git" $FluidSrc
}

if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Write-Error "未在 PATH 中找到 ninja。请先安装 Ninja（例如 `scoop install ninja` 或 `choco install ninja`）后重试；OHOS 交叉编译通常使用 Ninja 生成器。"
}

$env:PATH = "$(Join-Path $Native 'build-tools\cmake\bin');$(Join-Path $Native 'llvm\bin');$env:PATH"

# 让 CMake/pkg-config 能找到 OHOS 版 glib
$pkgDir = Join-Path $GlibPrefix "lib\pkgconfig"
$env:PKG_CONFIG_PATH = $pkgDir
$prefixPath = $GlibPrefix.Replace("\", "/")

if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Push-Location $BuildDir
try {
    & $CmakeBin -G Ninja `
        -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
        -DOHOS_ARCH=$OhosArch `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_INSTALL_PREFIX="$InstallPrefix" `
        -DCMAKE_PREFIX_PATH="$prefixPath" `
        -DBUILD_SHARED_LIBS=ON `
        -Denable-alsa=OFF `
        -Denable-oss=OFF `
        -Denable-msgsmms=OFF `
        -Denable-jack=OFF `
        -Denable-midishare=OFF `
        -Denable-opensles=OFF `
        -Denable-oboe=OFF `
        -Denable-network=OFF `
        -Denable-dsound=OFF `
        -Denable-waveout=OFF `
        -Denable-winmidi=OFF `
        -Denable-sdl2=OFF `
        -Denable-libsndfile=OFF `
        -Denable-dbus=OFF `
        -Denable-pulseaudio=OFF `
        -Denable-readline=OFF `
        -Denable-libinstpatch=OFF `
        -Denable-ladspa=OFF `
        -Denable-pipewire=OFF `
        -Denable-systemd=OFF `
        -Denable-lash=OFF `
        -Denable-wasapi=OFF `
        -Denable-ipv6=OFF `
        "$FluidSrc"

    & $CmakeBin --build . --parallel
    & $CmakeBin --install .
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "安装前缀: $InstallPrefix"
Write-Host "请把头文件整理到: <VMRP_FLUIDSYNTH_ROOT>\include"
Write-Host "把 libfluidsynth.so 及 readelf NEEDED 所列依赖放入: <VMRP_FLUIDSYNTH_ROOT>\lib\$OhosArch\"
Write-Host "然后对 entry 打开 -DVMRP_NATIVE_MIDI=ON -DVMRP_FLUIDSYNTH_ROOT=..."
