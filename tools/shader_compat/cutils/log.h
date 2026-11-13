/* Map the old platform-only cutils logging include to the public NDK API. */
#ifndef OPENDTV_SHADER_LOG_H
#define OPENDTV_SHADER_LOG_H
#include <android/log.h>
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "OpenDTV-GLSL", __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "OpenDTV-GLSL", __VA_ARGS__)
#endif
