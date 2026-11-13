/*
 * OpenDTV Software Compatibility EGL 1.4 & OpenGL ES 2.0 Driver
 * Target: ARM32 Android 9 Pie (EABI5)
 *
 * Experimental compatibility renderer for Android bring-up without Mali.
 * A real GLSL frontend validates shaders; the rendering backend is still
 * limited and is not a conformant general-purpose GLES implementation.
 */

#if defined(__arm__) || defined(__i386__)
typedef unsigned int size_t;
#else
typedef unsigned long size_t;
#endif
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

extern void *calloc(size_t nmemb, size_t size);
extern void free(void *ptr);
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);
extern int strcmp(const char *s1, const char *s2);
extern char *strstr(const char *haystack, const char *needle);
extern char *strchr(const char *s, int c);
extern int open(const char *pathname, int flags, ...);
extern int write(int fd, const void *buf, size_t count);
extern int close(int fd);
extern int lseek(int fd, int offset, int whence);

/* --- EGL Types and Constants --- */
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLSurface;
typedef void *EGLClientBuffer;
typedef void *EGLImageKHR;
typedef void *EGLSyncKHR;
typedef int32_t EGLint;
typedef uint32_t EGLBoolean;
typedef unsigned int EGLenum;
typedef void (*__eglMustCastToProperFunctionPointerType)(void);

#define EGL_SUCCESS                     0x3000
#define EGL_NOT_INITIALIZED             0x3001
#define EGL_BAD_ACCESS                  0x3002
#define EGL_BAD_ALLOC                   0x3003
#define EGL_BAD_ATTRIBUTE               0x3004
#define EGL_BAD_CONFIG                  0x3005
#define EGL_BAD_CONTEXT                 0x3006
#define EGL_BAD_CURRENT_SURFACE         0x3007
#define EGL_BAD_DISPLAY                 0x3008
#define EGL_BAD_MATCH                   0x3009
#define EGL_BAD_NATIVE_PIXMAP           0x300A
#define EGL_BAD_NATIVE_WINDOW           0x300B
#define EGL_BAD_PARAMETER               0x300C
#define EGL_BAD_SURFACE                 0x300D
#define EGL_CONTEXT_LOST                0x300E

#define EGL_BUFFER_SIZE                 0x3020
#define EGL_ALPHA_SIZE                  0x3021
#define EGL_BLUE_SIZE                   0x3022
#define EGL_GREEN_SIZE                  0x3023
#define EGL_RED_SIZE                    0x3024
#define EGL_DEPTH_SIZE                  0x3025
#define EGL_STENCIL_SIZE                0x3026
#define EGL_CONFIG_CAVEAT               0x3027
#define EGL_CONFIG_ID                   0x3028
#define EGL_LEVEL                       0x3029
#define EGL_MAX_PBUFFER_HEIGHT          0x302A
#define EGL_MAX_PBUFFER_PIXELS          0x302B
#define EGL_MAX_PBUFFER_WIDTH           0x302C
#define EGL_NATIVE_RENDERABLE           0x302D
#define EGL_NATIVE_VISUAL_ID            0x302E
#define EGL_NATIVE_VISUAL_TYPE          0x302F
#define EGL_SAMPLES                     0x3031
#define EGL_SAMPLE_BUFFERS              0x3032
#define EGL_SURFACE_TYPE                0x3033
#define EGL_TRANSPARENT_TYPE            0x3034
#define EGL_NONE                        0x3038
#define EGL_BIND_TO_TEXTURE_RGB         0x3039
#define EGL_BIND_TO_TEXTURE_RGBA        0x303A
#define EGL_MIN_SWAP_INTERVAL           0x303B
#define EGL_MAX_SWAP_INTERVAL           0x303C
#define EGL_LUMINANCE_SIZE              0x303D
#define EGL_ALPHA_MASK_SIZE             0x303E
#define EGL_COLOR_BUFFER_TYPE           0x303F
#define EGL_RENDERABLE_TYPE             0x3040
#define EGL_CONFORMANT                  0x3042

#define EGL_RGB_BUFFER                  0x308E
#define EGL_LUMINANCE_BUFFER            0x308F

#define EGL_HEIGHT                      0x3056
#define EGL_WIDTH                       0x3057

#define EGL_WINDOW_BIT                  0x0004
#define EGL_PBUFFER_BIT                 0x0001
#define EGL_OPENGL_ES_BIT               0x0001
#define EGL_OPENGL_ES2_BIT              0x0004
#define EGL_OPENGL_ES3_BIT              0x0040

#define EGL_VENDOR                      0x3053
#define EGL_VERSION                     0x3054
#define EGL_EXTENSIONS                  0x3055
#define EGL_CLIENT_APIS                 0x308D

#define EGL_READ                        0x305A
#define EGL_DRAW                        0x3059

#define EGL_CONTEXT_CLIENT_VERSION      0x3098

#define EGL_TRUE                        1
#define EGL_FALSE                       0
#define EGL_NO_CONTEXT                  ((EGLContext)0)
#define EGL_NO_SURFACE                  ((EGLSurface)0)
#define EGL_NO_DISPLAY                  ((EGLDisplay)0)
#define EGL_DEFAULT_DISPLAY             ((void *)0)

/* --- OpenGL ES 2.0 Types and Constants --- */
typedef void GLvoid;
typedef char GLchar;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int8_t GLbyte;
typedef int16_t GLshort;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef uint8_t GLubyte;
typedef uint16_t GLushort;
typedef uint32_t GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef int32_t GLfixed;

#define GL_FALSE                        0
#define GL_TRUE                         1
#define GL_NO_ERROR                     0

#define GL_VENDOR                       0x1F00
#define GL_RENDERER                     0x1F01
#define GL_VERSION                      0x1F02
#define GL_EXTENSIONS                   0x1F03
#define GL_SHADING_LANGUAGE_VERSION     0x8B8C

#define GL_MAX_TEXTURE_SIZE             0x0D33
#define GL_MAX_VIEWPORT_DIMS            0x0D3A
#define GL_MAX_VERTEX_ATTRIBS           0x8869
#define GL_MAX_VERTEX_UNIFORM_VECTORS   0x8DFB
#define GL_MAX_VARYING_VECTORS          0x8DFC
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_TEXTURE_IMAGE_UNITS      0x8872
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS 0x8DFD
#define GL_MAX_RENDERBUFFER_SIZE        0x84E8

#define GL_VERTEX_SHADER                0x8B31
#define GL_FRAGMENT_SHADER              0x8B30
#define GL_COMPILE_STATUS               0x8B81
#define GL_LINK_STATUS                  0x8B82
#define GL_INFO_LOG_LENGTH              0x8B84
#define GL_SHADER_SOURCE_LENGTH         0x8B88
#define GL_ACTIVE_UNIFORMS              0x8B86
#define GL_ACTIVE_ATTRIBUTES            0x8B89

#define GL_FRAMEBUFFER_COMPLETE         0x8CD5

#define GL_RGBA                         0x1908
#define GL_RGB                          0x1907
#define GL_UNSIGNED_BYTE                0x1401

/* --- State Tracking Structures --- */
typedef struct {
    int id;
    int client_version;
} SwContext;

typedef struct SwSurface {
    int id;
    int is_window;
    int width;
    int height;
    uint32_t *pixels;
    void *native_window;
    int refcount;
    int delete_pending;
} SwSurface;

typedef struct {
    GLuint id;
    GLenum type;
    GLboolean compiled;
    char *source;
    char info_log[1024];
    GLboolean has_color;
    float color[4];
} SwShader;

typedef struct {
    char name[32];
    GLuint index;
} SwAttribBinding;

typedef struct {
    GLuint id;
    GLboolean linked;
    GLuint vertex_shader;
    GLuint fragment_shader;
    SwAttribBinding attrib_bindings[8];
    int num_attrib_bindings;
    GLboolean has_color;
    float color[4];
} SwProgram;

