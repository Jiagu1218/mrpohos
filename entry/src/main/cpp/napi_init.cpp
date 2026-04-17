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
#include <chrono>
#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0001
#define LOG_TAG "vmrp_napi"

extern "C" {
#include "vmrp/header/vmrp.h"
#include "vmrp/header/bridge.h"
#include "vmrp/header/fileLib.h"
#include "vmrp/platform_harmony.h"
}

extern "C" uc_engine *getUcEngine();

static void logNative(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OH_LOG_INFO(LOG_APP, "%{public}s", buf);
}

static napi_threadsafe_function g_timerStartTsfn = nullptr;
static napi_threadsafe_function g_timerStopTsfn = nullptr;

static OHNativeWindow *g_nativeWindow = nullptr;
static OH_NativeXComponent *g_xComponent = nullptr;
static pthread_mutex_t g_windowMutex = PTHREAD_MUTEX_INITIALIZER;

/* RequestBuffer 常给未初始化的新缓冲；只写 dirty 矩形其余为垃圾会闪屏。在此保留与 MRP 对齐的整屏 RGBA 再整幅上传。 */
static uint32_t g_mrpRgbaComposite[(size_t)SCREEN_WIDTH * (size_t)SCREEN_HEIGHT];
static uint8_t g_mrpCompositeReady;

static void resetMrpNativeDrawState(void) {
    g_mrpCompositeReady = 0;
    bridge_reset_mr_draw_cache();
}

static void renderToNativeWindow(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h, int32_t srcPitch,
    int32_t srcFromFullScreen) {
    pthread_mutex_lock(&g_windowMutex);
    OHNativeWindow *window = g_nativeWindow;
    if (!window) {
        pthread_mutex_unlock(&g_windowMutex);
        return;
    }

    OHNativeWindowBuffer *buffer = nullptr;
    int releaseFenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(window, &buffer, &releaseFenceFd);
    if (ret != 0 || buffer == nullptr) {
        pthread_mutex_unlock(&g_windowMutex);
        return;
    }

    BufferHandle *bufferHandle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
    if (bufferHandle == nullptr) {
        pthread_mutex_unlock(&g_windowMutex);
        return;
    }

    if (releaseFenceFd != -1) {
        struct pollfd pollfds = {0};
        pollfds.fd = releaseFenceFd;
        pollfds.events = POLLIN;
        poll(&pollfds, 1, 3000);
        close(releaseFenceFd);
    }

    // stride 为每行字节数（见 BufferHandle 文档），真机常有行对齐；不能用 width 当 uint32 行步进，否则花屏。
    int32_t strideBytes = bufferHandle->stride;
    if (strideBytes < (int32_t)sizeof(uint32_t)) {
        strideBytes = bufferHandle->width * (int32_t)sizeof(uint32_t);
    }
    const size_t rowStrideUint32 = (size_t)strideBytes / sizeof(uint32_t);

    void *hintAddr = bufferHandle->virAddr;
    void *mappedAddr =
        mmap(hintAddr, (size_t)bufferHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, bufferHandle->fd, 0);
    if (mappedAddr == MAP_FAILED && hintAddr != nullptr) {
        mappedAddr = mmap(nullptr, (size_t)bufferHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, bufferHandle->fd, 0);
    }
    if (mappedAddr == MAP_FAILED) {
        pthread_mutex_unlock(&g_windowMutex);
        return;
    }

    int32_t bufW = bufferHandle->width > 0 ? bufferHandle->width : SCREEN_WIDTH;
    int32_t bufH = bufferHandle->height > 0 ? bufferHandle->height : SCREEN_HEIGHT;
    uint32_t *dst = (uint32_t *)mappedAddr;

    static int32_t s_loggedBufferShape = 0;
    if (!s_loggedBufferShape) {
        OH_LOG_INFO(LOG_APP,
            "NativeBuffer: w=%{public}d h=%{public}d strideBytes=%{public}d rowUint32=%{public}zu size=%{public}d",
            bufW, bufH, strideBytes, rowStrideUint32, bufferHandle->size);
        s_loggedBufferShape = 1;
    }

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

    for (int32_t sy = 0; sy < bufH && sy < SCREEN_HEIGHT; sy++) {
        uint32_t *dstRow = dst + (size_t)sy * rowStrideUint32;
        for (int32_t sx = 0; sx < bufW && sx < SCREEN_WIDTH; sx++) {
            dstRow[sx] = g_mrpRgbaComposite[(size_t)sy * (size_t)SCREEN_WIDTH + (size_t)sx];
        }
    }

    munmap(mappedAddr, (size_t)bufferHandle->size);

    Region region = {nullptr, 0};
    OH_NativeWindow_NativeWindowFlushBuffer(window, buffer, -1, region);
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
    renderToNativeWindow(bmp, x, y, w, h, srcPitch, srcFromFullScreen);
}

