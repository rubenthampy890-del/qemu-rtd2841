/* GLSL syntax/type validation using the pinned SwiftShader compiler frontend.
 * This does not supply shader execution or claim GLES conformance.
 */
#include "Compiler.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <pthread.h>

#ifdef __ANDROID__
/* Upstream logs an internal compiler assertion before calling this hook.
 * Never continue with a corrupt compiler state or silently accept a shader. */
void AndroidEnterDebugger() { std::abort(); }
#endif

namespace {
class Validator final : public TCompiler {
public:
    explicit Validator(GLenum type) : TCompiler(type) {}
private:
    bool translate(TIntermNode *) override { return false; }
};
pthread_mutex_t compiler_mutex = PTHREAD_MUTEX_INITIALIZER;
bool initialized = false;
bool initialization_attempted = false;

void message(char *log, size_t size, const char *text)
{
    if (log && size) {
        size_t n = std::strlen(text);
        if (n >= size) n = size - 1;
        std::memcpy(log, text, n);
        log[n] = '\0';
    }
}
}

extern "C" int sw_validate_shader(unsigned int type, const char *source,
                                  char *log, size_t log_size)
{
    message(log, log_size, "");
    if (!source || !*source ||
        (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER)) {
        message(log, log_size, "Empty shader source or invalid shader stage");
        return 0;
    }
    int rc = pthread_mutex_lock(&compiler_mutex);
    if (rc != 0) {
        if (log && log_size) {
            std::snprintf(log, log_size, "Compiler mutex lock failed: %d", rc);
        }
        return 0;
    }
    int valid = 0;
    try {
        /* Compiler globals are TLS keys retained for this library's lifetime.
         * Serialize construction/compilation because upstream owns globals.
         */
        if (!initialization_attempted) {
            initialization_attempted = true;
            initialized = InitCompilerGlobals();
        }
        if (!initialized) {
            message(log, log_size, "Compiler TLS initialization failed");
        } else {
            Validator compiler(type);
            ShBuiltInResources resources;
            resources.MaxVertexAttribs = 16;
            resources.MaxVertexUniformVectors = 128;
            resources.MaxVaryingVectors = 8;
            resources.MaxVertexTextureImageUnits = 16;
            resources.MaxCombinedTextureImageUnits = 16;
            resources.MaxTextureImageUnits = 16;
            resources.MaxFragmentUniformVectors = 128;
            resources.EXT_draw_buffers = 0;
            resources.ARB_texture_rectangle = 0;
            resources.OES_fragment_precision_high = 1;
            resources.OES_EGL_image_external = 1;
            if (!compiler.Init(resources)) {
                message(log, log_size, "Compiler resource initialization failed");
            } else {
                valid = compiler.compile(&source, 1, SH_VALIDATE);
                message(log, log_size, compiler.getInfoSink().info.c_str());
                if (compiler.getShaderVersion() != 100) {
                    valid = 0;
                    message(log, log_size, "Only GLSL ES 1.00 is supported");
                }
                if (!valid && log && log_size && !*log) {
                    message(log, log_size, "GLSL compilation failed");
                }
            }
        }
    } catch (const std::exception &e) {
        message(log, log_size, e.what());
        valid = 0;
    } catch (...) {
        message(log, log_size, "Unexpected shader compiler exception");
        valid = 0;
    }
    rc = pthread_mutex_unlock(&compiler_mutex);
    if (rc != 0) {
        if (log && log_size) {
            std::snprintf(log, log_size, "Compiler mutex unlock failed: %d", rc);
        }
        return 0;
    }
    return valid;
}
