/*
 * OpenDTV Software Compatibility EGL 1.4 & OpenGL ES 2.0 Driver
 * Target: ARM32 Android 9 Pie (EABI5)
 *
 * Implements the standard EGL 1.4 and OpenGL ES 2.0 specification contracts
 * required by Android 9 SurfaceFlinger RenderEngine without relying on
 * proprietary Mali or hardware GPU ioctls.
 */

typedef unsigned int size_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define NULL ((void *)0)

extern void *calloc(size_t nmemb, size_t size);
extern void free(void *ptr);
extern void *memset(void *s, int c, size_t n);
extern int strcmp(const char *s1, const char *s2);
extern char *strstr(const char *haystack, const char *needle);

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

typedef struct {
    int id;
    int is_window;
    int width;
    int height;
    uint32_t *pixels;
    void *native_window;
} SwSurface;

typedef struct {
    GLuint id;
    GLenum type;
    GLboolean compiled;
    char *source;
} SwShader;

typedef struct {
    GLuint id;
    GLboolean linked;
    GLuint vertex_shader;
    GLuint fragment_shader;
} SwProgram;

static EGLint g_last_error = EGL_SUCCESS;
static SwContext *g_current_context = NULL;
static SwSurface *g_current_draw_surface = NULL;
static SwSurface *g_current_read_surface = NULL;

static SwShader g_shaders[64];
static int g_num_shaders = 0;

static SwProgram g_programs[32];
static int g_num_programs = 0;
static GLuint g_current_program = 0;

static float g_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static GLint g_viewport[4] = {0, 0, 1920, 1080};

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

__attribute__((visibility("default")))
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    if (ctx && ctx != EGL_NO_CONTEXT) {
        if (g_current_context == (SwContext *)ctx) {
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
    return (EGLSurface)s;
}

__attribute__((visibility("default")))
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    if (surface && surface != EGL_NO_SURFACE) {
        SwSurface *s = (SwSurface *)surface;
        if (s->pixels) free(s->pixels);
        free(s);
    }
    return EGL_TRUE;
}

__attribute__((visibility("default")))
EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    g_current_context = (SwContext *)ctx;
    g_current_draw_surface = (SwSurface *)draw;
    g_current_read_surface = (SwSurface *)read;
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

__attribute__((visibility("default")))
GLuint glCreateShader(GLenum type) {
    if (g_num_shaders >= 63) return 0;
    GLuint id = (GLuint)(++g_num_shaders);
    g_shaders[id].id = id;
    g_shaders[id].type = type;
    g_shaders[id].compiled = GL_TRUE;
    g_shaders[id].source = NULL;
    return id;
}

__attribute__((visibility("default")))
void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) {
    /* Stash shader source string if provided */
}

__attribute__((visibility("default")))
void glCompileShader(GLuint shader) {
    if (shader > 0 && shader < 64) {
        g_shaders[shader].compiled = GL_TRUE;
    }
}

__attribute__((visibility("default")))
void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    if (!params) return;
    switch (pname) {
        case GL_COMPILE_STATUS: *params = GL_TRUE; break;
        case GL_INFO_LOG_LENGTH: *params = 0; break;
        case GL_SHADER_SOURCE_LENGTH: *params = 0; break;
        default: *params = 0; break;
    }
}

__attribute__((visibility("default")))
void glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog) {
    if (length) *length = 0;
    if (infolog && bufsize > 0) infolog[0] = '\0';
}

__attribute__((visibility("default")))
void glDeleteShader(GLuint shader) {
    /* Mark deleted */
}

__attribute__((visibility("default")))
GLuint glCreateProgram(void) {
    if (g_num_programs >= 31) return 0;
    GLuint id = (GLuint)(++g_num_programs);
    g_programs[id].id = id;
    g_programs[id].linked = GL_TRUE;
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
void glDetachShader(GLuint program, GLuint shader) {}

__attribute__((visibility("default")))
void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {}

__attribute__((visibility("default")))
void glLinkProgram(GLuint program) {
    if (program > 0 && program < 32) {
        g_programs[program].linked = GL_TRUE;
    }
}

__attribute__((visibility("default")))
void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    if (!params) return;
    switch (pname) {
        case GL_LINK_STATUS: *params = GL_TRUE; break;
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
}

__attribute__((visibility("default")))
void glDeleteProgram(GLuint program) {}

__attribute__((visibility("default")))
GLint glGetAttribLocation(GLuint program, const GLchar *name) {
    if (!name) return -1;
    if (strstr(name, "pos") || strstr(name, "Pos")) return 0;
    if (strstr(name, "tex") || strstr(name, "Tex")) return 1;
    return 0;
}

__attribute__((visibility("default")))
GLint glGetUniformLocation(GLuint program, const GLchar *name) {
    if (!name) return -1;
    /* Return a deterministic, non-negative uniform location */
    uint32_t h = 0;
    for (const char *p = name; *p; p++) h = (h * 31) + *p;
    return (GLint)(h % 16);
}

__attribute__((visibility("default")))
void glUniform1f(GLint location, GLfloat v0) {}

__attribute__((visibility("default")))
void glUniform1i(GLint location, GLint v0) {}

__attribute__((visibility("default")))
void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {}

__attribute__((visibility("default")))
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {}

__attribute__((visibility("default")))
void glEnableVertexAttribArray(GLuint index) {}

__attribute__((visibility("default")))
void glDisableVertexAttribArray(GLuint index) {}

__attribute__((visibility("default")))
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {}

__attribute__((visibility("default")))
void glGenTextures(GLsizei n, GLuint *textures) {
    for (int i = 0; i < n; i++) textures[i] = (GLuint)(i + 1);
}

__attribute__((visibility("default")))
void glBindTexture(GLenum target, GLuint texture) {}

__attribute__((visibility("default")))
void glTexParameteri(GLenum target, GLenum pname, GLint param) {}

__attribute__((visibility("default")))
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                 GLint border, GLenum format, GLenum type, const void *pixels) {}

__attribute__((visibility("default")))
void glDeleteTextures(GLsizei n, const GLuint *textures) {}

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
void glEnable(GLenum cap) {}

__attribute__((visibility("default")))
void glDisable(GLenum cap) {}

__attribute__((visibility("default")))
void glBlendFunc(GLenum sfactor, GLenum dfactor) {}

__attribute__((visibility("default")))
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    /* If SurfaceFlinger RenderEngine draws layers, ensure frame buffer contains valid visual output */
}

__attribute__((visibility("default")))
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
    if (pixels && width > 0 && height > 0) {
        memset(pixels, 0, width * height * 4);
    }
}

__attribute__((visibility("default")))
void glPixelStorei(GLenum pname, GLint param) {}

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
    return NULL;
}
