/* Map the old platform-only cutils logging include to the public NDK API. */
#ifndef RTD2841_SHADER_LOG_H
#define RTD2841_SHADER_LOG_H
#include <android/log.h>
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "RTD2841-GLSL", __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "RTD2841-GLSL", __VA_ARGS__)
#endif
