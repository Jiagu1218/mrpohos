## 上下文

当前渲染管线采用 EGL/GLES 2.0 懒初始化。在 Mate 70 RS 模拟器上实测发现两层问题：

**问题 1（已修复）：** PBuffer bootstrap 路径中 `glGetString(GL_VERSION)` 返回 `nullptr` 被视为永久失败，未走到已有的 Window-only fallback。修复后 Window 路径也返回 null，说明根因更深层。

**问题 2（根因）：** HarmonyOS 官方文档明确指定 OpenGL ES 的标准库为 `libGLESv3.so`，config 应使用 `EGL_OPENGL_ES3_BIT`，context 版本应为 `EGL_CONTEXT_CLIENT_VERSION=3`。我们代码链接的是 `libGLESv2.so` + GLES 2.0 context，而 `libGLESv2.so` 在当前 HarmonyOS 版本上不再提供实际 GL 实现，导致 `glGetString` 等函数返回 null。

```
官方文档要求 vs 当前代码:
  库:       libGLESv3.so    vs  libGLESv2.so ❌
  头文件:   <GLES3/gl3.h>   vs  <GLES2/gl2.h> ❌
  Config:   EGL_OPENGL_ES3_BIT vs EGL_OPENGL_ES2_BIT ❌
  Context:  CLIENT_VERSION=3   vs CLIENT_VERSION=2 ❌

GLES 3.0 完全向后兼容 GLES 2.0，现有 shader 无需修改。
```

## 目标 / 非目标

**目标：**

- 将渲染管线从 `libGLESv2.so` / GLES 2.0 升级到 `libGLESv3.so` / GLES 3.0，匹配 HarmonyOS 官方文档要求，使 GPU 渲染路径在模拟器和真机上可用。
- PBuffer 路径中 `kBuildGlNoDispatch` 降级为可恢复失败，允许走到 Window-only fallback 重试。
- 增加诊断日志（`eglGetError`、init 路径标记），便于后续定位类似问题。

**非目标：**

- 不重写 EGL 初始化架构，不引入 Vulkan 或其他渲染 API。
- 不改变 CPU fallback 路径本身的逻辑——它作为最终兜底保持不变。
- 不改变 `flushCpuNativeWindowLocked` 的行为或 NativeWindow format 选择。

## 决策

**决策 1：将渲染管线从 GLES 2.0 升级到 GLES 3.0**

- CMakeLists.txt: `find_library(MRP_GLES2_LIB GLESv2)` → `find_library(MRP_GLES3_LIB GLESv3)`
- mrp_gles_renderer.cpp: `#include <GLES2/gl2.h>` → `#include <GLES3/gl3.h>`
- Config: `EGL_OPENGL_ES2_BIT` → `EGL_OPENGL_ES3_BIT`
- Context: `EGL_CONTEXT_CLIENT_VERSION, 2` → `EGL_CONTEXT_CLIENT_VERSION, 3`
- **理由**：HarmonyOS 官方文档明确指定 `libGLESv3.so` 为 OpenGL ES 的标准库。GLES 3.0 完全向后兼容 2.0，现有 shader（使用 `attribute`/`varying` 而非 `in`/`out`）在兼容模式下仍然可用。

**决策 2：将 `kBuildGlNoDispatch` 在 PBuffer 路径中映射为 `kBuildGlFail`**

- `build_gl_resources` 保持返回 `kBuildGlNoDispatch` 语义不变（表示 GL 上下文无效）。
- 在 `lazy_init_egl_gl` 的 PBuffer bootstrap 分支中，将 `kBuildGlNoDispatch` 与 `kBuildGlFail` 同等对待——触发 Window-only fallback。
- 而非直接调用 `note_lazy_init_failed(1)` 永久禁用。
- **理由**：PBuffer 上 GL 不可用不代表 Window surface 上也不可用。

**决策 3：增加 `eglGetError` 诊断**

- 在 `build_gl_resources` 的 `GL_VERSION == null` 分支中，额外调用 `eglGetError()` 并记录到日志。
- **理由**：区分"EGL 上下文存在但 GL 分发表为空"与"EGL 内部错误"两种情况，便于后续诊断。

**决策 4：Window-only fallback 路径中 `kBuildGlNoDispatch` 仍为永久失败**

- 如果在 Window surface 直连路径上 `GL_VERSION` 仍为 null，则确认 GLES 确实不可用，保持永久禁用。
- **理由**：两条路径都失败说明确实是环境问题，不应无限重试。

**决策 5：增加 init 路径日志**

- 在 `finalize_lazy_init_ok` 调用点记录当前走的是哪种路径（pbuffer-bootstrap / window-only-fallback / window-direct）。
- 当前 `finalize_lazy_init_ok` 已有 `path_tag` 参数，日志已基本就位，确认即可。

## 风险 / 权衡

- **[风险] 升级到 GLES 3.0 后 shader 不兼容** → **缓解**：GLES 3.0 完全向后兼容 GLES 2.0 shader；我们的 shader 使用 `attribute`/`varying`/`gl_FragColor`，这是 GLES 2.0 兼容模式，3.0 驱动必须支持。
- **[风险] 首次 PBuffer GL null → fallback 到 Window → Window 也需要多次重试** → **缓解**：现有的 exponential backoff 机制（`note_lazy_init_failed(0)` + 最多 24 次重试）已覆盖。
- **[权衡] 不改变 `build_gl_resources` 的返回值语义** → 保持其职责单一（检测 GL 可用性），将重试策略交给调用方。
