#include "mrp_gles_renderer.h"

#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <GLES3/gl3.h>
#include <hilog/log.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0001
#define LOG_TAG "vmrp_gles"

static OHNativeWindow *g_oh_window = nullptr;
static int32_t g_pending_w = 0;
static int32_t g_pending_h = 0;

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static EGLConfig g_cfg = nullptr;

static GLuint g_program = 0;
static GLuint g_tex = 0;
static GLuint g_posLoc = 0;
static GLuint g_uvLoc = 0;
static GLuint g_samplerLoc = 0;

static GLuint g_vbo = 0;
static int32_t g_texW = 0;
static int32_t g_texH = 0;
static int g_egl_inited = 0;
static int g_egl_disabled = 0;
static int g_lazy_init_failures = 0;
static constexpr int kLazyInitMaxFailures = 24;
static int64_t g_lazy_retry_after_mono_ns = 0;

static pthread_t g_bound_thread{};
static int g_thread_bound = 0;

static constexpr int kBuildGlOk = 1;
static constexpr int kBuildGlFail = 0;
static constexpr int kBuildGlNoDispatch = 2;

static const char kVs[] = R"(
attribute vec2 aPos;
attribute vec2 aUv;
varying vec2 vUv;
void main() {
  vUv = aUv;
  gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char kFs[] = R"(
precision mediump float;
varying vec2 vUv;
uniform sampler2D uTex;
void main() {
  gl_FragColor = texture2D(uTex, vec2(vUv.x, 1.0 - vUv.y));
}
)";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    if (!sh) {
        return 0;
    }
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        OH_LOG_ERROR(LOG_APP, "GLES shader compile failed: %{public}s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    if (!prog) {
        return 0;
    }
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        OH_LOG_ERROR(LOG_APP, "GLES program link failed: %{public}s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void destroy_gl_objects(void) {
    if (g_tex) {
        glDeleteTextures(1, &g_tex);
        g_tex = 0;
    }
    if (g_vbo) {
        glDeleteBuffers(1, &g_vbo);
        g_vbo = 0;
    }
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
    g_posLoc = g_uvLoc = g_samplerLoc = 0;
    g_texW = g_texH = 0;
}

static void teardown_egl_gl(void) {
    g_thread_bound = 0;
    if (g_dpy == EGL_NO_DISPLAY) {
        g_egl_inited = 0;
        return;
    }
    if (g_egl_inited) {
        if (eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
            destroy_gl_objects();
        } else {
            OH_LOG_WARN(LOG_APP, "teardown: eglMakeCurrent failed, still deleting GL objects");
            destroy_gl_objects();
        }
        eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    } else {
        destroy_gl_objects();
    }
    if (g_surf != EGL_NO_SURFACE) {
        eglDestroySurface(g_dpy, g_surf);
        g_surf = EGL_NO_SURFACE;
    }
    if (g_ctx != EGL_NO_CONTEXT) {
        eglDestroyContext(g_dpy, g_ctx);
        g_ctx = EGL_NO_CONTEXT;
    }
    eglTerminate(g_dpy);
    g_dpy = EGL_NO_DISPLAY;
    g_cfg = nullptr;
    g_egl_inited = 0;
}

static int build_gl_resources(int32_t width, int32_t height) {
    const GLubyte *glv = glGetString(GL_VERSION);
    if (glv == nullptr) {
        EGLint eglErr = eglGetError();
        OH_LOG_ERROR(LOG_APP,
            "lazy_init: GL_VERSION null after eglMakeCurrent — eglGetError=0x%{public}x, will try fallback",
            (unsigned)eglErr);
        return kBuildGlNoDispatch;
    }
    OH_LOG_INFO(LOG_APP, "lazy_init: GL_VERSION %{public}s", (const char *)glv);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, kVs);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kFs);
    if (!vs || !fs) {
        if (vs) {
            glDeleteShader(vs);
        }
        if (fs) {
            glDeleteShader(fs);
        }
        return kBuildGlFail;
    }

    g_program = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!g_program) {
        return kBuildGlFail;
    }

    g_posLoc = (GLuint)glGetAttribLocation(g_program, "aPos");
    g_uvLoc = (GLuint)glGetAttribLocation(g_program, "aUv");
    g_samplerLoc = (GLuint)glGetUniformLocation(g_program, "uTex");

    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, nullptr);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        OH_LOG_ERROR(LOG_APP, "lazy_init: glTexImage2D err=0x%{public}x", (unsigned)err);
        destroy_gl_objects();
        return kBuildGlFail;
    }

    g_texW = width;
    g_texH = height;

    if (g_vbo) {
        glDeleteBuffers(1, &g_vbo);
    }
    static const GLfloat kVerts[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kVerts), kVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return kBuildGlOk;
}

