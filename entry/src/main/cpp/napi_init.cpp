#include "napi/native_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

extern "C" {
#include "vmrp/header/vmrp.h"
#include "vmrp/header/bridge.h"
#include "vmrp/header/fileLib.h"
#include "vmrp/platform_harmony.h"
}

extern "C" uc_engine *getUcEngine();

static napi_env g_env = nullptr;
static napi_threadsafe_function g_drawTsfn = nullptr;
static napi_threadsafe_function g_timerStartTsfn = nullptr;
static napi_threadsafe_function g_timerStopTsfn = nullptr;

typedef struct {
    uint16_t *pixels;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} DrawData;

static void onDrawCallback(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!g_drawTsfn) return;
    DrawData *data = (DrawData *)malloc(sizeof(DrawData));
    uint32_t pixelCount = w * h;
    data->pixels = (uint16_t *)malloc(pixelCount * sizeof(uint16_t));
    memcpy(data->pixels, bmp, pixelCount * sizeof(uint16_t));
    data->x = x;
    data->y = y;
    data->w = w;
    data->h = h;
    napi_acquire_threadsafe_function(g_drawTsfn);
    napi_call_threadsafe_function(g_drawTsfn, data, napi_tsfn_nonblocking);
    napi_release_threadsafe_function(g_drawTsfn, napi_tsfn_release);
}

static void onTimerStartCallback(uint16_t t) {
    if (!g_timerStartTsfn) return;
    uint32_t *val = (uint32_t *)malloc(sizeof(uint32_t));
    *val = (uint32_t)t;
    napi_acquire_threadsafe_function(g_timerStartTsfn);
    napi_call_threadsafe_function(g_timerStartTsfn, val, napi_tsfn_nonblocking);
    napi_release_threadsafe_function(g_timerStartTsfn, napi_tsfn_release);
}

static void onTimerStopCallback() {
    if (!g_timerStopTsfn) return;
    uint32_t *val = (uint32_t *)malloc(sizeof(uint32_t));
    *val = 0;
    napi_acquire_threadsafe_function(g_timerStopTsfn);
    napi_call_threadsafe_function(g_timerStopTsfn, val, napi_tsfn_nonblocking);
    napi_release_threadsafe_function(g_timerStopTsfn, napi_tsfn_release);
}

static void callJsDraw(napi_env env, napi_value js_cb, void *context, void *data) {
    if (data == nullptr) return;
    DrawData *drawData = (DrawData *)data;
    napi_value undefined;
    napi_get_undefined(env, &undefined);

    size_t byteLength = drawData->w * drawData->h * sizeof(uint16_t);
    napi_value arrayBuffer;
    void *abData = nullptr;
    napi_create_arraybuffer(env, byteLength, &abData, &arrayBuffer);
    if (abData) {
        memcpy(abData, drawData->pixels, byteLength);
    }

    napi_value args[5];
    napi_create_int32(env, drawData->x, &args[0]);
    napi_create_int32(env, drawData->y, &args[1]);
    napi_create_int32(env, drawData->w, &args[2]);
    napi_create_int32(env, drawData->h, &args[3]);
    args[4] = arrayBuffer;

    napi_call_function(env, undefined, js_cb, 5, args, nullptr);

    free(drawData->pixels);
    free(drawData);
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

static napi_value NapiInit(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    size_t pathLen = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &pathLen);
    char *sandboxPath = (char *)malloc(pathLen + 1);
    napi_get_value_string_utf8(env, args[0], sandboxPath, pathLen + 1, &pathLen);

    printf("NapiInit: sandboxPath='%s'\n", sandboxPath);
    fileLib_setSandboxPath(sandboxPath);
    free(sandboxPath);

    vmrp_setCallbacks(onDrawCallback, onTimerStartCallback, onTimerStopCallback);

    int32_t ret = startVmrp();
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
    int32_t ret = MR_FAILED;
    if (uc) {
        int32_t initRet = bridge_dsm_init(uc);
        if (initRet == MR_SUCCESS) {
            ret = bridge_dsm_mr_start_dsm(uc, filename, extName, NULL);
        }
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
    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

static napi_value NapiTimer(napi_env env, napi_callback_info info) {
    int32_t ret = timer();
    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

static napi_value NapiOnDraw(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_value resource_name;
    napi_create_string_utf8(env, "DrawCallback", NAPI_AUTO_LENGTH, &resource_name);

    napi_threadsafe_function tsfn;
    napi_create_threadsafe_function(env, args[0], nullptr, resource_name, 0, 1, nullptr, nullptr, nullptr, callJsDraw, &tsfn);
    if (g_drawTsfn) {
        napi_release_threadsafe_function(g_drawTsfn, napi_tsfn_release);
    }
    g_drawTsfn = tsfn;
    g_env = env;

    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return undefined;
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
        {"onDraw", nullptr, NapiOnDraw, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onTimerStart", nullptr, NapiOnTimerStart, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onTimerStop", nullptr, NapiOnTimerStop, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
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
