## 为什么

在 HarmonyOS 模拟器（Mate 70 RS）上，EGL 初始化全部成功（`eglGetDisplay` → `eglInitialize` → `eglChooseConfig` → `eglCreateContext` → `eglMakeCurrent`），但 `glGetString(GL_VERSION)` 在 PBuffer surface 上返回 `nullptr`。代码将此视为**永久失败**（`g_egl_disabled = 1`），不再尝试 Window Surface 直连路径，导致所有后续帧走 CPU `NativeWindow` 渲染。真机上也报告同样走 CPU 路径，说明此问题影响全平台。

华为文档明确指出模拟器支持 OpenGL ES（仅 Mac 不支持 3.1/3.2），FAQ 也提到"模拟器使用 OpenGL ES 指令绘制图像，与真机存在色差"——说明 GLES 在模拟器上应可用。GLES 2.0 是我们请求的版本，全平台理应支持。

## 变更内容

- 将 PBuffer bootstrap 路径中 `build_gl_resources` 返回 `kBuildGlNoDispatch`（`GL_VERSION` 为 null）从**永久失败**改为**可恢复失败**，使代码能继续走到已有的 Window-only fallback 路径（`mrp_gles_renderer.cpp:407-458`）重试。
- 在 `build_gl_resources` 的 `GL_VERSION` null 检测处增加 `eglGetError()` 诊断日志，帮助区分"GLES 驱动未绑定"与"绑定后出错"。
- 在 `lazy_init_egl_gl` 成功路径增加日志输出当前走的是哪种 init 路径（pbuffer-bootstrap / window-only-fallback / window-direct），便于后续诊断。

## 功能 (Capabilities)

### 新增功能

（无 — 本变更为纯修复，不引入新能力。）

### 修改功能

（无 — 现有规范 `path-encoding` 与渲染无关。）

## 影响

- `entry/src/main/cpp/mrp_gles_renderer.cpp` — `build_gl_resources` 返回值处理、`note_lazy_init_failed` 调用点、诊断日志。
- 无 API / 依赖 / 模块配置变更。