static int64_t mono_ns_now(void) {
    struct timespec ts {};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

static const EGLint kEglWindowSurfAttribs[] = {EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE};

static void log_egl_create_window_surface_failure(const char *stage) {
    const EGLint err = eglGetError();
    const char *name = "EGL_OTHER";
    switch (err) {
        case EGL_BAD_MATCH:
            name = "EGL_BAD_MATCH";
            break;
        case EGL_BAD_CONFIG:
            name = "EGL_BAD_CONFIG";
            break;
        case EGL_BAD_NATIVE_WINDOW:
            name = "EGL_BAD_NATIVE_WINDOW";
            break;
        case EGL_BAD_ALLOC:
            name = "EGL_BAD_ALLOC";
            break;
        default:
            break;
    }
    OH_LOG_ERROR(LOG_APP,
        "lazy_init %{public}s: eglCreateWindowSurface failed eglGetError=0x%{public}x (%{public}s)",
        stage, (unsigned)err, name);
}

static void finalize_lazy_init_ok(EGLint major, EGLint minor, int32_t width, int32_t height, const char *path_tag) {
    (void)eglSwapInterval(g_dpy, 1);
    g_lazy_init_failures = 0;
    g_lazy_retry_after_mono_ns = 0;
    g_egl_inited = 1;
    OH_LOG_INFO(LOG_APP,
        "mrp_gles lazy_init ok (%{public}s) EGL %{public}d.%{public}d tex=%{public}dx%{public}d",
        path_tag, (int)major, (int)minor, (int)width, (int)height);
}

static int64_t lazy_backoff_ns_after_failure(int failure_count) {
    const int64_t baseNs = 250000000LL;
    int shift = failure_count - 1;
    if (shift < 0) {
        shift = 0;
    }
    if (shift > 3) {
        shift = 3;
    }
    int64_t d = baseNs << shift;
    const int64_t capNs = 2000000000LL;
    return d > capNs ? capNs : d;
}

static void note_lazy_init_failed(int permanent) {
    if (permanent) {
        g_egl_disabled = 1;
        OH_LOG_ERROR(LOG_APP, "GLES: permanent init failure, CPU fallback for this surface");
        return;
    }
    g_lazy_init_failures++;
    g_lazy_retry_after_mono_ns = mono_ns_now() + lazy_backoff_ns_after_failure(g_lazy_init_failures);
    if (g_lazy_init_failures == 1 || g_lazy_init_failures == 6 || (g_lazy_init_failures % 12) == 0) {
        OH_LOG_WARN(LOG_APP,
            "GLES: transient init failure %{public}d/%{public}d; retry after backoff",
            g_lazy_init_failures, kLazyInitMaxFailures);
    }
    if (g_lazy_init_failures >= kLazyInitMaxFailures) {
        g_egl_disabled = 1;
        OH_LOG_ERROR(LOG_APP, "GLES: gave up after %{public}d attempts; CPU path.", g_lazy_init_failures);
    }
}

static void abort_lazy_init_partial(EGLSurface kill_pb) {
    if (g_dpy == EGL_NO_DISPLAY) {
        g_cfg = nullptr;
        return;
    }
    if (g_ctx != EGL_NO_CONTEXT) {
        if (g_surf != EGL_NO_SURFACE && eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
            destroy_gl_objects();
        } else if (kill_pb != EGL_NO_SURFACE && eglMakeCurrent(g_dpy, kill_pb, kill_pb, g_ctx)) {
            destroy_gl_objects();
        }
    }
    eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (kill_pb != EGL_NO_SURFACE) {
        eglDestroySurface(g_dpy, kill_pb);
    }
    if (g_surf != EGL_NO_SURFACE) {
        eglDestroySurface(g_dpy, g_surf);
        g_surf = EGL_NO_SURFACE;
    }
    if (g_ctx != EGL_NO_CONTEXT) {
        eglDestroyContext(g_dpy, g_ctx);
        g_ctx = EGL_NO_CONTEXT;
    }
    eglTerminate(g_dpy);
    g_dpy = EGL_NO_DISPLAY;
    g_cfg = nullptr;
}

static int lazy_init_egl_gl(void) {
    if (g_egl_inited || g_egl_disabled) {
        return g_egl_inited;
    }
    OHNativeWindow *window = g_oh_window;
    const int32_t width = g_pending_w;
    const int32_t height = g_pending_h;
    if (!window || width <= 0 || height <= 0) {
        return 0;
    }

    if (g_lazy_init_failures > 0 && !g_egl_disabled) {
        const int64_t now = mono_ns_now();
        if (g_lazy_retry_after_mono_ns > 0 && now < g_lazy_retry_after_mono_ns) {
            return 0;
        }
    }

    OH_LOG_INFO(LOG_APP, "GL: trying eglGetDisplay...");
    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY) {
        OH_LOG_ERROR(LOG_APP, "GL DIAG: eglGetDisplay failed");
        note_lazy_init_failed(1);
        return 0;
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(g_dpy, &major, &minor)) {
        OH_LOG_ERROR(LOG_APP, "lazy_init: eglInitialize failed");
        eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        note_lazy_init_failed(1);
        return 0;
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        OH_LOG_WARN(LOG_APP, "lazy_init: eglBindAPI(EGL_OPENGL_ES_API) failed, continuing");
    }

    static const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLNativeWindowType native_win = reinterpret_cast<EGLNativeWindowType>(window);

    static const EGLint cfg_pb_win[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT | EGL_PBUFFER_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE};

    static const EGLint cfg_win_only[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};

    EGLint ncfg = 0;
    EGLSurface pb_surf = EGL_NO_SURFACE;
    int tried_pbootstrap = 0;

    if (g_lazy_init_failures == 0) {
        if (eglChooseConfig(g_dpy, cfg_pb_win, &g_cfg, 1, &ncfg) && ncfg >= 1) {
            const EGLint pb_attr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
            pb_surf = eglCreatePbufferSurface(g_dpy, g_cfg, pb_attr);
            if (pb_surf != EGL_NO_SURFACE) {
                tried_pbootstrap = 1;
            }
        }
    }

    if (!tried_pbootstrap) {
        if (!eglChooseConfig(g_dpy, cfg_win_only, &g_cfg, 1, &ncfg) || ncfg < 1) {
            OH_LOG_ERROR(LOG_APP, "lazy_init: eglChooseConfig failed");
            eglTerminate(g_dpy);
            g_dpy = EGL_NO_DISPLAY;
            note_lazy_init_failed(1);
            return 0;
        }
    }

    g_ctx = eglCreateContext(g_dpy, g_cfg, EGL_NO_CONTEXT, ctx_attribs);
    if (g_ctx == EGL_NO_CONTEXT) {
        OH_LOG_ERROR(LOG_APP, "lazy_init: eglCreateContext failed");
        if (pb_surf != EGL_NO_SURFACE) {
            eglDestroySurface(g_dpy, pb_surf);
        }
        eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        note_lazy_init_failed(1);
        return 0;
    }

    if (tried_pbootstrap) {
        if (!eglMakeCurrent(g_dpy, pb_surf, pb_surf, g_ctx)) {
            OH_LOG_ERROR(LOG_APP, "lazy_init: eglMakeCurrent(pbuffer) failed");
            abort_lazy_init_partial(pb_surf);
            note_lazy_init_failed(0);
            return 0;
        }

        const int br_pb = build_gl_resources(width, height);
        if (br_pb == kBuildGlNoDispatch || br_pb == kBuildGlFail) {
            OH_LOG_WARN(LOG_APP, "lazy_init: GL not usable on pbuffer (rc=%{public}d), trying direct window surface", br_pb);
            destroy_gl_objects();
            eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(g_dpy, pb_surf);
            pb_surf = EGL_NO_SURFACE;
            eglDestroyContext(g_dpy, g_ctx);
            g_ctx = EGL_NO_CONTEXT;

            if (!eglChooseConfig(g_dpy, cfg_win_only, &g_cfg, 1, &ncfg) || ncfg < 1) {
                OH_LOG_ERROR(LOG_APP, "lazy_init: eglChooseConfig(win-only) fallback failed");
                eglTerminate(g_dpy);
                g_dpy = EGL_NO_DISPLAY;
                note_lazy_init_failed(1);
                return 0;
            }
            g_ctx = eglCreateContext(g_dpy, g_cfg, EGL_NO_CONTEXT, ctx_attribs);
            if (g_ctx == EGL_NO_CONTEXT) {
                eglTerminate(g_dpy);
                g_dpy = EGL_NO_DISPLAY;
                note_lazy_init_failed(1);
                return 0;
            }
            g_surf = eglCreateWindowSurface(g_dpy, g_cfg, native_win, kEglWindowSurfAttribs);
            if (g_surf == EGL_NO_SURFACE) {
                log_egl_create_window_surface_failure("window-only after pbuffer GL failure");
                abort_lazy_init_partial(EGL_NO_SURFACE);
                note_lazy_init_failed(0);
                return 0;
            }
            if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
                OH_LOG_ERROR(LOG_APP, "lazy_init: eglMakeCurrent(window) after pbuffer GL failure");
                abort_lazy_init_partial(EGL_NO_SURFACE);
                note_lazy_init_failed(0);
                return 0;
            }
            {
                const int br_w = build_gl_resources(width, height);
                if (br_w == kBuildGlNoDispatch) {
                    abort_lazy_init_partial(EGL_NO_SURFACE);
                    note_lazy_init_failed(1);
                    return 0;
                }
                if (br_w != kBuildGlOk) {
                    OH_LOG_ERROR(LOG_APP, "lazy_init: direct window path failed after pbuffer GL failure");
                    abort_lazy_init_partial(EGL_NO_SURFACE);
                    note_lazy_init_failed(0);
                    return 0;
                }
            }
            finalize_lazy_init_ok(major, minor, width, height, "window-only fallback");
            return 1;
        }

        g_surf = eglCreateWindowSurface(g_dpy, g_cfg, native_win, kEglWindowSurfAttribs);
        if (g_surf == EGL_NO_SURFACE) {
            log_egl_create_window_surface_failure("after pbuffer bootstrap");
            abort_lazy_init_partial(pb_surf);
            note_lazy_init_failed(0);
            return 0;
        }

        if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
            OH_LOG_ERROR(LOG_APP, "lazy_init: eglMakeCurrent(window) after pbuffer failed");
            abort_lazy_init_partial(pb_surf);
            note_lazy_init_failed(0);
            return 0;
        }

        eglDestroySurface(g_dpy, pb_surf);
        pb_surf = EGL_NO_SURFACE;

        finalize_lazy_init_ok(major, minor, width, height, "pbuffer bootstrap");
        return 1;
    }

    g_surf = eglCreateWindowSurface(g_dpy, g_cfg, native_win, kEglWindowSurfAttribs);
    if (g_surf == EGL_NO_SURFACE) {
        log_egl_create_window_surface_failure("window direct");
        eglDestroyContext(g_dpy, g_ctx);
        g_ctx = EGL_NO_CONTEXT;
        eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        note_lazy_init_failed(0);
        return 0;
    }

    if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
        OH_LOG_ERROR(LOG_APP, "lazy_init: eglMakeCurrent(window) failed");
        abort_lazy_init_partial(EGL_NO_SURFACE);
        note_lazy_init_failed(0);
        return 0;
    }

    {
        const int br_win = build_gl_resources(width, height);
        if (br_win == kBuildGlNoDispatch) {
            abort_lazy_init_partial(EGL_NO_SURFACE);
            note_lazy_init_failed(1);
            return 0;
        }
        if (br_win != kBuildGlOk) {
            OH_LOG_ERROR(LOG_APP, "lazy_init: GL build on window surface failed");
            abort_lazy_init_partial(EGL_NO_SURFACE);
            note_lazy_init_failed(0);
            return 0;
        }
    }

    finalize_lazy_init_ok(major, minor, width, height, "window direct");
    return 1;
}

