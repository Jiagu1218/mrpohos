#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>
#include <native_buffer/buffer_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <poll.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <hilog/log.h>
#include <cstdarg>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0001
#define LOG_TAG "vmrp_napi"

extern "C" {
#include "vmrp/header/vmrp.h"
#include "vmrp/header/bridge.h"
#include "vmrp/header/fileLib.h"
#include "vmrp/harmony_midi_audio.h"
#include "vmrp/platform_harmony.h"
}

extern "C" uc_engine *getUcEngine();

#include "mrp_gles_renderer.h"

static void logNative(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OH_LOG_INFO(LOG_APP, "%{public}s", buf);
}

static napi_threadsafe_function g_editRequestTsfn = nullptr;
/** 在 JS 线程执行 timer()；由后台线程 sleep(t) 后投递，避免 ArkTS setTimeout 漂移与节流。 */
static napi_threadsafe_function g_timerInvokeTsfn = nullptr;
static std::atomic<uint64_t> g_mrpTimerSeq{0};
/** 最近一次 MRP timerStart(t) 的 t（ms），供 vmrp_perf 与 mrpT 对照。 */
static std::atomic<uint32_t> g_lastMrpTimerT{0};

static napi_value NoopJsCallback(napi_env env, napi_callback_info info) {
    napi_value u;
    napi_get_undefined(env, &u);
    return u;
}

static void CallTimerFromTsfn(napi_env env, napi_value js_callback, void *context, void *data) {
    (void)env;
    (void)js_callback;
    (void)context;
    (void)data;
    using SteadyClock = std::chrono::steady_clock;
    const SteadyClock::time_point wall0 = SteadyClock::now();
    static SteadyClock::time_point s_wallPrev{};
    static int s_perfTicks = 0;
    long gapMs = -1;
    if (s_perfTicks > 0) {
        gapMs = (long)std::chrono::duration_cast<std::chrono::milliseconds>(wall0 - s_wallPrev).count();
    }
    s_wallPrev = wall0;

    const SteadyClock::time_point t0 = SteadyClock::now();
    (void)timer();
    const SteadyClock::time_point t1 = SteadyClock::now();
    const int emuUs = (int)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    s_perfTicks++;
    if (s_perfTicks % 45 == 0) {
        const uint32_t mrpT = g_lastMrpTimerT.load(std::memory_order_relaxed);
        OH_LOG_INFO(LOG_APP,
            "vmrp_perf#%{public}d gapMs=%{public}ld emuUs=%{public}d mrpT=%{public}u | "
            "gap≈上帧timer内工作+sleep(mrpT)+投递抖动; emu≈本帧Unicorn+draw",
            s_perfTicks, gapMs, emuUs, mrpT);
    }
}

struct EditRequestTsfnData {
    char *title;
    char *text;
    int32_t type;
    int32_t max_size;
};

static napi_threadsafe_function g_messageUiTsfn = nullptr;

struct MessageUiTsfnData {
    int32_t op;
    int32_t view_kind;
    int32_t dialog_type;
    char *title;
    char *text;
};

static OHNativeWindow *g_nativeWindow = nullptr;
static OH_NativeXComponent *g_xComponent = nullptr;
static pthread_mutex_t g_windowMutex = PTHREAD_MUTEX_INITIALIZER;

/* RequestBuffer 常给未初始化的新缓冲；只写 dirty 矩形其余为垃圾会闪屏。在此保留与 MRP 对齐的整屏 RGBA 再整幅上传。 */
static uint32_t g_mrpRgbaComposite[(size_t)SCREEN_WIDTH * (size_t)SCREEN_HEIGHT];
static uint16_t g_mrpRgb565Composite[(size_t)SCREEN_WIDTH * (size_t)SCREEN_HEIGHT]; /* RGB565 直通缓冲 */
static uint8_t g_mrpCompositeReady;
static int32_t g_nativeWindowFormat = NATIVEBUFFER_PIXEL_FMT_RGBA_8888; /* 当前格式：RGB565 或 RGBA8888 */
static uint8_t g_rgb565CompositeReady;

