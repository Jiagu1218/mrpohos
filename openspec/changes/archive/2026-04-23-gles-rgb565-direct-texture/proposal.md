## 为什么

GPU 渲染路径已在上一个变更中修复，但当前每帧仍需在 CPU 侧将 MRP 的 RGB565 像素逐个转换为 RGBA8888（`compositeMrpRgbaFromDraw`，76800 像素 × 5 次位移/掩码操作），然后上传 4 字节/像素的纹理到 GPU。OpenGL ES 3.0 原生支持 `GL_UNSIGNED_SHORT_5_6_5` 纹理格式，可以直接上传 RGB565 数据，省掉 CPU 转换并将纹理上传带宽减半。

## 变更内容

- 将 `mrp_gles_renderer_present_rgba` 的纹理格式从 `GL_RGBA / GL_UNSIGNED_BYTE` 改为 `GL_RGB / GL_UNSIGNED_SHORT_5_6_5`，直接接收 RGB565 缓冲。
- `onDrawCallback` 中 GPU 路径不再调用 `compositeMrpRgbaFromDraw`，直接使用已有的 `g_mrpRgb565Composite` 缓冲。
- 移除 `g_mrpRgbaComposite` 缓冲及 `compositeMrpRgbaFromDraw` 函数（CPU fallback 路径已有独立的 RGB565 直传逻辑，不受影响）。

## 功能 (Capabilities)

### 新增功能

（无）

### 修改功能

（无 — 纯性能优化，不改变规范级行为。）

## 影响

- `entry/src/main/cpp/mrp_gles_renderer.cpp` — `build_gl_resources` 纹理创建、`mrp_gles_renderer_present_rgba` 纹理上传。
- `entry/src/main/cpp/napi_init.cpp` — `onDrawCallback` 调度逻辑、移除 RGBA 转换相关代码。
- `entry/src/main/cpp/mrp_gles_renderer.h` — 函数签名（参数类型从 `uint32_t*` 改为 `uint16_t*`）。