int mrp_gles_renderer_init(OHNativeWindow *window, int32_t width, int32_t height) {
    if (!window || width <= 0 || height <= 0) {
        return 0;
    }

    if (g_oh_window != nullptr || g_dpy != EGL_NO_DISPLAY || g_egl_inited) {
        mrp_gles_renderer_shutdown();
    }

    g_oh_window = window;
    g_pending_w = width;
    g_pending_h = height;
    g_egl_disabled = 0;
    g_lazy_init_failures = 0;
    g_lazy_retry_after_mono_ns = 0;

    OH_LOG_INFO(LOG_APP,
        "mrp_gles_renderer_init: surface registered %{public}dx%{public}d, EGL/GL deferred to first present",
        (int)width, (int)height);
    return 1;
}

void mrp_gles_renderer_shutdown(void) {
    const int had = (g_oh_window != nullptr || g_dpy != EGL_NO_DISPLAY || g_egl_inited);
    teardown_egl_gl();
    g_oh_window = nullptr;
    g_pending_w = 0;
    g_pending_h = 0;
    g_egl_disabled = 0;
    g_lazy_init_failures = 0;
    g_lazy_retry_after_mono_ns = 0;
    if (had) {
        OH_LOG_INFO(LOG_APP, "mrp_gles_renderer_shutdown done");
    }
}