/* Vertex Attribute State */
typedef struct {
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const void *pointer;
    GLboolean enabled;
} SwVertexAttrib;

/* Texture State */
typedef struct {
    GLsizei width;
    GLsizei height;
    GLenum format;
    uint32_t *pixels;
} SwTexture;

typedef struct {
    float x;
    float y;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
} SwVertex;

static EGLint g_last_error = EGL_SUCCESS;
static SwContext *g_current_context = NULL;
static SwSurface *g_current_draw_surface = NULL;
static SwSurface *g_current_read_surface = NULL;

static SwShader g_shaders[64];
static int g_num_shaders = 0;

static SwProgram g_programs[32];
static int g_num_programs = 0;
static GLuint g_current_program = 0;

static SwVertexAttrib g_vertex_attribs[4];
static SwTexture g_textures[64];
static GLuint g_bound_texture = 0;

static float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static float g_uniform_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static GLint g_uniform_sampler = 0;
static GLint g_viewport[4] = {0, 0, 1920, 1080};
static GLboolean g_blend_enabled = GL_FALSE;
static GLint g_pack_alignment = 4;
static GLint g_unpack_alignment = 4;
static int g_fb_fd = -1;
static void *g_fb_map = NULL;
static size_t g_fb_map_size = 0;

#if defined(__arm__) && !defined(__aarch64__)
static inline void *sys_mmap2(void *addr, unsigned long length, long prot,
                             long flags, long fd, unsigned long pgoff) {
    register long r7 __asm__("r7") = 192; /* __NR_mmap2 */
    register long r0 __asm__("r0") = (long)addr;
    register long r1 __asm__("r1") = (long)length;
    register long r2 __asm__("r2") = prot;
    register long r3 __asm__("r3") = flags;
    register long r4 __asm__("r4") = fd;
    register long r5 __asm__("r5") = (long)pgoff;
    __asm__ volatile("swi #0" : "+r"(r0)
                     : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
                     : "memory");
    return (void *)r0;
}

static inline long sys_munmap(void *addr, unsigned long length) {
    register long r7 __asm__("r7") = 91; /* __NR_munmap */
    register long r0 __asm__("r0") = (long)addr;
    register long r1 __asm__("r1") = (long)length;
    __asm__ volatile("swi #0" : "+r"(r0) : "r"(r7), "r"(r1) : "memory");
    return r0;
}
#else
#include <sys/mman.h>
static inline void *sys_mmap2(void *addr, unsigned long length, long prot,
                             long flags, long fd, unsigned long pgoff) {
    return mmap(addr, length, (int)prot, (int)flags, (int)fd, (off_t)pgoff * 4096);
}
static inline long sys_munmap(void *addr, unsigned long length) {
    return (long)munmap(addr, (size_t)length);
}
#endif

/* --- EGL 1.4 API Implementations --- */

__attribute__((visibility("default")))
EGLint eglGetError(void) {
    EGLint err = g_last_error;
    g_last_error = EGL_SUCCESS;
    return err;
}

__attribute__((visibility("default")))
EGLDisplay eglGetDisplay(void *display_id) {
    return (EGLDisplay)1;
}

__attribute__((visibility("default")))
EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
    if (major) *major = 1;
    if (minor) *minor = 4;
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLBoolean eglTerminate(EGLDisplay dpy) {
    if (g_fb_map) {
        sys_munmap(g_fb_map, g_fb_map_size);
        g_fb_map = NULL;
        g_fb_map_size = 0;
    }
    if (g_fb_fd >= 0) {
        close(g_fb_fd);
        g_fb_fd = -1;
    }
    return EGL_TRUE;
}

__attribute__((visibility("default")))
const char *eglQueryString(EGLDisplay dpy, EGLint name) {
    switch (name) {
        case EGL_VENDOR: return "Google Inc. (OpenDTV Software Compatibility)";
        case EGL_VERSION: return "1.4 Android META-EGL";
        case EGL_EXTENSIONS:
            return "EGL_KHR_image_base EGL_KHR_gl_texture_2D_image EGL_ANDROID_image_native_buffer EGL_KHR_fence_sync EGL_ANDROID_native_fence_sync";
        case EGL_CLIENT_APIS: return "OpenGL_ES";
        default: return "";
    }
}

__attribute__((visibility("default")))
const char *eglQueryStringImplementationANDROID(EGLDisplay dpy, EGLint name) {
    return eglQueryString(dpy, name);
}

__attribute__((visibility("default")))
EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    if (!num_config) {
        g_last_error = EGL_BAD_PARAMETER;
        return EGL_FALSE;
    }
    if (!configs || config_size == 0) {
        *num_config = 1;
        return EGL_TRUE;
    }
    configs[0] = (EGLConfig)1;
    *num_config = 1;
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                           EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    if (!num_config) {
        g_last_error = EGL_BAD_PARAMETER;
        return EGL_FALSE;
    }
    if (!configs || config_size == 0) {
        *num_config = 1;
        return EGL_TRUE;
    }
    configs[0] = (EGLConfig)1;
    *num_config = 1;
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) {
    if (!value) {
        g_last_error = EGL_BAD_PARAMETER;
        return EGL_FALSE;
    }
    switch (attribute) {
        case EGL_BUFFER_SIZE:             *value = 32; break;
        case EGL_RED_SIZE:                *value = 8; break;
        case EGL_GREEN_SIZE:              *value = 8; break;
        case EGL_BLUE_SIZE:               *value = 8; break;
        case EGL_ALPHA_SIZE:              *value = 8; break;
        case EGL_DEPTH_SIZE:              *value = 24; break;
        case EGL_STENCIL_SIZE:            *value = 8; break;
        case EGL_CONFIG_CAVEAT:           *value = EGL_NONE; break;
        case EGL_CONFIG_ID:               *value = 1; break;
        case EGL_LEVEL:                   *value = 0; break;
        case EGL_MAX_PBUFFER_WIDTH:       *value = 4096; break;
        case EGL_MAX_PBUFFER_HEIGHT:      *value = 4096; break;
        case EGL_MAX_PBUFFER_PIXELS:      *value = 4096 * 4096; break;
        case EGL_NATIVE_RENDERABLE:       *value = EGL_FALSE; break;
        case EGL_NATIVE_VISUAL_ID:        *value = 1; break; /* HAL_PIXEL_FORMAT_RGBA_8888 */
        case EGL_NATIVE_VISUAL_TYPE:      *value = 1; break;
        case EGL_SAMPLES:                 *value = 0; break;
        case EGL_SAMPLE_BUFFERS:          *value = 0; break;
        case EGL_SURFACE_TYPE:            *value = (EGL_WINDOW_BIT | EGL_PBUFFER_BIT); break;
        case EGL_TRANSPARENT_TYPE:        *value = EGL_NONE; break;
        case EGL_BIND_TO_TEXTURE_RGB:     *value = EGL_TRUE; break;
        case EGL_BIND_TO_TEXTURE_RGBA:    *value = EGL_TRUE; break;
        case EGL_MIN_SWAP_INTERVAL:       *value = 1; break;
        case EGL_MAX_SWAP_INTERVAL:       *value = 1; break;
        case EGL_LUMINANCE_SIZE:          *value = 0; break;
        case EGL_ALPHA_MASK_SIZE:         *value = 0; break;
        case EGL_COLOR_BUFFER_TYPE:       *value = EGL_RGB_BUFFER; break;
        case EGL_RENDERABLE_TYPE:         *value = (EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT); break;
        case EGL_CONFORMANT:              *value = (EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT); break;
        default:                          *value = 0; break;
    }
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list) {
    int client_version = 1;
    if (attrib_list) {
        for (const EGLint *p = attrib_list; *p != EGL_NONE; p += 2) {
            if (*p == EGL_CONTEXT_CLIENT_VERSION) {
                client_version = *(p + 1);
            }
        }
    }
    SwContext *ctx = (SwContext *)calloc(1, sizeof(SwContext));
    if (!ctx) {
        g_last_error = EGL_BAD_ALLOC;
        return EGL_NO_CONTEXT;
    }
    ctx->id = 1;
    ctx->client_version = client_version;
    return (EGLContext)ctx;
}

