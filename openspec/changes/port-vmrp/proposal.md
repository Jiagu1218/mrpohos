## 为什么

vmrp 是一个开源的 MRP（斯凯/冒泡平台）模拟器，用 Unicorn Engine 模拟执行 ARM 32位机器码，可以运行曾经功能手机上的 MRP 应用（游戏、工具等）。目前 vmrp 支持 Windows（SDL2）和 Web（Emscripten），但不支持 HarmonyOS。将 vmrp 移植到 HarmonyOS 可以让用户在现代鸿蒙设备上重温经典的 MRP 应用。

## 变更内容

- **新增**：将 vmrp 核心 C 代码（vmrp.c, bridge.c, memory.c, fileLib.c, network.c, rbtree.c, utils.c）编译为 HarmonyOS 原生 .so 库
- **新增**：Unicorn Engine 在 HarmonyOS 上的编译集成
- **新增**：通过 NAPI 暴露模拟器接口给 ArkTS 层（初始化、事件传递、定时器回调）
- **新增**：使用 XComponent 替代 SDL2 进行画面渲染（RGB565 → HarmonyOS Canvas）
- **新增**：使用 ArkTS 触屏/按键事件替代 SDL2 事件系统
- **新增**：使用 ArkTS 定时器替代 SDL2 定时器
- **修改**：文件系统层适配 HarmonyOS 沙箱机制（路径映射：内存卡根目录 → 沙箱目录）

## 功能 (Capabilities)

### 新增功能
- `unicorn-engine`: Unicorn Engine 在 HarmonyOS NAPI 层的编译和集成，提供 ARM CPU 模拟能力
- `vmrp-core`: vmrp 核心 C 代码的 HarmonyOS 移植，包括 NAPI 接口、内存管理、函数桥接(bridge)、文件系统适配
- `vmrp-rendering`: 使用 XComponent 的画面渲染系统，接收 C 层的 RGB565 像素数据并显示到屏幕
- `vmrp-input`: 输入事件系统，将 ArkTS 层的触屏和按键事件传递给模拟器的 event() 函数
- `vmrp-timer`: 定时器系统，用 ArkTS 定时器驱动模拟器的 timer() 回调
- `vmrp-resources`: 资源文件管理，将 cfunction.ext、字体、MRP 应用等打包到应用中并解压到沙箱目录

### 修改功能

（无现有功能需修改，这是全新移植）

## 影响

- **C/C++ 原生层**：新增 Unicorn Engine 库 + vmrp 核心代码编译为 libentry.so 的一部分
- **CMakeLists.txt**：需要添加 Unicorn、zlib 的编译和链接配置
- **NAPI 接口**：napi_init.cpp 需要从简单的 add() 函数扩展为完整的模拟器控制接口
- **ArkTS UI 层**：Index.ets 需要从 Hello World 改为 XComponent 渲染 + 事件处理界面
- **资源文件**：需要打包 mythroad 目录结构（cfunction.ext、字体、示例 MRP）到应用中
- **外部依赖**：Unicorn Engine（需交叉编译）、zlib（HarmonyOS 自带 libz.so）