int mrp_gles_renderer_is_ready(void) {
    return (g_oh_window != nullptr && g_pending_w > 0 && g_pending_h > 0 && !g_egl_disabled) ? 1 : 0;
}

int mrp_gles_renderer_present_rgb565(const uint16_t *rgb565, int32_t width, int32_t height) {
    if (!rgb565 || width != g_pending_w || height != g_pending_h) {
        return -1;
    }
    if (!g_oh_window || g_egl_disabled) {
        return -1;
    }

    if (!g_egl_inited) {
        if (!lazy_init_egl_gl()) {
            return -1;
        }
    }

    if (g_dpy == EGL_NO_DISPLAY || g_surf == EGL_NO_SURFACE || g_ctx == EGL_NO_CONTEXT) {
        return -1;
    }

    pthread_t self = pthread_self();
    if (!g_thread_bound) {
        g_bound_thread = self;
        g_thread_bound = 1;
    } else if (!pthread_equal(g_bound_thread, self)) {
        static int s_warned = 0;
        if (!s_warned) {
            s_warned = 1;
            OH_LOG_WARN(LOG_APP, "mrp_gles_renderer_present_rgb565: thread changed; EGL context may fail");
        }
    }

    if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
        static int s_makeCurrentErr = 0;
        if (s_makeCurrentErr++ < 3) {
            OH_LOG_ERROR(LOG_APP, "present: eglMakeCurrent failed");
        }
        return -1;
    }

    if (width != g_texW || height != g_texH) {
        destroy_gl_objects();
        if (build_gl_resources(width, height) != kBuildGlOk) {
            return -1;
        }
    }

    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rgb565);

    glUseProgram(g_program);
    glUniform1i((GLint)g_samplerLoc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glVertexAttribPointer((GLuint)g_posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)0);
    glVertexAttribPointer((GLuint)g_uvLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray((GLuint)g_posLoc);
    glEnableVertexAttribArray((GLuint)g_uvLoc);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)g_posLoc);
    glDisableVertexAttribArray((GLuint)g_uvLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!eglSwapBuffers(g_dpy, g_surf)) {
        static int s_swap_fail;
        if (s_swap_fail++ < 8) {
            OH_LOG_WARN(LOG_APP, "eglSwapBuffers failed (count=%{public}d)", s_swap_fail);
        }
        return -1;
    }

    return 0;
}