static void surface_retain(SwSurface *s) {
    if (s) {
        s->refcount++;
    }
}

static void surface_release(SwSurface *s) {
    if (!s) return;
    s->refcount--;
    if (s->refcount <= 0) {
        if (s == g_current_draw_surface) g_current_draw_surface = NULL;
        if (s == g_current_read_surface) g_current_read_surface = NULL;
        if (s->pixels) {
            free(s->pixels);
            s->pixels = NULL;
        }
        free(s);
    }
}

__attribute__((visibility("default")))
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    if (ctx && ctx != EGL_NO_CONTEXT) {
        if (g_current_context == (SwContext *)ctx) {
            if (g_current_draw_surface) {
                surface_release(g_current_draw_surface);
                g_current_draw_surface = NULL;
            }
            if (g_current_read_surface) {
                surface_release(g_current_read_surface);
                g_current_read_surface = NULL;
            }
            g_current_context = NULL;
        }
        free(ctx);
    }
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) {
    int w = 1, h = 1;
    if (attrib_list) {
        for (const EGLint *p = attrib_list; *p != EGL_NONE; p += 2) {
            if (*p == EGL_WIDTH) w = *(p + 1);
            if (*p == EGL_HEIGHT) h = *(p + 1);
        }
    }
    SwSurface *s = (SwSurface *)calloc(1, sizeof(SwSurface));
    if (!s) {
        g_last_error = EGL_BAD_ALLOC;
        return EGL_NO_SURFACE;
    }
    s->id = 1;
    s->is_window = 0;
    s->width = (w > 0) ? w : 1;
    s->height = (h > 0) ? h : 1;
    s->pixels = (uint32_t *)calloc(s->width * s->height, sizeof(uint32_t));
    s->refcount = 1;
    s->delete_pending = 0;
    return (EGLSurface)s;
}

__attribute__((visibility("default")))
EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, void *win, const EGLint *attrib_list) {
    SwSurface *s = (SwSurface *)calloc(1, sizeof(SwSurface));
    if (!s) {
        g_last_error = EGL_BAD_ALLOC;
        return EGL_NO_SURFACE;
    }
    s->id = 2;
    s->is_window = 1;
    s->width = 1920;
    s->height = 1080;
    s->native_window = win;
    s->pixels = (uint32_t *)calloc(s->width * s->height, sizeof(uint32_t));
    if (s->pixels) {
        for (size_t i = 0; i < (size_t)s->width * s->height; i++) {
            s->pixels[i] = 0xFF141824;
        }
    }
    s->refcount = 1;
    s->delete_pending = 0;
    return (EGLSurface)s;
}

__attribute__((visibility("default")))
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    if (!surface || surface == EGL_NO_SURFACE) {
        g_last_error = EGL_BAD_SURFACE;
        return EGL_FALSE;
    }
    SwSurface *s = (SwSurface *)surface;
    s->delete_pending = 1;
    surface_release(s);
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    SwSurface *new_draw = (SwSurface *)draw;
    SwSurface *new_read = (SwSurface *)read;

    if (new_draw) surface_retain(new_draw);
    if (new_read) surface_retain(new_read);

    if (g_current_draw_surface) surface_release(g_current_draw_surface);
    if (g_current_read_surface) surface_release(g_current_read_surface);

    g_current_context = (SwContext *)ctx;
    g_current_draw_surface = new_draw;
    g_current_read_surface = new_read;
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLContext eglGetCurrentContext(void) {
    return (EGLContext)g_current_context;
}

__attribute__((visibility("default")))
EGLSurface eglGetCurrentSurface(EGLint readdraw) {
    return (readdraw == EGL_READ) ? (EGLSurface)g_current_read_surface : (EGLSurface)g_current_draw_surface;
}

__attribute__((visibility("default")))
EGLDisplay eglGetCurrentDisplay(void) {
    return (EGLDisplay)1;
}

__attribute__((visibility("default")))
EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value) {
    if (!surface || !value) return EGL_FALSE;
    SwSurface *s = (SwSurface *)surface;
    switch (attribute) {
        case EGL_WIDTH: *value = s->width; break;
        case EGL_HEIGHT: *value = s->height; break;
        case EGL_CONFIG_ID: *value = 1; break;
        default: *value = 0; break;
    }
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (!surface || surface == EGL_NO_SURFACE) {
        g_last_error = EGL_BAD_SURFACE;
        return EGL_FALSE;
    }
    SwSurface *s = (SwSurface *)surface;
    if (!s->is_window) {
        return EGL_TRUE;
    }
    if (!s->pixels) {
        g_last_error = EGL_BAD_NATIVE_WINDOW;
        return EGL_FALSE;
    }
    if (g_fb_fd < 0) {
        g_fb_fd = open("/dev/graphics/fb0", 2 /* O_RDWR */);
        if (g_fb_fd < 0) {
            g_fb_fd = open("/dev/fb0", 2 /* O_RDWR */);
        }
    }
    if (g_fb_fd < 0) {
        g_last_error = EGL_BAD_NATIVE_WINDOW;
        return EGL_FALSE;
    }
    if (!g_fb_map) {
        g_fb_map_size = 1920 * 1080 * 4;
        g_fb_map = sys_mmap2(NULL, g_fb_map_size, 3 /* PROT_READ | PROT_WRITE */,
                             1 /* MAP_SHARED */, g_fb_fd, 0);
        if ((unsigned long)g_fb_map >= 0xFFFFF000UL) {
            g_fb_map = NULL;
            g_last_error = EGL_BAD_ALLOC;
            return EGL_FALSE;
        }
    }

    size_t copy_bytes = (size_t)s->width * (size_t)s->height * 4;
    if (copy_bytes > g_fb_map_size) copy_bytes = g_fb_map_size;
    memcpy(g_fb_map, s->pixels, copy_bytes);
    __asm__ volatile("dmb sy" ::: "memory");
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval) {
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list) {
    return (EGLImageKHR)buffer;
}

__attribute__((visibility("default")))
EGLBoolean eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image) {
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLSyncKHR eglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list) {
    return (EGLSyncKHR)1;
}

__attribute__((visibility("default")))
EGLBoolean eglDestroySyncKHR(EGLDisplay dpy, EGLSyncKHR sync) {
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLint eglClientWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, uint64_t timeout) {
    return 0x309C; /* EGL_CONDITION_SATISFIED_KHR */
}