static void resetMrpNativeDrawState(void) {
    g_mrpCompositeReady = 0;
    g_rgb565CompositeReady = 0;
    bridge_reset_mr_draw_cache();
}

/** 将本次 dirty 的 RGB565 画入整屏 RGBA 合成缓冲（逻辑分辨率 SCREEN_*）。调用方已持 g_windowMutex。 */
static void compositeMrpRgbaFromDraw(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen) {
    const int32_t bufW = SCREEN_WIDTH;
    const int32_t bufH = SCREEN_HEIGHT;

    if (srcPitch <= 0) {
        srcPitch = w;
    }

    if (!g_mrpCompositeReady) {
        const size_t n = (size_t)SCREEN_WIDTH * (size_t)SCREEN_HEIGHT;
        for (size_t i = 0; i < n; i++) {
            g_mrpRgbaComposite[i] = 0xFF000000u;
        }
        g_mrpCompositeReady = 1;
    }

    for (int32_t row = 0; row < h; row++) {
        int32_t screenY = y + row;
        if (screenY < 0 || screenY >= SCREEN_HEIGHT || screenY >= bufH) {
            continue;
        }
        for (int32_t col = 0; col < w; col++) {
            int32_t screenX = x + col;
            if (screenX < 0 || screenX >= SCREEN_WIDTH || screenX >= bufW) {
                continue;
            }
            int32_t srcX = srcFromFullScreen ? (x + col) : col;
            int32_t srcY = srcFromFullScreen ? (y + row) : row;
            uint16_t pixel = bmp[(size_t)srcY * (size_t)srcPitch + (size_t)srcX];
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;
            g_mrpRgbaComposite[(size_t)screenY * (size_t)SCREEN_WIDTH + (size_t)screenX] =
                0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
    }
}

/** 将本次 dirty 的 RGB565 直接写入直通缓冲（无转换）。调用方已持 g_windowMutex。 */
static void compositeMrpRgb565FromDraw(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen) {
    const int32_t bufW = SCREEN_WIDTH;
    const int32_t bufH = SCREEN_HEIGHT;

    if (srcPitch <= 0) {
        srcPitch = w;
    }

    if (!g_rgb565CompositeReady) {
        const size_t n = (size_t)SCREEN_WIDTH * (size_t)SCREEN_HEIGHT;
        for (size_t i = 0; i < n; i++) {
            g_mrpRgb565Composite[i] = 0;
        }
        g_rgb565CompositeReady = 1;
    }

    for (int32_t row = 0; row < h; row++) {
        int32_t screenY = y + row;
        if (screenY < 0 || screenY >= SCREEN_HEIGHT || screenY >= bufH) {
            continue;
        }
        for (int32_t col = 0; col < w; col++) {
            int32_t screenX = x + col;
            if (screenX < 0 || screenX >= SCREEN_WIDTH || screenX >= bufW) {
                continue;
            }
            int32_t srcX = srcFromFullScreen ? (x + col) : col;
            int32_t srcY = srcFromFullScreen ? (y + row) : row;
            g_mrpRgb565Composite[(size_t)screenY * (size_t)SCREEN_WIDTH + (size_t)screenX] =
                bmp[(size_t)srcY * (size_t)srcPitch + (size_t)srcX];
        }
    }
}

/** RequestBuffer → 根据 g_nativeWindowFormat 拷贝对应缓冲 → Flush。已持 g_windowMutex 且 g_nativeWindow 非空。 */
static void flushCpuNativeWindowLocked(void) {
    OHNativeWindow *window = g_nativeWindow;

    OHNativeWindowBuffer *buffer = nullptr;
    int releaseFenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(window, &buffer, &releaseFenceFd);
    if (ret != 0 || buffer == nullptr) {
        return;
    }

    BufferHandle *bufferHandle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
    if (bufferHandle == nullptr) {
        return;
    }

    if (releaseFenceFd != -1) {
        struct pollfd pollfds = {0};
        pollfds.fd = releaseFenceFd;
        pollfds.events = POLLIN;
        poll(&pollfds, 1, 32);
        close(releaseFenceFd);
    }

    int32_t bufW = bufferHandle->width > 0 ? bufferHandle->width : SCREEN_WIDTH;
    int32_t bufH = bufferHandle->height > 0 ? bufferHandle->height : SCREEN_HEIGHT;

    bool did_mmap = false;
    void *mappedAddr = bufferHandle->virAddr;
    if (mappedAddr == nullptr) {
        mappedAddr =
            mmap(nullptr, (size_t)bufferHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, bufferHandle->fd, 0);
        if (mappedAddr == MAP_FAILED) {
            return;
        }
        did_mmap = true;
    }

    static int32_t s_loggedBufferShape = 0;
    if (!s_loggedBufferShape) {
        OH_LOG_INFO(LOG_APP,
            "NativeBuffer: w=%{public}d h=%{public}d stride=%{public}d format=%{public}d",
            bufW, bufH, bufferHandle->stride, g_nativeWindowFormat);
        s_loggedBufferShape = 1;
    }

    const int32_t copyH = (bufH < SCREEN_HEIGHT) ? bufH : SCREEN_HEIGHT;
    const int32_t copyW = (bufW < SCREEN_WIDTH) ? bufW : SCREEN_WIDTH;

    if (g_nativeWindowFormat == NATIVEBUFFER_PIXEL_FMT_RGB_565) {
        /* RGB565 直通：每像素 2 字节 */
        int32_t strideBytes = bufferHandle->stride;
        if (strideBytes < copyW * (int32_t)sizeof(uint16_t)) {
            strideBytes = copyW * (int32_t)sizeof(uint16_t);
        }
        const size_t rowStrideUint16 = (size_t)strideBytes / sizeof(uint16_t);
        const size_t rowCopyBytes = (size_t)copyW * sizeof(uint16_t);
        uint16_t *dst = (uint16_t *)mappedAddr;
        for (int32_t sy = 0; sy < copyH; sy++) {
            uint16_t *dstRow = dst + (size_t)sy * rowStrideUint16;
            memcpy(dstRow, &g_mrpRgb565Composite[(size_t)sy * (size_t)SCREEN_WIDTH], rowCopyBytes);
        }
    } else {
        /* RGBA8888：每像素 4 字节 */
        int32_t strideBytes = bufferHandle->stride;
        if (strideBytes < (int32_t)sizeof(uint32_t)) {
            strideBytes = bufW * (int32_t)sizeof(uint32_t);
        }
        const size_t rowStrideUint32 = (size_t)strideBytes / sizeof(uint32_t);
        const size_t rowCopyBytes = (size_t)copyW * sizeof(uint32_t);
        uint32_t *dst = (uint32_t *)mappedAddr;
        for (int32_t sy = 0; sy < copyH; sy++) {
            uint32_t *dstRow = dst + (size_t)sy * rowStrideUint32;
            memcpy(dstRow, &g_mrpRgbaComposite[(size_t)sy * (size_t)SCREEN_WIDTH], rowCopyBytes);
        }
    }

    if (did_mmap) {
        munmap(mappedAddr, (size_t)bufferHandle->size);
    }

    Region region = {nullptr, 0};
    OH_NativeWindow_NativeWindowFlushBuffer(window, buffer, -1, region);
}

static void renderToNativeWindow(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen) {
    pthread_mutex_lock(&g_windowMutex);
    if (!g_nativeWindow) {
        pthread_mutex_unlock(&g_windowMutex);
        return;
    }
    /* 填充 RGBA 缓冲（GL 路径用）和 RGB565 缓冲（CPU 路径用） */
    compositeMrpRgbaFromDraw(bmp, x, y, w, h, srcPitch, srcFromFullScreen);
    compositeMrpRgb565FromDraw(bmp, x, y, w, h, srcPitch, srcFromFullScreen);
    flushCpuNativeWindowLocked();
    pthread_mutex_unlock(&g_windowMutex);
}

static void onDrawCallback(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen) {
    static int drawCount = 0;
    if (drawCount < 20) {
        OH_LOG_INFO(LOG_APP,
            "drawFrame: x=%{public}d, y=%{public}d, w=%{public}d, h=%{public}d, pitch=%{public}d fullScr=%{public}d "
            "window=%{public}p",
            x, y, w, h, srcPitch, srcFromFullScreen, g_nativeWindow);
        drawCount++;
    }

    pthread_mutex_lock(&g_windowMutex);
    if (!g_nativeWindow) {
        pthread_mutex_unlock(&g_windowMutex);
        return;
    }
    /* 填充 RGBA 缓冲（GL 路径用）和 RGB565 缓冲（CPU 路径用） */
    compositeMrpRgbaFromDraw(bmp, x, y, w, h, srcPitch, srcFromFullScreen);
    compositeMrpRgb565FromDraw(bmp, x, y, w, h, srcPitch, srcFromFullScreen);
    if (mrp_gles_renderer_is_ready()) {
        if (mrp_gles_renderer_present_rgba(g_mrpRgbaComposite, SCREEN_WIDTH, SCREEN_HEIGHT) == 0) {
            pthread_mutex_unlock(&g_windowMutex);
            return;
        }
        OH_LOG_INFO(LOG_APP, ">>> 走 CPU 渲染路径（GL失败回退）<<<");
    } else {
        OH_LOG_INFO(LOG_APP, ">>> 走 CPU 渲染路径（无GL）<<<");
    }
    flushCpuNativeWindowLocked();
    pthread_mutex_unlock(&g_windowMutex);
}

static void onTimerStartCallback(uint16_t t) {
    g_lastMrpTimerT.store((uint32_t)(unsigned)t, std::memory_order_relaxed);
    if (!g_timerInvokeTsfn) {
        static int s_warnedMissingTsfn;
        if (!s_warnedMissingTsfn) {
            s_warnedMissingTsfn = 1;
            OH_LOG_WARN(LOG_APP, "onTimerStartCallback: g_timerInvokeTsfn null (module Init not run yet)");
        }
        return;
    }
    /* t 为 uint16：曾用 int16 记上次值会导致 65535 记成 -1，与 (int)t 比较恒不等 → 日志刷屏 */
    static uint16_t s_lastLoggedT = 0xFFFFu;
    static int s_haveLastLoggedT = 0;
    if (!s_haveLastLoggedT || t != s_lastLoggedT) {
        s_haveLastLoggedT = 1;
        s_lastLoggedT = t;
        OH_LOG_INFO(LOG_APP, "onTimerStartCallback: t=%{public}u (native sleep + JS-thread timer(); log on change only)",
            (unsigned)t);
    }
    const uint64_t mySeq = ++g_mrpTimerSeq;
    const unsigned delayMs = (unsigned)t;
    std::thread([mySeq, delayMs]() {
        if (delayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
        if (g_mrpTimerSeq.load(std::memory_order_acquire) != mySeq) {
            return;
        }
        if (g_timerInvokeTsfn) {
            napi_call_threadsafe_function(g_timerInvokeTsfn, nullptr, napi_tsfn_blocking);
        }
    }).detach();
}

static void onTimerStopCallback() {
    g_mrpTimerSeq.fetch_add(1, std::memory_order_release);
}

static void callJsMessageUi(napi_env env, napi_value js_cb, void *context, void *data) {
    if (data == nullptr) {
        return;
    }
    MessageUiTsfnData *d = (MessageUiTsfnData *)data;
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_value a0;
    napi_value a1;
    napi_value a2;
    napi_value a3;
    napi_value a4;
    napi_create_int32(env, d->op, &a0);
    napi_create_int32(env, d->view_kind, &a1);
    napi_create_int32(env, d->dialog_type, &a2);
    napi_create_string_utf8(env, d->title, NAPI_AUTO_LENGTH, &a3);
    napi_create_string_utf8(env, d->text, NAPI_AUTO_LENGTH, &a4);
    napi_value argv[5] = {a0, a1, a2, a3, a4};
    napi_call_function(env, undefined, js_cb, 5, argv, nullptr);
    free(d->title);
    free(d->text);
    free(d);
}

static void callJsEditRequest(napi_env env, napi_value js_cb, void *context, void *data) {
    if (data == nullptr) {
        return;
    }
    EditRequestTsfnData *d = (EditRequestTsfnData *)data;
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_value arg0;
    napi_value arg1;
    napi_value arg2;
    napi_value arg3;
    napi_create_string_utf8(env, d->title, NAPI_AUTO_LENGTH, &arg0);
    napi_create_string_utf8(env, d->text, NAPI_AUTO_LENGTH, &arg1);
    napi_create_int32(env, d->type, &arg2);
    napi_create_int32(env, d->max_size, &arg3);
    napi_value argv[4] = {arg0, arg1, arg2, arg3};
    napi_call_function(env, undefined, js_cb, 4, argv, nullptr);
    free(d->title);
    free(d->text);
    free(d);
}

extern "C" void vmrp_harmony_post_message_ui(int32_t op, int32_t view_kind, int32_t dialog_type, const char *title_utf8,
    const char *text_utf8) {
    if (!g_messageUiTsfn) {
        static int s_warnedMissingMessageUiTsfn;
        if (!s_warnedMissingMessageUiTsfn) {
            s_warnedMissingMessageUiTsfn = 1;
            OH_LOG_WARN(LOG_APP,
                "messageUi: onMessageUi not registered (register before init/start like onEditRequest)");
        }
        return;
    }
    MessageUiTsfnData *p = (MessageUiTsfnData *)malloc(sizeof(MessageUiTsfnData));
    if (!p) {
        return;
    }
    p->op = op;
    p->view_kind = view_kind;
    p->dialog_type = dialog_type;
    p->title = strdup(title_utf8 ? title_utf8 : "");
    p->text = strdup(text_utf8 ? text_utf8 : "");
    if (!p->title || !p->text) {
        free(p->title);
        free(p->text);
        free(p);
        return;
    }
    napi_call_threadsafe_function(g_messageUiTsfn, p, napi_tsfn_nonblocking);
}

extern "C" void vmrp_harmony_on_edit_request_for_platform(const char *title, const char *text, int32_t type,
    int32_t max_size) {
    if (!g_editRequestTsfn) {
        static int s_warnedMissingEditTsfn;
        if (!s_warnedMissingEditTsfn) {
            s_warnedMissingEditTsfn = 1;
            OH_LOG_WARN(LOG_APP,
                "editCreate: JS onEditRequest not registered (register before init/start)");
        }
        return;
    }
    EditRequestTsfnData *p = (EditRequestTsfnData *)malloc(sizeof(EditRequestTsfnData));
    if (!p) {
        return;
    }
    p->title = strdup(title ? title : "");
    p->text = strdup(text ? text : "");
    if (!p->title || !p->text) {
        free(p->title);
        free(p->text);
        free(p);
        return;
    }
    p->type = type;
    p->max_size = max_size;
    napi_call_threadsafe_function(g_editRequestTsfn, p, napi_tsfn_nonblocking);
}

static void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    (void)component;
    OHNativeWindow *w = static_cast<OHNativeWindow *>(window);
    pthread_mutex_lock(&g_windowMutex);
    g_nativeWindow = w;
    pthread_mutex_unlock(&g_windowMutex);

    OH_NativeWindow_NativeWindowHandleOpt(w, SET_BUFFER_GEOMETRY, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* 尝试设置 RGB565 格式，失败则回退 RGBA */
    if (OH_NativeWindow_NativeWindowHandleOpt(w, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_RGB_565) == 0) {
        g_nativeWindowFormat = NATIVEBUFFER_PIXEL_FMT_RGB_565;
        OH_LOG_INFO(LOG_APP, ">>> RGB565 直通模式已启用 <<<");
    } else {
        OH_NativeWindow_NativeWindowHandleOpt(w, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_RGBA_8888);
        g_nativeWindowFormat = NATIVEBUFFER_PIXEL_FMT_RGBA_8888;
        OH_LOG_INFO(LOG_APP, ">>> RGBA8888 模式（回退）<<<");
    }

    mrp_gles_renderer_init(w, SCREEN_WIDTH, SCREEN_HEIGHT);

    OH_LOG_INFO(LOG_APP, "OnSurfaceCreated: window=%{public}p, size=%{public}dx%{public}d, format=%{public}d", (void *)w, SCREEN_WIDTH,
        SCREEN_HEIGHT, g_nativeWindowFormat);
}

static void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window) {
    (void)component;
    OHNativeWindow *w = static_cast<OHNativeWindow *>(window);
    pthread_mutex_lock(&g_windowMutex);
    g_nativeWindow = w;
    pthread_mutex_unlock(&g_windowMutex);

    OH_NativeWindow_NativeWindowHandleOpt(w, SET_BUFFER_GEOMETRY, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* 尝试设置 RGB565 格式，失败则回退 RGBA */
    if (OH_NativeWindow_NativeWindowHandleOpt(w, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_RGB_565) == 0) {
        g_nativeWindowFormat = NATIVEBUFFER_PIXEL_FMT_RGB_565;
    } else {
        OH_NativeWindow_NativeWindowHandleOpt(w, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_RGBA_8888);
        g_nativeWindowFormat = NATIVEBUFFER_PIXEL_FMT_RGBA_8888;
    }
    mrp_gles_renderer_init(w, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window) {
    (void)component;
    (void)window;
    pthread_mutex_lock(&g_windowMutex);
    mrp_gles_renderer_shutdown();
    g_nativeWindow = nullptr;
    g_mrpCompositeReady = 0;
    pthread_mutex_unlock(&g_windowMutex);
}

static void DispatchTouchEventCB(OH_NativeXComponent *component, void *window) {
    OH_NativeXComponent_TouchEvent touchEvent;
    int32_t ret = OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent);
    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS || touchEvent.numPoints == 0) return;

    OH_NativeXComponent_TouchPoint *tp = &touchEvent.touchPoints[0];

    uint64_t xCompWidth = 0, xCompHeight = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &xCompWidth, &xCompHeight);
    if (xCompWidth == 0 || xCompHeight == 0) return;

    int32_t mx = (int32_t)(tp->x * SCREEN_WIDTH / (double)xCompWidth);
    int32_t my = (int32_t)(tp->y * SCREEN_HEIGHT / (double)xCompHeight);
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx >= SCREEN_WIDTH) mx = SCREEN_WIDTH - 1;
    if (my >= SCREEN_HEIGHT) my = SCREEN_HEIGHT - 1;

    static int32_t touchLogCount = 0;
    if (touchLogCount < 20) {
        OH_LOG_INFO(LOG_APP, "Touch: type=%{public}d x=%{public}lf y=%{public}lf -> mx=%{public}d my=%{public}d compSize=%{public}llux%{public}llu",
            tp->type, tp->x, tp->y, mx, my, (unsigned long long)xCompWidth, (unsigned long long)xCompHeight);
        touchLogCount++;
    }

    // MOVE 高频时每帧都 event()→uc_emu_start 会占满主线程，易触发 OnVsyncTimeOut（~16ms 一帧预算）。
    // 仅对 MOVE 做时间节流；DOWN/UP 始终投递，避免起点/终点丢失。
    static bool s_moveThrottleArmed = false;
    static std::chrono::steady_clock::time_point s_lastMoveSent{};
    int32_t code = -1;
    switch (tp->type) {
        case OH_NATIVEXCOMPONENT_DOWN:
            s_moveThrottleArmed = false;
            code = 2;
            break;
        case OH_NATIVEXCOMPONENT_UP:
            s_moveThrottleArmed = false;
            code = 3;
            break;
        case OH_NATIVEXCOMPONENT_MOVE: {
            const auto now = std::chrono::steady_clock::now();
            if (s_moveThrottleArmed && now - s_lastMoveSent < std::chrono::milliseconds(25)) {
                return;
            }
            s_moveThrottleArmed = true;
            s_lastMoveSent = now;
            code = 12;
            break;
        }
        default:
            return;
    }

    event(code, mx, my);
}