static void onTimerStartCallback(uint16_t t) {
    if (!g_timerStartTsfn) {
        static int s_warnedMissingTsfn;
        if (!s_warnedMissingTsfn) {
            s_warnedMissingTsfn = 1;
            OH_LOG_WARN(LOG_APP,
                "onTimerStartCallback: MRP timerStart(%{public}d) but JS onTimerStart not registered yet (register before init/start)",
                (int)t);
        }
        return;
    }
    static int16_t s_lastLoggedT = -1;
    if ((int)t != s_lastLoggedT) {
        s_lastLoggedT = (int16_t)t;
        OH_LOG_INFO(LOG_APP, "onTimerStartCallback: t=%{public}d (MRP timerStart; log on change only)", (int)t);
    }
    uint32_t *val = (uint32_t *)malloc(sizeof(uint32_t));
    *val = (uint32_t)t;
    napi_call_threadsafe_function(g_timerStartTsfn, val, napi_tsfn_nonblocking);
}

static void onTimerStopCallback() {
    if (!g_timerStopTsfn) return;
    uint32_t *val = (uint32_t *)malloc(sizeof(uint32_t));
    *val = 0;
    napi_call_threadsafe_function(g_timerStopTsfn, val, napi_tsfn_nonblocking);
}

static void callJsTimerStart(napi_env env, napi_value js_cb, void *context, void *data) {
    if (data == nullptr) return;
    uint32_t *val = (uint32_t *)data;
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_value arg;
    napi_create_uint32(env, *val, &arg);
    napi_call_function(env, undefined, js_cb, 1, &arg, nullptr);
    free(val);
}

static void callJsTimerStop(napi_env env, napi_value js_cb, void *context, void *data) {
    if (data == nullptr) return;
    free(data);
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_call_function(env, undefined, js_cb, 0, nullptr, nullptr);
}

static void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    pthread_mutex_lock(&g_windowMutex);
    g_nativeWindow = static_cast<OHNativeWindow *>(window);
    pthread_mutex_unlock(&g_windowMutex);

    int32_t code = SET_BUFFER_GEOMETRY;
    OH_NativeWindow_NativeWindowHandleOpt(g_nativeWindow, code, SCREEN_WIDTH, SCREEN_HEIGHT);

    int32_t formatCode = SET_FORMAT;
    OH_NativeWindow_NativeWindowHandleOpt(g_nativeWindow, formatCode, NATIVEBUFFER_PIXEL_FMT_RGBA_8888);

    OH_LOG_INFO(LOG_APP, "OnSurfaceCreated: window=%{public}p, size=%{public}dx%{public}d", window, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window) {
    pthread_mutex_lock(&g_windowMutex);
    g_nativeWindow = static_cast<OHNativeWindow *>(window);
    pthread_mutex_unlock(&g_windowMutex);
}

static void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window) {
    pthread_mutex_lock(&g_windowMutex);
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
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_value resource_name;
    napi_create_string_utf8(env, "TimerStartCallback", NAPI_AUTO_LENGTH, &resource_name);

    napi_threadsafe_function tsfn;
    napi_create_threadsafe_function(env, args[0], nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr, callJsTimerStart, &tsfn);
    if (g_timerStartTsfn) {
        napi_release_threadsafe_function(g_timerStartTsfn, napi_tsfn_release);
    }
    g_timerStartTsfn = tsfn;

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
}

static napi_value NapiOnTimerStop(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_value resource_name;
    napi_create_string_utf8(env, "TimerStopCallback", NAPI_AUTO_LENGTH, &resource_name);

    napi_threadsafe_function tsfn;
    napi_create_threadsafe_function(env, args[0], nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr, callJsTimerStop, &tsfn);
    if (g_timerStopTsfn) {
        napi_release_threadsafe_function(g_timerStopTsfn, napi_tsfn_release);
    }
    g_timerStopTsfn = tsfn;

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
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

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