__attribute__((visibility("default")))
EGLBoolean eglWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags) {
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLint eglDupNativeFenceFDANDROID(EGLDisplay dpy, EGLSyncKHR sync) {
    return -1; /* No fence fd needed for software synchronization */
}


/* --- OpenGL ES 2.0 API Implementations --- */

__attribute__((visibility("default")))
const GLubyte *glGetString(GLenum name) {
    switch (name) {
        case GL_VENDOR: return (const GLubyte *)"Google Inc. (OpenDTV Software Rasterizer)";
        case GL_RENDERER: return (const GLubyte *)"OpenDTV ARM32 Software Rasterizer";
        case GL_VERSION: return (const GLubyte *)"OpenGL ES 2.0 (OpenDTV Compatibility)";
        case GL_EXTENSIONS:
            return (const GLubyte *)"GL_OES_EGL_image GL_OES_EGL_image_external GL_OES_rgb8_rgba8 GL_OES_depth24 GL_OES_texture_npot GL_EXT_texture_format_BGRA8888";
        case GL_SHADING_LANGUAGE_VERSION:
            return (const GLubyte *)"OpenGL ES GLSL ES 1.00";
        default: return (const GLubyte *)"";
    }
}

__attribute__((visibility("default")))
GLenum glGetError(void) {
    return GL_NO_ERROR;
}

__attribute__((visibility("default")))
void glGetIntegerv(GLenum pname, GLint *params) {
    if (!params) return;
    switch (pname) {
        case GL_MAX_TEXTURE_SIZE: *params = 4096; break;
        case GL_MAX_VIEWPORT_DIMS: params[0] = 4096; params[1] = 4096; break;
        case GL_MAX_VERTEX_ATTRIBS: *params = 16; break;
        case GL_MAX_VERTEX_UNIFORM_VECTORS: *params = 128; break;
        case GL_MAX_VARYING_VECTORS: *params = 8; break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *params = 16; break;
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: *params = 16; break;
        case GL_MAX_TEXTURE_IMAGE_UNITS: *params = 16; break;
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS: *params = 128; break;
        case GL_MAX_RENDERBUFFER_SIZE: *params = 4096; break;
        default: *params = 0; break;
    }
}

static float parse_float_val(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
    float sign = 1.0f;
    if (**p == '-') { sign = -1.0f; (*p)++; }
    else if (**p == '+') { (*p)++; }
    float val = 0.0f;
    while (**p >= '0' && **p <= '9') {
        val = val * 10.0f + (float)(**p - '0');
        (*p)++;
    }
    if (**p == '.') {
        (*p)++;
        float frac = 0.1f;
        while (**p >= '0' && **p <= '9') {
            val += (float)(**p - '0') * frac;
            frac *= 0.1f;
            (*p)++;
        }
    }
    if (**p == 'f' || **p == 'F') (*p)++;
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
    return sign * val;
}

static void parse_fragment_color(const char *src, float *color, GLboolean *has_color) {
    *has_color = GL_FALSE;
    color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f; color[3] = 1.0f;
    if (!src) return;
    const char *fc = strstr(src, "gl_FragColor");
    if (!fc) return;
    const char *v4 = strstr(fc, "vec4");
    if (!v4) return;
    const char *op = strchr(v4, '(');
    if (!op) return;
    const char *p = op + 1;
    color[0] = parse_float_val(&p);
    if (*p == ',') p++;
    color[1] = parse_float_val(&p);
    if (*p == ',') p++;
    color[2] = parse_float_val(&p);
    if (*p == ',') p++;
    color[3] = parse_float_val(&p);
    *has_color = GL_TRUE;
}

/* The frontend performs grammar, name and type checking. Linking requires
 * the pinned compiler archive: there is no heuristic fallback. */
extern int sw_validate_shader(unsigned int type, const char *source,
                              char *log, size_t log_size);

__attribute__((visibility("default")))
GLuint glCreateShader(GLenum type) {
    if (g_num_shaders >= 63) return 0;
    GLuint id = (GLuint)(++g_num_shaders);
    g_shaders[id].id = id;
    g_shaders[id].type = type;
    g_shaders[id].compiled = GL_FALSE;
    g_shaders[id].source = NULL;
    g_shaders[id].info_log[0] = '\0';
    g_shaders[id].has_color = GL_FALSE;
    g_shaders[id].color[0] = 1.0f;
    g_shaders[id].color[1] = 1.0f;
    g_shaders[id].color[2] = 1.0f;
    g_shaders[id].color[3] = 1.0f;
    return id;
}

__attribute__((visibility("default")))
void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) {
    if (shader > 0 && shader < 64 && string && count > 0) {
        if (g_shaders[shader].source) {
            free(g_shaders[shader].source);
            g_shaders[shader].source = NULL;
        }
        size_t total_len = 0;
        for (int i = 0; i < count; i++) {
            if (string[i]) {
                if (length && length[i] >= 0) {
                    total_len += (size_t)length[i];
                } else {
                    size_t l = 0;
                    while (string[i][l]) l++;
                    total_len += l;
                }
            }
        }
        char *src = (char *)calloc(total_len + 1, 1);
        if (src) {
            size_t off = 0;
            for (int i = 0; i < count; i++) {
                if (string[i]) {
                    size_t l = (length && length[i] >= 0) ? (size_t)length[i] : 0;
                    if (!length || length[i] < 0) {
                        while (string[i][l]) l++;
                    }
                    for (size_t j = 0; j < l; j++) src[off++] = string[i][j];
                }
            }
            src[off] = '\0';
            g_shaders[shader].source = src;
        }
    }
}

__attribute__((visibility("default")))
void glCompileShader(GLuint shader) {
    if (shader > 0 && shader < 64) {
        const char *src = g_shaders[shader].source;
        if (sw_validate_shader(g_shaders[shader].type, src,
                               g_shaders[shader].info_log,
                               sizeof(g_shaders[shader].info_log))) {
            g_shaders[shader].compiled = GL_TRUE;
            if (g_shaders[shader].type == 0x8B30 /* GL_FRAGMENT_SHADER */) {
                parse_fragment_color(src, g_shaders[shader].color, &g_shaders[shader].has_color);
            }
        } else {
            g_shaders[shader].compiled = GL_FALSE;
            g_shaders[shader].has_color = GL_FALSE;
        }
    }
}

__attribute__((visibility("default")))
void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    if (!params) return;
    switch (pname) {
        case GL_COMPILE_STATUS:
            *params = (shader > 0 && shader < 64) ? g_shaders[shader].compiled : GL_FALSE;
            break;
        case GL_INFO_LOG_LENGTH: {
            size_t n = 0;
            if (shader > 0 && shader < 64) {
                while (g_shaders[shader].info_log[n]) n++;
            }
            *params = n ? (GLint)(n + 1) : 0;
            break;
        }
        case GL_SHADER_SOURCE_LENGTH: {
            if (shader > 0 && shader < 64 && g_shaders[shader].source) {
                size_t l = 0;
                while (g_shaders[shader].source[l]) l++;
                *params = (GLint)(l + 1);
            } else {
                *params = 0;
            }
            break;
        }
        default: *params = 0; break;
    }
}

__attribute__((visibility("default")))
void glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog) {
    GLsizei copied = 0;
    if (infolog && bufsize > 0) {
        if (shader > 0 && shader < 64) {
            while (copied < bufsize - 1 && g_shaders[shader].info_log[copied]) {
                infolog[copied] = g_shaders[shader].info_log[copied];
                copied++;
            }
        }
        infolog[copied] = '\0';
    }
    if (length) *length = copied;
}

__attribute__((visibility("default")))
void glDeleteShader(GLuint shader) {
    if (shader > 0 && shader < 64) {
        if (g_shaders[shader].source) {
            free(g_shaders[shader].source);
            g_shaders[shader].source = NULL;
        }
        g_shaders[shader].compiled = GL_FALSE;
    }
}

__attribute__((visibility("default")))
GLuint glCreateProgram(void) {
    if (g_num_programs >= 31) return 0;
    GLuint id = (GLuint)(++g_num_programs);
    g_programs[id].id = id;
    g_programs[id].linked = GL_FALSE;
    g_programs[id].vertex_shader = 0;
    g_programs[id].fragment_shader = 0;
    g_programs[id].num_attrib_bindings = 0;
    g_programs[id].has_color = GL_FALSE;
    g_programs[id].color[0] = 1.0f;
    g_programs[id].color[1] = 1.0f;
    g_programs[id].color[2] = 1.0f;
    g_programs[id].color[3] = 1.0f;
    return id;
}

__attribute__((visibility("default")))
void glAttachShader(GLuint program, GLuint shader) {
    if (program > 0 && program < 32 && shader > 0 && shader < 64) {
        if (g_shaders[shader].type == GL_VERTEX_SHADER) {
            g_programs[program].vertex_shader = shader;
        } else {
            g_programs[program].fragment_shader = shader;
        }
    }
}

__attribute__((visibility("default")))
void glDetachShader(GLuint program, GLuint shader) {
    if (program > 0 && program < 32) {
        if (g_programs[program].vertex_shader == shader) g_programs[program].vertex_shader = 0;
        if (g_programs[program].fragment_shader == shader) g_programs[program].fragment_shader = 0;
    }
}