static napi_value NapiInit(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    size_t pathLen = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &pathLen);
    char *sandboxPath = (char *)malloc(pathLen + 1);
    napi_get_value_string_utf8(env, args[0], sandboxPath, pathLen + 1, &pathLen);

    printf("NapiInit: sandboxPath='%s'\n", sandboxPath);
    OH_LOG_INFO(LOG_APP, "NapiInit: sandboxPath='%{public}s'", sandboxPath);
    fileLib_setSandboxPath(sandboxPath);

    size_t sbLen = strlen(sandboxPath);
    char *sf2Path = (char *)malloc(sbLen + 48u);
    if (sf2Path != nullptr) {
        snprintf(sf2Path, sbLen + 48u, "%s/vmrp/default.sf2", sandboxPath);
        int32_t midiInit = harmony_midi_init(sf2Path);
        OH_LOG_INFO(LOG_APP, "harmony_midi_init path='%{public}s' ret=%{public}d", sf2Path, midiInit);
        free(sf2Path);
    }

    free(sandboxPath);

    resetMrpNativeDrawState();

    vmrp_setCallbacks(onDrawCallback, onTimerStartCallback, onTimerStopCallback);

    int32_t ret = initVmrpAndLoad();
    OH_LOG_INFO(LOG_APP, "initVmrpAndLoad ret=%{public}d", ret);
    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

