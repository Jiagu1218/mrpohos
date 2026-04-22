## 1. 诊断日志增强

- [x] 1.1 在 `build_gl_resources` 的 `GL_VERSION == null` 分支中，增加 `eglGetError()` 日志输出，区分 "EGL 上下文无 GL 分发" 与 "EGL 内部错误"

## 2. 核心修复：GLES 2.0 → 3.0 升级

- [x] 2.1 CMakeLists.txt：`find_library(MRP_GLES2_LIB GLESv2)` → `find_library(MRP_GLES3_LIB GLESv3)`，并更新 `target_link_libraries` 中的引用
- [x] 2.2 mrp_gles_renderer.cpp：`#include <GLES2/gl2.h>` → `#include <GLES3/gl3.h>`
- [x] 2.3 mrp_gles_renderer.cpp：config 中 `EGL_OPENGL_ES2_BIT` → `EGL_OPENGL_ES3_BIT`
- [x] 2.4 mrp_gles_renderer.cpp：context 中 `EGL_CONTEXT_CLIENT_VERSION, 2` → `3`

## 3. PBuffer fallback 修复

- [x] 3.1 修改 `lazy_init_egl_gl` 中 PBuffer bootstrap 分支：将 `build_gl_resources` 返回 `kBuildGlNoDispatch` 时的处理从永久失败改为 Window-only fallback 路径
- [x] 3.2 确认 Window-only fallback 路径中 `kBuildGlNoDispatch` 仍为永久失败，两条路径都失败才最终放弃 GPU

## 4. 验证

- [x] 4.1 在 Mate 70 RS 模拟器上构建并启动应用，确认 hilog 中 `vmrp_gles` 输出有效 GL_VERSION 字符串（含 "OpenGL ES 3"）
- [x] 4.2 确认应用画面正常渲染（非黑屏），GPU 路径生效