__attribute__((visibility("default")))
void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
    if (program > 0 && program < 32 && name && index < 4) {
        SwProgram *p = &g_programs[program];
        for (int i = 0; i < p->num_attrib_bindings; i++) {
            if (strcmp(p->attrib_bindings[i].name, name) == 0) {
                p->attrib_bindings[i].index = index;
                return;
            }
        }
        if (p->num_attrib_bindings < 8) {
            int k = p->num_attrib_bindings++;
            size_t len = 0;
            while (name[len] && len < 31) {
                p->attrib_bindings[k].name[len] = name[len];
                len++;
            }
            p->attrib_bindings[k].name[len] = '\0';
            p->attrib_bindings[k].index = index;
        }
    }
}

__attribute__((visibility("default")))
void glLinkProgram(GLuint program) {
    if (program > 0 && program < 32) {
        GLuint vs = g_programs[program].vertex_shader;
        GLuint fs = g_programs[program].fragment_shader;
        if (vs > 0 && vs < 64 && fs > 0 && fs < 64 &&
            g_shaders[vs].compiled == GL_TRUE && g_shaders[fs].compiled == GL_TRUE) {
            g_programs[program].linked = GL_TRUE;
            if (g_shaders[fs].has_color) {
                g_programs[program].has_color = GL_TRUE;
                for (int c = 0; c < 4; c++) {
                    g_programs[program].color[c] = g_shaders[fs].color[c];
                }
            }
        } else {
            g_programs[program].linked = GL_FALSE;
        }
    }
}

__attribute__((visibility("default")))
void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    if (!params) return;
    switch (pname) {
        case GL_LINK_STATUS:
            *params = (program > 0 && program < 32) ? g_programs[program].linked : GL_FALSE;
            break;
        case GL_INFO_LOG_LENGTH: *params = 0; break;
        case GL_ACTIVE_ATTRIBUTES: *params = 2; break;
        case GL_ACTIVE_UNIFORMS: *params = 4; break;
        default: *params = 0; break;
    }
}

__attribute__((visibility("default")))
void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog) {
    if (length) *length = 0;
    if (infolog && bufsize > 0) infolog[0] = '\0';
}

__attribute__((visibility("default")))
void glUseProgram(GLuint program) {
    g_current_program = program;
    if (program > 0 && program < 32 && g_programs[program].has_color) {
        for (int c = 0; c < 4; c++) {
            g_uniform_color[c] = g_programs[program].color[c];
        }
    }
}

__attribute__((visibility("default")))
void glDeleteProgram(GLuint program) {
    if (program > 0 && program < 32) {
        g_programs[program].linked = GL_FALSE;
        g_programs[program].vertex_shader = 0;
        g_programs[program].fragment_shader = 0;
        g_programs[program].num_attrib_bindings = 0;
    }
}

__attribute__((visibility("default")))
GLint glGetAttribLocation(GLuint program, const GLchar *name) {
    if (!name) return -1;
    if (program > 0 && program < 32) {
        SwProgram *p = &g_programs[program];
        for (int i = 0; i < p->num_attrib_bindings; i++) {
            if (strcmp(p->attrib_bindings[i].name, name) == 0) {
                return (GLint)p->attrib_bindings[i].index;
            }
        }
    }
    if (strstr(name, "pos") || strstr(name, "Pos") || strstr(name, "position") || strstr(name, "Position") || strstr(name, "vPosition")) return 0;
    if (strstr(name, "tex") || strstr(name, "Tex") || strstr(name, "coord") || strstr(name, "Coord")) return 1;
    if (strstr(name, "col") || strstr(name, "Col") || strstr(name, "color") || strstr(name, "Color")) return 2;
    return 0;
}

__attribute__((visibility("default")))
GLint glGetUniformLocation(GLuint program, const GLchar *name) {
    if (!name) return -1;
    if (strstr(name, "color") || strstr(name, "Color") || strstr(name, "u_color") || strstr(name, "uColor")) return 0;
    if (strstr(name, "tex") || strstr(name, "Tex") || strstr(name, "sampler") || strstr(name, "u_texture")) return 1;
    uint32_t h = 0;
    for (const char *p = name; *p; p++) h = (h * 31) + *p;
    return (GLint)(h % 16);
}

__attribute__((visibility("default")))
void glUniform1f(GLint location, GLfloat v0) {}

__attribute__((visibility("default")))
void glUniform1i(GLint location, GLint v0) {
    g_uniform_sampler = v0;
}

__attribute__((visibility("default")))
void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    if (value && count >= 1) {
        g_uniform_color[0] = value[0];
        g_uniform_color[1] = value[1];
        g_uniform_color[2] = value[2];
        g_uniform_color[3] = value[3];
    }
}

__attribute__((visibility("default")))
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    g_uniform_color[0] = v0;
    g_uniform_color[1] = v1;
    g_uniform_color[2] = v2;
    g_uniform_color[3] = v3;
}

__attribute__((visibility("default")))
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {}

__attribute__((visibility("default")))
void glEnableVertexAttribArray(GLuint index) {
    if (index < 4) {
        g_vertex_attribs[index].enabled = GL_TRUE;
    }
}

__attribute__((visibility("default")))
void glDisableVertexAttribArray(GLuint index) {
    if (index < 4) {
        g_vertex_attribs[index].enabled = GL_FALSE;
    }
}

__attribute__((visibility("default")))
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    if (index < 4) {
        g_vertex_attribs[index].size = size;
        g_vertex_attribs[index].type = type;
        g_vertex_attribs[index].normalized = normalized;
        g_vertex_attribs[index].stride = stride;
        g_vertex_attribs[index].pointer = pointer;
    }
}

static GLuint g_next_texture_id = 1;

__attribute__((visibility("default")))
void glGenTextures(GLsizei n, GLuint *textures) {
    if (!textures || n <= 0) return;
    for (GLsizei i = 0; i < n; i++) {
        GLuint id = 0;
        for (GLuint try_id = 1; try_id < 64; try_id++) {
            GLuint candidate = ((g_next_texture_id - 1 + try_id) % 63) + 1;
            if (!g_textures[candidate].pixels && g_textures[candidate].width == 0) {
                id = candidate;
                g_next_texture_id = (candidate % 63) + 1;
                break;
            }
        }
        if (id == 0) id = (GLuint)(i + 1);
        textures[i] = id;
    }
}

__attribute__((visibility("default")))
void glBindTexture(GLenum target, GLuint texture) {
    if (texture < 64) {
        g_bound_texture = texture;
    }
}

__attribute__((visibility("default")))
void glTexParameteri(GLenum target, GLenum pname, GLint param) {}

