#ifndef VOCALREMOVER_JNISUPPORT_H
#define VOCALREMOVER_JNISUPPORT_H

#include <jni.h>

namespace vocalremover {

// Set once from JNI_OnLoad so native worker threads can reach the VM.
void setJavaVM(JavaVM* vm);
JavaVM* getJavaVM();

// RAII helper that attaches the current native thread to the JVM (if it is not
// already attached) and detaches it on destruction. Used by the capture thread,
// which is created in C++ and must attach before making JNI calls.
class ScopedJniEnv {
public:
    ScopedJniEnv();
    ~ScopedJniEnv();

    JNIEnv* env() const { return env_; }
    bool valid() const { return env_ != nullptr; }

private:
    JNIEnv* env_ = nullptr;
    bool attached_ = false;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_JNISUPPORT_H
