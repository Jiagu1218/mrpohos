## 上下文

GPU 渲染路径使用 `mrp_gles_renderer_present_rgba` 上传 RGBA8888 纹理到 GPU。MRP 虚拟机输出的原始像素格式是 RGB565（`uint16_t`），当前在 CPU 侧通过 `compositeMrpRgbaFromDraw` 逐像素转换为 RGBA8888（`uint32_t`），每帧 240×320 = 76800 像素，每像素 5 次位运算。

项目中已经存在 `g_mrpRgb565Composite` 缓冲和 `compositeMrpRgb565FromDraw` 函数（为 CPU fallback 路径服务），直接复用即可。

## 目标 / 非目标

**目标：**

- GPU 路径直接上传 RGB565 纹理，消除 CPU 侧 RGB565→RGBA 转换。
- 纹理上传带宽从 307,200 字节/帧降至 153,600 字节/帧。

**非目标：**

- 不改变 CPU fallback 路径（`flushCpuNativeWindowLocked`）的任何逻辑。
- 不改变 shader 代码。
- 不处理 MRP 输出非 RGB565 的假设场景（MRP 固定输出 RGB565）。

## 决策

**决策 1：纹理格式改为 `GL_RGB` + `GL_UNSIGNED_SHORT_5_6_5`**

- `glTexImage2D` 内部格式：`GL_RGB565`（ES 3.0 支持的 sized internal format）
- `glTexSubImage2D` format/type：`GL_RGB` / `GL_UNSIGNED_SHORT_5_6_5`
- **理由**：直接匹配 MRP 输出格式，零转换直传。GLES 3.0 规范明确支持此组合。

**决策 2：函数重命名为 `mrp_gles_renderer_present_rgb565`**

- 参数从 `const uint32_t *rgba` 改为 `const uint16_t *rgb565`
- **理由**：签名反映实际数据格式，避免调用方传错缓冲。

**决策 3：移除 RGBA 转换相关代码**

- 移除 `g_mrpRgbaComposite`、`compositeMrpRgbaFromDraw`
- `onDrawCallback` 中 GPU 路径直接使用 `g_mrpRgb565Composite`
- **理由**：GPU 路径不再需要 RGBA 缓冲。CPU fallback 路径已独立使用 RGB565 缓冲。

## 风险 / 权衡

- **[风险] 某些 GLES 实现不支持 RGB565 纹理上传** → **缓解**：GLES 3.0 规范强制要求支持 `GL_UNSIGNED_SHORT_5_6_5`；如果不支持，`glTexSubImage2D` 会产生 `GL_INVALID_OPERATION`，可在日志中观察到，回退到 CPU 路径。
- **[权衡] 内部格式 `GL_RGB565` vs `GL_RGB`** → 使用 `GL_RGB565`（sized format）明确告诉驱动不需要额外转换；ES 3.0 支持。