__attribute__((visibility("default")))
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                 GLint border, GLenum format, GLenum type, const void *pixels) {
    if (width <= 0 || height <= 0) return;
    GLuint id = g_bound_texture;
    if (id == 0 || id >= 64) return;

    if (g_textures[id].pixels) {
        free(g_textures[id].pixels);
        g_textures[id].pixels = NULL;
    }

    size_t num_pixels = (size_t)width * (size_t)height;
    uint32_t *tex_data = (uint32_t *)calloc(num_pixels, sizeof(uint32_t));
    if (!tex_data) return;

    g_textures[id].width = width;
    g_textures[id].height = height;
    g_textures[id].format = format;
    g_textures[id].pixels = tex_data;

    if (!pixels) return;

    const uint8_t *src_bytes = (const uint8_t *)pixels;
    int align = (g_unpack_alignment == 1 || g_unpack_alignment == 2 || g_unpack_alignment == 4 || g_unpack_alignment == 8) ? g_unpack_alignment : 4;

    if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
        size_t row_bytes = (size_t)width * 4;
        size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
        for (GLsizei y = 0; y < height; y++) {
            const uint8_t *row_src = src_bytes + (size_t)y * row_stride;
            for (GLsizei x = 0; x < width; x++) {
                uint8_t r = row_src[x * 4 + 0];
                uint8_t g = row_src[x * 4 + 1];
                uint8_t b = row_src[x * 4 + 2];
                uint8_t a = row_src[x * 4 + 3];
                tex_data[(size_t)y * (size_t)width + (size_t)x] =
                    ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    } else if (format == GL_RGB && type == GL_UNSIGNED_BYTE) {
        size_t row_bytes = (size_t)width * 3;
        size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
        for (GLsizei y = 0; y < height; y++) {
            const uint8_t *row_src = src_bytes + (size_t)y * row_stride;
            for (GLsizei x = 0; x < width; x++) {
                uint8_t r = row_src[x * 3 + 0];
                uint8_t g = row_src[x * 3 + 1];
                uint8_t b = row_src[x * 3 + 2];
                tex_data[(size_t)y * (size_t)width + (size_t)x] =
                    (0xFF000000U) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    } else if (format == 0x80E1 /* GL_BGRA_EXT */ && type == GL_UNSIGNED_BYTE) {
        size_t row_bytes = (size_t)width * 4;
        size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
        for (GLsizei y = 0; y < height; y++) {
            const uint8_t *row_src = src_bytes + (size_t)y * row_stride;
            for (GLsizei x = 0; x < width; x++) {
                uint8_t b = row_src[x * 4 + 0];
                uint8_t g = row_src[x * 4 + 1];
                uint8_t r = row_src[x * 4 + 2];
                uint8_t a = row_src[x * 4 + 3];
                tex_data[(size_t)y * (size_t)width + (size_t)x] =
                    ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    } else if (format == 0x1909 /* GL_LUMINANCE */ && type == GL_UNSIGNED_BYTE) {
        size_t row_bytes = (size_t)width;
        size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
        for (GLsizei y = 0; y < height; y++) {
            const uint8_t *row_src = src_bytes + (size_t)y * row_stride;
            for (GLsizei x = 0; x < width; x++) {
                uint8_t l = row_src[x];
                tex_data[(size_t)y * (size_t)width + (size_t)x] =
                    (0xFF000000U) | ((uint32_t)l << 16) | ((uint32_t)l << 8) | (uint32_t)l;
            }
        }
    } else if (format == 0x190A /* GL_LUMINANCE_ALPHA */ && type == GL_UNSIGNED_BYTE) {
        size_t row_bytes = (size_t)width * 2;
        size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
        for (GLsizei y = 0; y < height; y++) {
            const uint8_t *row_src = src_bytes + (size_t)y * row_stride;
            for (GLsizei x = 0; x < width; x++) {
                uint8_t l = row_src[x * 2 + 0];
                uint8_t a = row_src[x * 2 + 1];
                tex_data[(size_t)y * (size_t)width + (size_t)x] =
                    ((uint32_t)a << 24) | ((uint32_t)l << 16) | ((uint32_t)l << 8) | (uint32_t)l;
            }
        }
    } else {
        for (size_t i = 0; i < num_pixels; i++) {
            tex_data[i] = ((const uint32_t *)pixels)[i];
        }
    }
}

__attribute__((visibility("default")))
void glDeleteTextures(GLsizei n, const GLuint *textures) {
    if (!textures || n <= 0) return;
    for (GLsizei i = 0; i < n; i++) {
        GLuint id = textures[i];
        if (id > 0 && id < 64) {
            if (g_textures[id].pixels) {
                free(g_textures[id].pixels);
                g_textures[id].pixels = NULL;
            }
            g_textures[id].width = 0;
            g_textures[id].height = 0;
            g_textures[id].format = 0;
            if (g_bound_texture == id) g_bound_texture = 0;
        }
    }
}

__attribute__((visibility("default")))
void glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    for (int i = 0; i < n; i++) framebuffers[i] = (GLuint)(i + 1);
}

__attribute__((visibility("default")))
void glBindFramebuffer(GLenum target, GLuint framebuffer) {}

__attribute__((visibility("default")))
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {}

__attribute__((visibility("default")))
GLenum glCheckFramebufferStatus(GLenum target) {
    return GL_FRAMEBUFFER_COMPLETE;
}

__attribute__((visibility("default")))
void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {}

__attribute__((visibility("default")))
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    g_viewport[0] = x;
    g_viewport[1] = y;
    g_viewport[2] = width;
    g_viewport[3] = height;
}

__attribute__((visibility("default")))
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {}

__attribute__((visibility("default")))
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    g_clear_color[0] = red;
    g_clear_color[1] = green;
    g_clear_color[2] = blue;
    g_clear_color[3] = alpha;
}

__attribute__((visibility("default")))
void glClear(GLbitfield mask) {
    if (g_current_draw_surface && g_current_draw_surface->pixels) {
        uint8_t r = (uint8_t)(g_clear_color[0] * 255.0f);
        uint8_t g = (uint8_t)(g_clear_color[1] * 255.0f);
        uint8_t b = (uint8_t)(g_clear_color[2] * 255.0f);
        uint8_t a = (uint8_t)(g_clear_color[3] * 255.0f);
        uint32_t val = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        int total = g_current_draw_surface->width * g_current_draw_surface->height;
        for (int i = 0; i < total; i++) {
            g_current_draw_surface->pixels[i] = val;
        }
    }
}

__attribute__((visibility("default")))
void glEnable(GLenum cap) {
    if (cap == 0x0BE2 /* GL_BLEND */) {
        g_blend_enabled = GL_TRUE;
    }
}

__attribute__((visibility("default")))
void glDisable(GLenum cap) {
    if (cap == 0x0BE2 /* GL_BLEND */) {
        g_blend_enabled = GL_FALSE;
    }
}

__attribute__((visibility("default")))
void glBlendFunc(GLenum sfactor, GLenum dfactor) {}

static void get_attrib_float(int index, int vertex_idx, float *out, int count, float default_val) {
    for (int k = 0; k < count; k++) out[k] = default_val;
    if (index < 0 || index >= 4 || !g_vertex_attribs[index].enabled || !g_vertex_attribs[index].pointer) {
        return;
    }
    SwVertexAttrib *va = &g_vertex_attribs[index];
    int size = va->size > 0 ? va->size : 4;
    int comp_count = size < count ? size : count;
    int comp_size = 4;
    if (va->type == 0x1401 /* GL_UNSIGNED_BYTE */ || va->type == 0x1400 /* GL_BYTE */) {
        comp_size = 1;
    } else if (va->type == 0x1402 /* GL_SHORT */ || va->type == 0x1403 /* GL_UNSIGNED_SHORT */) {
        comp_size = 2;
    } else if (va->type == 0x1406 /* GL_FLOAT */) {
        comp_size = 4;
    }
    int effective_stride = va->stride > 0 ? va->stride : (size * comp_size);
    const uint8_t *base = (const uint8_t *)va->pointer + (vertex_idx * effective_stride);

    for (int k = 0; k < comp_count; k++) {
        if (va->type == 0x1406 /* GL_FLOAT */) {
            out[k] = *(const float *)(const void *)(base + k * 4);
        } else if (va->type == 0x1401 /* GL_UNSIGNED_BYTE */) {
            uint8_t b = *(base + k);
            out[k] = va->normalized ? ((float)b / 255.0f) : (float)b;
        } else if (va->type == 0x1402 /* GL_SHORT */) {
            int16_t s = *(const int16_t *)(const void *)(base + k * 2);
            out[k] = va->normalized ? ((float)s / 32767.0f) : (float)s;
        }
    }
}

static void fetch_vertex(int idx, SwVertex *v, int surf_width, int surf_height) {
    float pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    get_attrib_float(0, idx, pos, 4, 0.0f);
    if (pos[3] != 0.0f && pos[3] != 1.0f) {
        pos[0] /= pos[3];
        pos[1] /= pos[3];
    }
    v->x = (pos[0] + 1.0f) * 0.5f * (float)surf_width;
    v->y = (1.0f - (pos[1] + 1.0f) * 0.5f) * (float)surf_height;

    if (g_vertex_attribs[1].enabled) {
        float uv[2] = {0.0f, 0.0f};
        get_attrib_float(1, idx, uv, 2, 0.0f);
        v->u = uv[0];
        v->v = uv[1];
    } else {
        v->u = 0.0f;
        v->v = 0.0f;
    }

    if (g_vertex_attribs[2].enabled) {
        float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        get_attrib_float(2, idx, col, 4, 1.0f);
        v->r = col[0];
        v->g = col[1];
        v->b = col[2];
        v->a = col[3];
    } else {
        v->r = g_uniform_color[0];
        v->g = g_uniform_color[1];
        v->b = g_uniform_color[2];
        v->a = g_uniform_color[3];
    }
}

static inline float cross2d(float x1, float y1, float x2, float y2) {
    return x1 * y2 - y1 * x2;
}

static void rasterize_triangle(const SwVertex *v0_in, const SwVertex *v1_in, const SwVertex *v2_in) {
    if (!g_current_draw_surface || !g_current_draw_surface->pixels) return;

    SwVertex v0 = *v0_in;
    SwVertex v1 = *v1_in;
    SwVertex v2 = *v2_in;

    int surf_width = g_current_draw_surface->width;
    int surf_height = g_current_draw_surface->height;

    float area = cross2d(v1.x - v0.x, v1.y - v0.y, v2.x - v0.x, v2.y - v0.y);
    if (area == 0.0f) return;

    if (area < 0.0f) {
        SwVertex tmp = v1;
        v1 = v2;
        v2 = tmp;
        area = -area;
    }

    float inv_area = 1.0f / area;

    float min_xf = v0.x < v1.x ? (v0.x < v2.x ? v0.x : v2.x) : (v1.x < v2.x ? v1.x : v2.x);
    float max_xf = v0.x > v1.x ? (v0.x > v2.x ? v0.x : v2.x) : (v1.x > v2.x ? v1.x : v2.x);
    float min_yf = v0.y < v1.y ? (v0.y < v2.y ? v0.y : v2.y) : (v1.y < v2.y ? v1.y : v2.y);
    float max_yf = v0.y > v1.y ? (v0.y > v2.y ? v0.y : v2.y) : (v1.y > v2.y ? v1.y : v2.y);

    int min_x = (int)min_xf;
    int max_x = (int)(max_xf + 1.0f);
    int min_y = (int)min_yf;
    int max_y = (int)(max_yf + 1.0f);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= surf_width) max_x = surf_width - 1;
    if (max_y >= surf_height) max_y = surf_height - 1;

    if (min_x > max_x || min_y > max_y) return;

    SwTexture *tex = NULL;
    if (g_vertex_attribs[1].enabled && g_bound_texture > 0 && g_bound_texture < 64) {
        if (g_textures[g_bound_texture].pixels &&
            g_textures[g_bound_texture].width > 0 &&
            g_textures[g_bound_texture].height > 0) {
            tex = &g_textures[g_bound_texture];
        }
    }

    uint32_t *dst_pixels = g_current_draw_surface->pixels;

    for (int y = min_y; y <= max_y; y++) {
        float py = (float)y + 0.5f;
        for (int x = min_x; x <= max_x; x++) {
            float px = (float)x + 0.5f;

            float e0 = cross2d(v2.x - v1.x, v2.y - v1.y, px - v1.x, py - v1.y);
            float e1 = cross2d(v0.x - v2.x, v0.y - v2.y, px - v2.x, py - v2.y);
            float e2 = cross2d(v1.x - v0.x, v1.y - v0.y, px - v0.x, py - v0.y);

            if (e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) {
                float w0 = e0 * inv_area;
                float w1 = e1 * inv_area;
                float w2 = e2 * inv_area;

                float r = w0 * v0.r + w1 * v1.r + w2 * v2.r;
                float g = w0 * v0.g + w1 * v1.g + w2 * v2.g;
                float b = w0 * v0.b + w1 * v1.b + w2 * v2.b;
                float a = w0 * v0.a + w1 * v1.a + w2 * v2.a;

                if (tex) {
                    float u = w0 * v0.u + w1 * v1.u + w2 * v2.u;
                    float v = w0 * v0.v + w1 * v1.v + w2 * v2.v;

                    float u_int = (float)((int)u);
                    float v_int = (float)((int)v);
                    u = u - u_int; if (u < 0.0f) u += 1.0f;
                    v = v - v_int; if (v < 0.0f) v += 1.0f;

                    int tx = (int)(u * (float)tex->width);
                    int ty = (int)(v * (float)tex->height);
                    if (tx < 0) tx = 0; if (tx >= tex->width) tx = tex->width - 1;
                    if (ty < 0) ty = 0; if (ty >= tex->height) ty = tex->height - 1;

                    uint32_t texel = tex->pixels[(size_t)ty * (size_t)tex->width + (size_t)tx];
                    float ta = (float)((texel >> 24) & 0xff) / 255.0f;
                    float tr = (float)((texel >> 16) & 0xff) / 255.0f;
                    float tg = (float)((texel >> 8) & 0xff) / 255.0f;
                    float tb = (float)(texel & 0xff) / 255.0f;

                    r *= tr;
                    g *= tg;
                    b *= tb;
                    a *= ta;
                }

                size_t pidx = (size_t)y * (size_t)surf_width + (size_t)x;
                float out_r = r, out_g = g, out_b = b, out_a = a;

                if (g_blend_enabled || a < 0.999f) {
                    uint32_t dst_val = dst_pixels[pidx];
                    float da = (float)((dst_val >> 24) & 0xff) / 255.0f;
                    float dr = (float)((dst_val >> 16) & 0xff) / 255.0f;
                    float dg = (float)((dst_val >> 8) & 0xff) / 255.0f;
                    float db = (float)(dst_val & 0xff) / 255.0f;

                    out_r = a * r + (1.0f - a) * dr;
                    out_g = a * g + (1.0f - a) * dg;
                    out_b = a * b + (1.0f - a) * db;
                    out_a = a + (1.0f - a) * da;
                }

                uint32_t pr = (uint32_t)(out_r * 255.0f + 0.5f); if (pr > 255) pr = 255;
                uint32_t pg = (uint32_t)(out_g * 255.0f + 0.5f); if (pg > 255) pg = 255;
                uint32_t pb = (uint32_t)(out_b * 255.0f + 0.5f); if (pb > 255) pb = 255;
                uint32_t pa = (uint32_t)(out_a * 255.0f + 0.5f); if (pa > 255) pa = 255;

                dst_pixels[pidx] = (pa << 24) | (pr << 16) | (pg << 8) | pb;
            }
        }
    }
}

__attribute__((visibility("default")))
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (count <= 0) return;
    if (!g_current_draw_surface || !g_current_draw_surface->pixels) return;
    if (!g_vertex_attribs[0].enabled || !g_vertex_attribs[0].pointer) return;

    int sw = g_current_draw_surface->width;
    int sh = g_current_draw_surface->height;

    if (mode == 0x0004 /* GL_TRIANGLES */) {
        for (GLint i = 0; i + 2 < count; i += 3) {
            SwVertex v0, v1, v2;
            fetch_vertex(first + i, &v0, sw, sh);
            fetch_vertex(first + i + 1, &v1, sw, sh);
            fetch_vertex(first + i + 2, &v2, sw, sh);
            rasterize_triangle(&v0, &v1, &v2);
        }
    } else if (mode == 0x0005 /* GL_TRIANGLE_STRIP */) {
        for (GLint i = 0; i + 2 < count; i++) {
            SwVertex v0, v1, v2;
            fetch_vertex(first + i, &v0, sw, sh);
            fetch_vertex(first + i + 1, &v1, sw, sh);
            fetch_vertex(first + i + 2, &v2, sw, sh);
            rasterize_triangle(&v0, &v1, &v2);
        }
    } else if (mode == 0x0006 /* GL_TRIANGLE_FAN */) {
        for (GLint i = 1; i + 1 < count; i++) {
            SwVertex v0, v1, v2;
            fetch_vertex(first, &v0, sw, sh);
            fetch_vertex(first + i, &v1, sw, sh);
            fetch_vertex(first + i + 1, &v2, sw, sh);
            rasterize_triangle(&v0, &v1, &v2);
        }
    }
}