static napi_value NapiStart(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    size_t len1 = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &len1);
    char *filename = (char *)malloc(len1 + 1);
    napi_get_value_string_utf8(env, args[0], filename, len1 + 1, &len1);

    size_t len2 = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &len2);
    char *extName = (char *)malloc(len2 + 1);
    napi_get_value_string_utf8(env, args[1], extName, len2 + 1, &len2);

    printf("NapiStart: filename='%s', extName='%s'\n", filename, extName);

    uc_engine *uc = getUcEngine();
    OH_LOG_INFO(LOG_APP, "NapiStart: filename='%{public}s', extName='%{public}s', uc=%{public}p", filename, extName, uc);

    int32_t ret = MR_FAILED;
    if (uc) {
        resetMrpNativeDrawState();
        // 分段日志：卡顿时看最后一条即可区分 init vs start_dsm（后者多为 uc_emu_start 不归）
        OH_LOG_INFO(LOG_APP, "vmrp_boot NapiStart step=1 before bridge_dsm_init");
        int32_t initRet = bridge_dsm_init(uc);
        OH_LOG_INFO(LOG_APP, "vmrp_boot NapiStart step=2 after bridge_dsm_init ret=%{public}d", initRet);
        if (initRet == MR_SUCCESS) {
            OH_LOG_INFO(LOG_APP, "vmrp_boot NapiStart step=3 before bridge_dsm_mr_start_dsm");
            ret = bridge_dsm_mr_start_dsm(uc, filename, extName, NULL);
            OH_LOG_INFO(LOG_APP, "vmrp_boot NapiStart step=4 after bridge_dsm_mr_start_dsm ret=%{public}d", ret);
        }
    } else {
        OH_LOG_ERROR(LOG_APP, "NapiStart: uc is NULL!");
    }

    free(filename);
    free(extName);

    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

