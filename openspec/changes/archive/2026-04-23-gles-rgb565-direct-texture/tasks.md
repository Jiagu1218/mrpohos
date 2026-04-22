## 1. GL 纹理格式改为 RGB565

- [x] 1.1 `mrp_gles_renderer.h`：函数声明从 `mrp_gles_renderer_present_rgba(const uint32_t *rgba, ...)` 改为 `mrp_gles_renderer_present_rgb565(const uint16_t *rgb565, ...)`
- [x] 1.2 `mrp_gles_renderer.cpp`：`build_gl_resources` 中 `glTexImage2D` 内部格式改为 `GL_RGB565`，format 改为 `GL_RGB`，type 改为 `GL_UNSIGNED_SHORT_5_6_5`
- [x] 1.3 `mrp_gles_renderer.cpp`：函数实现从 `present_rgba` 改为 `present_rgb565`，`glTexSubImage2D` 的 format/type 同步改为 `GL_RGB` / `GL_UNSIGNED_SHORT_5_6_5`

## 2. 调用方切换到 RGB565 直传

- [x] 2.1 `napi_init.cpp`：`onDrawCallback` 中 GPU 路径调用改为 `mrp_gles_renderer_present_rgb565(g_mrpRgb565Composite, ...)`，移除 `compositeMrpRgbaFromDraw` 调用
- [x] 2.2 `napi_init.cpp`：移除 `g_mrpRgbaComposite` 缓冲定义和 `compositeMrpRgbaFromDraw` 函数

## 3. 验证

- [x] 3.1 构建并通过，模拟器上画面显示正常（非黑屏/偏色）