__attribute__((visibility("default")))
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    if (count <= 0 || !indices) return;
    if (!g_current_draw_surface || !g_current_draw_surface->pixels) return;
    if (!g_vertex_attribs[0].enabled || !g_vertex_attribs[0].pointer) return;

    int sw = g_current_draw_surface->width;
    int sh = g_current_draw_surface->height;

    #define GET_ELEM_IDX(k) ((type == 0x1401 /* GL_UNSIGNED_BYTE */) ? \
        (int)((const uint8_t *)indices)[(k)] : (int)((const uint16_t *)indices)[(k)])

    if (mode == 0x0004 /* GL_TRIANGLES */) {
        for (GLsizei i = 0; i + 2 < count; i += 3) {
            SwVertex v0, v1, v2;
            fetch_vertex(GET_ELEM_IDX(i), &v0, sw, sh);
            fetch_vertex(GET_ELEM_IDX(i + 1), &v1, sw, sh);
            fetch_vertex(GET_ELEM_IDX(i + 2), &v2, sw, sh);
            rasterize_triangle(&v0, &v1, &v2);
        }
    } else if (mode == 0x0005 /* GL_TRIANGLE_STRIP */) {
        for (GLsizei i = 0; i + 2 < count; i++) {
            SwVertex v0, v1, v2;
            fetch_vertex(GET_ELEM_IDX(i), &v0, sw, sh);
            fetch_vertex(GET_ELEM_IDX(i + 1), &v1, sw, sh);
            fetch_vertex(GET_ELEM_IDX(i + 2), &v2, sw, sh);
            rasterize_triangle(&v0, &v1, &v2);
        }
    } else if (mode == 0x0006 /* GL_TRIANGLE_FAN */) {
        for (GLsizei i = 1; i + 1 < count; i++) {
            SwVertex v0, v1, v2;
            fetch_vertex(GET_ELEM_IDX(0), &v0, sw, sh);
            fetch_vertex(GET_ELEM_IDX(i), &v1, sw, sh);
            fetch_vertex(GET_ELEM_IDX(i + 1), &v2, sw, sh);
            rasterize_triangle(&v0, &v1, &v2);
        }
    }
    #undef GET_ELEM_IDX
}