static napi_value NapiEvent(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    int32_t code, p0, p1;
    napi_get_value_int32(env, args[0], &code);
    napi_get_value_int32(env, args[1], &p0);
    napi_get_value_int32(env, args[2], &p1);

    int32_t ret = event(code, p0, p1);
    OH_LOG_INFO(LOG_APP, "NapiEvent: code=%{public}d, p0=%{public}d, p1=%{public}d, ret=%{public}d", code, p0, p1, ret);
    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

static napi_value NapiTimer(napi_env env, napi_callback_info info) {
    // 必须在调用 NapiTimer 的线程上同步执行：此前用 napi_async_work 会在 FFRT 工作线程跑 timer()，
    // 客态 drawBitmap 会误在非 UI 线程操作 NativeWindow，易触发 OnVsyncTimeOut / ProcessJank。
    int32_t ret = timer();
    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

static napi_value NapiOnTimerStart(napi_env env, napi_callback_info info) {
    (void)info;
    /* MRP 定时已由 native onTimerStartCallback 调度；保留空导出以兼容旧 TypeScript。 */
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

static napi_value NapiSetEditResult(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 1) {
        napi_value undefined;
        napi_get_undefined(env, &undefined);
        return undefined;
    }

    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &len);
    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        napi_value undefined;
        napi_get_undefined(env, &undefined);
        return undefined;
    }
    napi_get_value_string_utf8(env, args[0], buf, len + 1, &len);
    harmony_edit_set_result_utf8(buf);
    free(buf);

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

static napi_value NapiOnMessageUi(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_value resource_name;
    napi_create_string_utf8(env, "MessageUiCallback", NAPI_AUTO_LENGTH, &resource_name);

    napi_threadsafe_function tsfn = nullptr;
    napi_create_threadsafe_function(env, args[0], nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr,
        callJsMessageUi, &tsfn);
    if (g_messageUiTsfn) {
        napi_release_threadsafe_function(g_messageUiTsfn, napi_tsfn_release);
    }
    g_messageUiTsfn = tsfn;

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

static napi_value NapiOnEditRequest(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_value resource_name;
    napi_create_string_utf8(env, "EditRequestCallback", NAPI_AUTO_LENGTH, &resource_name);

    napi_threadsafe_function tsfn;
    napi_create_threadsafe_function(env, args[0], nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr,
        callJsEditRequest, &tsfn);
    if (g_editRequestTsfn) {
        napi_release_threadsafe_function(g_editRequestTsfn, napi_tsfn_release);
    }
    g_editRequestTsfn = tsfn;

    vmrp_setEditRequestCallback(vmrp_harmony_on_edit_request_for_platform);

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

static napi_value NapiOnTimerStop(napi_env env, napi_callback_info info) {
    (void)info;
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        {"init", nullptr, NapiInit, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"start", nullptr, NapiStart, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"event", nullptr, NapiEvent, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"timer", nullptr, NapiTimer, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onTimerStart", nullptr, NapiOnTimerStart, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onTimerStop", nullptr, NapiOnTimerStop, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onMessageUi", nullptr, NapiOnMessageUi, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onEditRequest", nullptr, NapiOnEditRequest, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setEditResult", nullptr, NapiSetEditResult, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    {
        napi_value noop;
        napi_create_function(env, "MrpTimerNoop", NAPI_AUTO_LENGTH, NoopJsCallback, nullptr, &noop);
        napi_value resource_name;
        napi_create_string_utf8(env, "MrpTimerInvoke", NAPI_AUTO_LENGTH, &resource_name);
        napi_threadsafe_function tsfn = nullptr;
        napi_status st = napi_create_threadsafe_function(env, noop, nullptr, resource_name, 0, 1, nullptr, nullptr,
            nullptr, CallTimerFromTsfn, &tsfn);
        if (st == napi_ok) {
            g_timerInvokeTsfn = tsfn;
        } else {
            OH_LOG_ERROR(LOG_APP, "napi_create_threadsafe_function MrpTimerInvoke failed st=%{public}d", (int)st);
        }
    }

    napi_value exportInstance = nullptr;
    napi_status status = napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    if (status == napi_ok && exportInstance != nullptr) {
        OH_NativeXComponent *nativeXComponent = nullptr;
        napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
        if (nativeXComponent != nullptr) {
            g_xComponent = nativeXComponent;
            static OH_NativeXComponent_Callback callback;
            callback.OnSurfaceCreated = OnSurfaceCreatedCB;
            callback.OnSurfaceChanged = OnSurfaceChangedCB;
            callback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
            callback.DispatchTouchEvent = DispatchTouchEventCB;
            OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);
            printf("XComponent callback registered\n");
        }
    }

    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) {
    napi_module_register(&demoModule);
}
