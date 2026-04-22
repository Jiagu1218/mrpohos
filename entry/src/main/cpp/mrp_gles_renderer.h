#pragma once

#include <native_window/external_window.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 登记 NativeWindow 与逻辑分辨率；不在此函数内创建 EGL/GL（避免 OnSurfaceCreated 主线程与
 * 系统图形栈竞态）。成功返回 1。
 */
int mrp_gles_renderer_init(OHNativeWindow *window, int32_t width, int32_t height);

/** 释放登记与 EGL/GL 资源。 */
void mrp_gles_renderer_shutdown(void);

/** 是否已登记有效 surface 且未标记 GLES 懒加载失败（可尝试 present）。 */
int mrp_gles_renderer_is_ready(void);

/**
 * 首次调用时在同一线程懒加载 EGL+GL，随后每帧上传 RGB565 纹理、绘制、swap。
 * 返回 0 成功；非 0 时上层应回退 CPU NativeWindow 路径。
 */
int mrp_gles_renderer_present_rgb565(const uint16_t *rgb565, int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif
