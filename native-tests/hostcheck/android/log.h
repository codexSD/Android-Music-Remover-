// HOST SYNTAX-CHECK STUB ONLY. Not used by the Android build (the real header
// ships with the NDK). Exists so the Android-specific engine sources can be
// compiled with -fsyntax-only on a plain host to catch C++ errors.
#ifndef HOSTCHECK_ANDROID_LOG_H
#define HOSTCHECK_ANDROID_LOG_H

typedef enum android_LogPriority {
    ANDROID_LOG_INFO = 4,
    ANDROID_LOG_WARN = 5,
    ANDROID_LOG_ERROR = 6,
} android_LogPriority;

#ifdef __cplusplus
extern "C" {
#endif
int __android_log_print(int prio, const char* tag, const char* fmt, ...);
#ifdef __cplusplus
}
#endif

#endif
