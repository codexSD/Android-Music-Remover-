#include "JniSupport.h"

#include "Log.h"

namespace vocalremover {

namespace {
JavaVM* g_vm = nullptr;
}

void setJavaVM(JavaVM* vm) { g_vm = vm; }

JavaVM* getJavaVM() { return g_vm; }

ScopedJniEnv::ScopedJniEnv() {
    if (g_vm == nullptr) {
        VR_LOGE("ScopedJniEnv: JavaVM not set");
        return;
    }
    const jint result = g_vm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
    if (result == JNI_OK) {
        return;  // Already attached; do not detach on destruction.
    }
    if (g_vm->AttachCurrentThread(&env_, nullptr) == JNI_OK) {
        attached_ = true;
    } else {
        VR_LOGE("ScopedJniEnv: failed to attach thread");
        env_ = nullptr;
    }
}

ScopedJniEnv::~ScopedJniEnv() {
    if (attached_ && g_vm != nullptr) {
        g_vm->DetachCurrentThread();
    }
}

}  // namespace vocalremover
