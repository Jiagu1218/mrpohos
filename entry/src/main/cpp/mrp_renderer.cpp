#include "mrp_renderer.h"

#include "mrp_gles_renderer.h"
#include "mrp_vulkan_renderer.h"

#include <hilog/log.h>
#include <string.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0001
#define LOG_TAG "vmrp_renderer"

static OHNativeWindow *g_window = nullptr;
static int32_t g_width = 0;
static int32_t g_height = 0;
static int g_preferred_mode = MRP_RENDERER_AUTO;
static int g_active_backend = MRP_ACTIVE_NONE;
static int g_initialized = 0;

static const char *mode_name(int mode) {
    switch (mode) {
        case MRP_RENDERER_AUTO: return "AUTO";
        case MRP_RENDERER_VULKAN: return "VULKAN";
        case MRP_RENDERER_GLES: return "GLES";
        case MRP_RENDERER_CPU: return "CPU";
        default: return "?";
    }
}

static const char *backend_name(int backend) {
    switch (backend) {
        case MRP_ACTIVE_NONE: return "NONE";
        case MRP_ACTIVE_VULKAN: return "Vulkan";
        case MRP_ACTIVE_GLES: return "GLES";
        case MRP_ACTIVE_CPU: return "CPU";
        default: return "?";
    }
}

static int try_init_vulkan(void) {
    if (mrp_vulkan_init(g_window, g_width, g_height)) {
        OH_LOG_INFO(LOG_APP, "mrp_renderer: Vulkan backend initialized");
        return MRP_ACTIVE_VULKAN;
    }
    OH_LOG_WARN(LOG_APP, "mrp_renderer: Vulkan init failed, trying next");
    return MRP_ACTIVE_NONE;
}

static int try_init_gles(void) {
    if (mrp_gles_renderer_init(g_window, g_width, g_height)) {
        OH_LOG_INFO(LOG_APP, "mrp_renderer: GLES backend initialized");
        return MRP_ACTIVE_GLES;
    }
    OH_LOG_WARN(LOG_APP, "mrp_renderer: GLES init failed");
    return MRP_ACTIVE_NONE;
}

static void do_init(void) {
    g_active_backend = MRP_ACTIVE_NONE;

    int start;
    switch (g_preferred_mode) {
        case MRP_RENDERER_VULKAN: start = 1; break;
        case MRP_RENDERER_GLES:  start = 2; break;
        case MRP_RENDERER_CPU:   start = 3; break;
        default:                 start = 1; break;
    }

    for (int step = start; step <= 3; step++) {
        if (step == 1) {
            int r = try_init_vulkan();
            if (r != MRP_ACTIVE_NONE) { g_active_backend = r; break; }
        } else if (step == 2) {
            int r = try_init_gles();
            if (r != MRP_ACTIVE_NONE) { g_active_backend = r; break; }
        } else {
            g_active_backend = MRP_ACTIVE_CPU;
            OH_LOG_INFO(LOG_APP, "mrp_renderer: using CPU fallback");
            break;
        }
    }

    OH_LOG_INFO(LOG_APP,
        "mrp_renderer: preferred=%{public}s active=%{public}s",
        mode_name(g_preferred_mode), backend_name(g_active_backend));
}

int mrp_renderer_init(OHNativeWindow *window, int32_t width, int32_t height) {
    if (!window || width <= 0 || height <= 0) {
        return 0;
    }
    if (g_initialized) {
        mrp_renderer_shutdown();
    }
    g_window = window;
    g_width = width;
    g_height = height;
    g_active_backend = MRP_ACTIVE_NONE;
    g_initialized = 1;

    do_init();
    return 1;
}

void mrp_renderer_shutdown(void) {
    if (!g_initialized) return;
    if (g_active_backend == MRP_ACTIVE_VULKAN) {
        mrp_vulkan_shutdown();
    } else if (g_active_backend == MRP_ACTIVE_GLES) {
        mrp_gles_renderer_shutdown();
    }
    OH_LOG_INFO(LOG_APP, "mrp_renderer: shutdown (was %{public}s)", backend_name(g_active_backend));
    g_window = nullptr;
    g_width = 0;
    g_height = 0;
    g_active_backend = MRP_ACTIVE_NONE;
    g_initialized = 0;
}

int mrp_renderer_is_ready(void) {
    if (!g_window || g_width <= 0 || g_height <= 0) return 0;
    if (g_active_backend == MRP_ACTIVE_CPU) return 1;
    if (g_active_backend == MRP_ACTIVE_VULKAN) return mrp_vulkan_is_ready();
    if (g_active_backend == MRP_ACTIVE_GLES) return mrp_gles_renderer_is_ready();
    return 0;
}

int mrp_renderer_present_rgb565(const uint16_t *rgb565, int32_t width, int32_t height) {
    if (g_active_backend == MRP_ACTIVE_VULKAN) {
        return mrp_vulkan_present_rgb565(rgb565, width, height);
    }
    if (g_active_backend == MRP_ACTIVE_GLES) {
        return mrp_gles_renderer_present_rgb565(rgb565, width, height);
    }
    return -1;
}

void mrp_renderer_set_mode(int mode) {
    if (mode < MRP_RENDERER_AUTO || mode > MRP_RENDERER_CPU) mode = MRP_RENDERER_AUTO;
    g_preferred_mode = mode;
    OH_LOG_INFO(LOG_APP, "mrp_renderer: set preferred mode = %{public}s", mode_name(mode));
}

int mrp_renderer_get_mode(void) {
    return g_preferred_mode;
}

int mrp_renderer_get_active_mode(void) {
    return g_active_backend;
}
