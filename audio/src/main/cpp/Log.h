#ifndef VOCALREMOVER_LOG_H
#define VOCALREMOVER_LOG_H

#include <android/log.h>

#define VR_LOG_TAG "VocalRemoverNative"

#define VR_LOGI(...) __android_log_print(ANDROID_LOG_INFO, VR_LOG_TAG, __VA_ARGS__)
#define VR_LOGW(...) __android_log_print(ANDROID_LOG_WARN, VR_LOG_TAG, __VA_ARGS__)
#define VR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, VR_LOG_TAG, __VA_ARGS__)

#endif  // VOCALREMOVER_LOG_H
