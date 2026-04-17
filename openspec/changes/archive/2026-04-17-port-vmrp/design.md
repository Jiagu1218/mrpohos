## 上下文

vmrp 是一个用 C 语言编写的 MRP 模拟器（约 2650 行核心代码），依赖 Unicorn Engine 执行 ARM 32位机器码。当前支持 Windows（SDL2）和 Web（Emscripten）两个平台。

HarmonyOS 应用采用 Stage 模型，UI 层使用 ArkTS/ETS，原生层通过 NAPI（Node-API 兼容接口）与 C/C++ 交互。项目已包含 NAPI 脚手架（napi_init.cpp + CMakeLists.txt）。

MRP 应用的文件系统从内存卡根目录开始，无盘符。路径格式为 `/mythroad/xxx`（绝对）或 `xxx.mrp`（相对）。HarmonyOS 使用沙箱文件系统，需要路径映射。

### 关键约束
- 目标屏幕：240x320 RGB565（MRP 标准分辨率）
- ARM 代码中的函数调用通过 bridge.c 的函数表拦截和转发
- cfunction.ext 是预编译的 ARM 二进制，不可修改
- HarmonyOS 提供 libz.so，POSIX socket 和 pthread 在 NAPI 层可用

## 目标 / 非目标

**目标：**
- 在 HarmonyOS 上运行基本的 MRP 应用（显示画面、接收触屏/按键输入）
- Unicorn Engine 编译集成到 HarmonyOS 构建系统
- 文件系统路径正确映射到 HarmonyOS 沙箱
- 通过 XComponent 实现画面渲染

**非目标：**
- 不移植 mythroad/ 源码（直接使用预编译的 cfunction.ext）
- 不实现 UI 对话框/菜单/编辑框（原版 native 也是 stub，后续迭代）
- 不实现声音播放（原版 native 也是 stub）
- 不实现短信/电话等手机特有功能
- 不修改 Unicorn Engine 源码

## 决策

### 1. 整体编译 vs NAPI 薄壳

**决策**：将 vmrp 核心 C 代码整体编译进 HarmonyOS .so，通过少量 NAPI 接口暴露给 ArkTS。

**替代方案**：
- 用 ArkTS 重写整个模拟逻辑 → 工作量巨大，不现实
- 只把 Unicorn 编译为 .so，其余用 ArkTS 重写 → bridge 层的 ARM 调用拦截逻辑极难用 ArkTS 实现

**理由**：vmrp 核心是纯 C 计算逻辑，与平台无关。只需替换 SDL2 相关的 5 个接口边界（画面、定时器、输入、文件系统路径、初始化）。

### 2. SDL2 替换方案

**决策**：用 XComponent 替代 SDL2 做画面渲染，ArkTS 事件系统替代 SDL2 事件循环。

**渲染数据流**：
```
ARM 代码调 mr_drawBitmap(buf, x, y, w, h)
  → bridge.c 拦截，读取模拟内存中的 RGB565 像素
  → 通过 NAPI 回调通知 ArkTS 层
  → ArkTS 层将 RGB565 转换为 RGBA8888 写入 XComponent Canvas
```

**理由**：XComponent 是 HarmonyOS 自定义渲染的标准方式，支持直接操作像素缓冲区。

### 3. NAPI 接口设计

**决策**：暴露以下 NAPI 函数：

| NAPI 函数 | 方向 | 说明 |
|-----------|------|------|
| `init(sandboxPath)` | ArkTS → C | 初始化模拟器，传入沙箱路径 |
| `start(filename, extName)` | ArkTS → C | 加载并启动 MRP |
| `event(code, p0, p1)` | ArkTS → C | 传递输入事件 |
| `timer()` | ArkTS → C | 定时器回调 |
| `onDraw(callback)` | C → ArkTS | 注册画面更新回调 |
| `onTimerStart(callback)` | C → ArkTS | 注册定时器启动回调 |

**理由**：保持接口最小化。C 层通过 napi_create_threadsafe_function 回调 ArkTS 层。

### 4. 文件系统适配

**决策**：在 vmrp 初始化时通过 NAPI 传入沙箱根路径，在 fileLib.c 的 my_open() 等函数中做路径映射。

- 绝对路径 `/mythroad/xxx` → `{sandbox}/mythroad/xxx`（去掉前导 `/`，拼上沙箱路径）
- 相对路径 `xxx.mrp` → `{sandbox}/xxx.mrp`

**替代方案**：`chdir()` 到沙箱目录 → 简单但不能处理绝对路径的情况。

**理由**：显式路径映射更可靠，能在 log 中清楚看到实际访问的路径。

### 5. 资源文件分发

**决策**：将 mythroad 资源打包到应用的 `rawfile/` 目录，首次启动时复制到沙箱。

需要打包的文件：
- `cfunction.ext`（核心 ARM 二进制，332KB）
- `mythroad/system/gb12.uc2`、`gb16.uc2`（字体）
- `mythroad/dsm_gm.mrp`（启动器）
- 用户 MRP 应用文件

## 风险 / 权衡

- **[Unicorn 编译配置]** → Unicorn 依赖确认能在 HarmonyOS 编译，但具体 CMake 参数需要调试。缓解：先单独编译 Unicorn 验证。
- **[渲染性能]** → 240x320 RGB565 数据量小（~150KB/帧），通过 NAPI 回调传递应该没压力。缓解：如性能不足可考虑共享内存方案。
- **[ARM 代码实际路径]** → cfunction.ext 内部传给文件操作的路径格式需要运行时验证。缓解：在 my_open() 加日志观察，按需调整路径映射。
- **[bridge.c 函数表完整性]** → bridge.c 的 mr_table_funcMap 有 ~150+ 个函数映射，部分未实现（NULL）。某些 MRP 应用可能调用未实现的函数。缓解：先跑起来，遇到未实现的函数再逐个补充。
