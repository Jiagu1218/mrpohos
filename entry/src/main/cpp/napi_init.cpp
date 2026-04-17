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

static napi_env g_env = nullptr;
static napi_threadsafe_function g_timerStartTsfn = nullptr;
static napi_threadsafe_function g_timerStopTsfn = nullptr;

static OHNativeWindow *g_nativeWindow = nullptr;
static OH_NativeXComponent *g_xComponent = nullptr;
static pthread_mutex_t g_windowMutex = PTHREAD_MUTEX_INITIALIZER;

static void renderToNativeWindow(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) {
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

    void *mappedAddr = mmap(bufferHandle->virAddr, bufferHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, bufferHandle->fd, 0);
    if (mappedAddr == MAP_FAILED) {
        pthread_mutex_unlock(&g_windowMutex);
        return;
    }

    int32_t screenW = SCREEN_WIDTH;
    int32_t screenH = SCREEN_HEIGHT;
    uint32_t *dst = (uint32_t *)mappedAddr;
    int32_t stride = bufferHandle->width;

    for (int32_t row = 0; row < h; row++) {
        int32_t screenY = y + row;
        if (screenY < 0 || screenY >= screenH) continue;
        uint16_t *srcRow = bmp + row * w;
        uint32_t *dstRow = dst + screenY * stride;
        for (int32_t col = 0; col < w; col++) {
            int32_t screenX = x + col;
            if (screenX < 0 || screenX >= screenW) continue;
            uint16_t pixel = srcRow[col];
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;
            dstRow[screenX] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
    }

    munmap(mappedAddr, bufferHandle->size);

    Region region = {nullptr, 0};
    OH_NativeWindow_NativeWindowFlushBuffer(window, buffer, -1, region);
    pthread_mutex_unlock(&g_windowMutex);
}

static void onDrawCallback(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) {
    static int drawCount = 0;
    if (drawCount < 20) {
        OH_LOG_INFO(LOG_APP, "drawFrame: x=%{public}d, y=%{public}d, w=%{public}d, h=%{public}d, window=%{public}p", x, y, w, h, g_nativeWindow);
        drawCount++;
    }
    renderToNativeWindow(bmp, x, y, w, h);
}

static void onTimerStartCallback(uint16_t t) {
    OH_LOG_INFO(LOG_APP, "onTimerStartCallback: t=%{public}d, tsfn=%{public}p", (int)t, (void*)g_timerStartTsfn);
    if (!g_timerStartTsfn) return;
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

    int32_t code = -1;
    switch (tp->type) {
        case OH_NATIVEXCOMPONENT_DOWN:  code = 2; break;
        case OH_NATIVEXCOMPONENT_UP:    code = 3; break;
        case OH_NATIVEXCOMPONENT_MOVE:  code = 12; break;
        default: return;
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
        int32_t initRet = bridge_dsm_init(uc);
        OH_LOG_INFO(LOG_APP, "bridge_dsm_init ret=%{public}d", initRet);
        if (initRet == MR_SUCCESS) {
            ret = bridge_dsm_mr_start_dsm(uc, filename, extName, NULL);
            OH_LOG_INFO(LOG_APP, "bridge_dsm_mr_start_dsm ret=%{public}d", ret);
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
    static int timerCount = 0;
    int32_t ret = timer();
    if (timerCount < 10) {
        OH_LOG_INFO(LOG_APP, "NapiTimer: ret=%{public}d, count=%{public}d", ret, timerCount);
        timerCount++;
    }
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
