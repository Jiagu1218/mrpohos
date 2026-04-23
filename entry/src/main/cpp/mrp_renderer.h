#pragma once

#include <native_window/external_window.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum MrpRendererMode {
    MRP_RENDERER_AUTO = 0,
    MRP_RENDERER_VULKAN = 1,
    MRP_RENDERER_GLES = 2,
    MRP_RENDERER_CPU = 3,
};

enum MrpActiveBackend {
    MRP_ACTIVE_NONE = 0,
    MRP_ACTIVE_VULKAN = 1,
    MRP_ACTIVE_GLES = 2,
    MRP_ACTIVE_CPU = 3,
};

int mrp_renderer_init(OHNativeWindow *window, int32_t width, int32_t height);
void mrp_renderer_shutdown(void);
int mrp_renderer_is_ready(void);
int mrp_renderer_present_rgb565(const uint16_t *rgb565, int32_t width, int32_t height);

void mrp_renderer_set_mode(int mode);
int mrp_renderer_get_mode(void);
int mrp_renderer_get_active_mode(void);

#ifdef __cplusplus
}
#endif
