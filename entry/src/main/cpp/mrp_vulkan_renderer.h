#pragma once

#include <native_window/external_window.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int mrp_vulkan_init(OHNativeWindow *window, int32_t width, int32_t height);
void mrp_vulkan_shutdown(void);
int mrp_vulkan_is_ready(void);
int mrp_vulkan_present_rgb565(const uint16_t *rgb565, int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif
