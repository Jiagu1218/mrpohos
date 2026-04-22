## 为什么

当前 MRP 在 CPU 路径下每次渲染都需要将 RGB565  framebuffer 转换为 RGBA 再写入 NativeWindow，导致每帧都有额外的 CPU 消耗。HarmonyOS NativeWindow 支持 RGB565 格式配置，可直接写入 16-bit 像素数据，避免转换开销。

## 变更内容

1. **新增 RGB565 直通渲染路径**：在 NativeWindow 支持 RGB565 时创建 RGB565 NativeBuffer 并直接写入
2. **自动回退**：当 RGB565 不可用时回退到现有 RGBA 路径
3. **GLES2 路径兼容**：GL 纹理上传保持 RGBA 格式不变，仅优化 CPU 路径

## 功能 (Capabilities)

### 新增功能
- `nativewindow-rgb565`: HarmonyOS NativeWindow RGB565 格式直通渲染

### 修改功能
- 无

## 影响

- `entry/src/main/cpp/napi_init.cpp` - 修改 `flushCpuNativeWindowLocked()` 和创建 RGB565 路径
- 需检测 `OH_NativeWindow_NativeWindowHandleOpt` 的 `GET_FORMAT` 支持