__attribute__((visibility("default")))
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
    if (!pixels || width <= 0 || height <= 0) return;
    if (!g_current_read_surface || !g_current_read_surface->pixels) {
        memset(pixels, 0, (size_t)width * (size_t)height * 4);
        return;
    }

    SwSurface *s = g_current_read_surface;
    uint8_t *dst = (uint8_t *)pixels;
    int align = (g_pack_alignment == 1 || g_pack_alignment == 2 || g_pack_alignment == 4 || g_pack_alignment == 8) ? g_pack_alignment : 4;

    for (GLint row = 0; row < height; row++) {
        GLint sy = y + row;
        for (GLint col = 0; col < width; col++) {
            GLint sx = x + col;
            uint32_t val = 0;
            if (sx >= 0 && sx < s->width && sy >= 0 && sy < s->height) {
                val = s->pixels[sy * s->width + sx];
            }
            uint8_t a = (uint8_t)((val >> 24) & 0xff);
            uint8_t r = (uint8_t)((val >> 16) & 0xff);
            uint8_t g = (uint8_t)((val >> 8) & 0xff);
            uint8_t b = (uint8_t)(val & 0xff);

            if (format == GL_RGBA) {
                size_t row_bytes = (size_t)width * 4;
                size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
                size_t idx = (size_t)row * row_stride + (size_t)col * 4;
                dst[idx + 0] = r;
                dst[idx + 1] = g;
                dst[idx + 2] = b;
                dst[idx + 3] = a;
            } else if (format == GL_RGB) {
                size_t row_bytes = (size_t)width * 3;
                size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
                size_t idx = (size_t)row * row_stride + (size_t)col * 3;
                dst[idx + 0] = r;
                dst[idx + 1] = g;
                dst[idx + 2] = b;
            } else {
                /* Default BGRA / native */
                size_t row_bytes = (size_t)width * 4;
                size_t row_stride = (row_bytes + align - 1) & ~((size_t)align - 1);
                size_t idx = (size_t)row * row_stride + (size_t)col * 4;
                dst[idx + 0] = b;
                dst[idx + 1] = g;
                dst[idx + 2] = r;
                dst[idx + 3] = a;
            }
        }
    }
}

__attribute__((visibility("default")))
void glPixelStorei(GLenum pname, GLint param) {
    if (pname == 0x0D05 /* GL_PACK_ALIGNMENT */) {
        if (param == 1 || param == 2 || param == 4 || param == 8) {
            g_pack_alignment = param;
        }
    } else if (pname == 0x0CF5 /* GL_UNPACK_ALIGNMENT */) {
        if (param == 1 || param == 2 || param == 4 || param == 8) {
            g_unpack_alignment = param;
        }
    }
}

__attribute__((visibility("default")))
void glFlush(void) {}

__attribute__((visibility("default")))
void glEGLImageTargetTexture2DOES(GLenum target, void *image) {}

/* eglGetProcAddress extension dispatcher */
__attribute__((visibility("default")))
__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *procname) {
    if (!procname) return NULL;
    if (strcmp(procname, "eglCreateImageKHR") == 0) return (__eglMustCastToProperFunctionPointerType)eglCreateImageKHR;
    if (strcmp(procname, "eglDestroyImageKHR") == 0) return (__eglMustCastToProperFunctionPointerType)eglDestroyImageKHR;
    if (strcmp(procname, "eglCreateSyncKHR") == 0) return (__eglMustCastToProperFunctionPointerType)eglCreateSyncKHR;
    if (strcmp(procname, "eglDestroySyncKHR") == 0) return (__eglMustCastToProperFunctionPointerType)eglDestroySyncKHR;
    if (strcmp(procname, "eglClientWaitSyncKHR") == 0) return (__eglMustCastToProperFunctionPointerType)eglClientWaitSyncKHR;
    if (strcmp(procname, "eglWaitSyncKHR") == 0) return (__eglMustCastToProperFunctionPointerType)eglWaitSyncKHR;
    if (strcmp(procname, "eglDupNativeFenceFDANDROID") == 0) return (__eglMustCastToProperFunctionPointerType)eglDupNativeFenceFDANDROID;
    if (strcmp(procname, "glEGLImageTargetTexture2DOES") == 0) return (__eglMustCastToProperFunctionPointerType)glEGLImageTargetTexture2DOES;
    if (strcmp(procname, "glDrawElements") == 0) return (__eglMustCastToProperFunctionPointerType)glDrawElements;
    if (strcmp(procname, "glVertexAttribPointer") == 0) return (__eglMustCastToProperFunctionPointerType)glVertexAttribPointer;
    if (strcmp(procname, "glEnableVertexAttribArray") == 0) return (__eglMustCastToProperFunctionPointerType)glEnableVertexAttribArray;
    if (strcmp(procname, "glDisableVertexAttribArray") == 0) return (__eglMustCastToProperFunctionPointerType)glDisableVertexAttribArray;
    if (strcmp(procname, "glUniform4f") == 0) return (__eglMustCastToProperFunctionPointerType)glUniform4f;
    if (strcmp(procname, "glUniform4fv") == 0) return (__eglMustCastToProperFunctionPointerType)glUniform4fv;
    return NULL;
}
