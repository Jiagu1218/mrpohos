## 上下文

当前 MRP 在 CPU 渲染路径下，每次渲染都执行 RGB565→RGBA 转换（`compositeMrpRgbaFromDraw`），然后以 RGBA 格式写入 NativeWindow。这导致每帧都有额外的 CPU 拷贝和字节转换开销。

现有代码：
- `napi_init.cpp`: `flushCpuNativeWindowLocked()` 将 RGBA 数据拷贝到 NativeWindow
- `SET_FORMAT` 使用 `NATIVEBUFFER_PIXEL_FMT_RGBA_8888`

HarmonyOS NativeWindow API 支持通过 `OH_NativeWindow_NativeWindowHandleOpt` 设置 `GET_FORMAT`/`SET_FORMAT`，且 `native_buffer/buffer_common.h` 定义了 `NATIVEBUFFER_PIXEL_FMT_RGB_565 = 3`。

## 目标 / 非目标

**目标：**
- 实现 RGB565 格式 NativeWindow 直通，避免每帧 RGB565→RGBA 转换
- 自动检测并回退到 RGBA 格式（当 RGB565 不可用时）

**非目标：**
- 不修改 GLES2 渲染路径（GL 纹理仍使用 RGBA）
- 不修改已有的 MRP framebuffer 存储格式（仍为 RGB565）

## 决策

### 方案 A：直接设置 RGB565 格式（推荐）
```c
// 在 OnSurfaceCreated 时尝试设置 RGB565
OH_NativeWindow_NativeWindowHandleOpt(w, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_RGB_565);
```
- 优点：实现简单，无需额外缓冲区管理
- 缺点：部分设备可能不支持 RGB565 格式

### 方案 B：创建 RGB565 NativeBuffer 后Attach
使用 `OH_NativeBuffer_Alloc()` 创建 RGB565 格式的 NativeBuffer，然后 Attach 到 NativeWindow。
- 优点：更可控，显式管理 buffer 生命周期
- 缺点：实现复杂，需要更多代码

**选定方案 A**，因为实现简洁。如遇设备兼容问题，可通过 `GET_FORMAT` 探测后回退。

## 风险 / 权衡

1. **设备兼容性**：部分设备/模拟器可能不支持 RGB565
   - 缓解：`GET_FORMAT` 探测，不支持时回退 RGBA
2. **色彩偏差**：RGB565 色彩空间小于 RGBA8888，红色/蓝色精度略低
   - 缓解：MRP 游戏为复古像素风，色彩损失可接受
3. **跨步不匹配**：RGB565 每像素 2 字节，stride 计算需调整
   - 缓解：按 `width * 2` 计算 stride

## 迁移计划

1. 在 `OnSurfaceCreated`/`OnSurfaceChanged` 中尝试设置 RGB565
2. 添加格式探测回退逻辑
3. 修改 `flushCpuNativeWindowLocked` 根据格式选择写入路径
4. 测试：模拟器 + 真机验证