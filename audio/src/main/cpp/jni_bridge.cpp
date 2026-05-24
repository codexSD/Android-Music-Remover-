#include <jni.h>

#include "AudioEngine.h"
#include "JniSupport.h"

using vocalremover::AudioEngine;

namespace {
inline AudioEngine* toEngine(jlong handle) {
    return reinterpret_cast<AudioEngine*>(handle);
}
}  // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    vocalremover::setJavaVM(vm);
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeCreate(JNIEnv* /*env*/,
                                                             jobject /*thiz*/) {
    return reinterpret_cast<jlong>(new AudioEngine());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeStart(
    JNIEnv* env, jobject /*thiz*/, jlong handle, jobject audioRecord,
    jint sampleRate, jint channelCount, jbyteArray modelBytes) {
    AudioEngine* engine = toEngine(handle);
    if (engine == nullptr) return JNI_FALSE;

    const uint8_t* modelData = nullptr;
    size_t modelLen = 0;
    jbyte* elems = nullptr;
    if (modelBytes != nullptr) {
        modelLen = static_cast<size_t>(env->GetArrayLength(modelBytes));
        elems = env->GetByteArrayElements(modelBytes, nullptr);
        modelData = reinterpret_cast<const uint8_t*>(elems);
    }

    const bool ok = engine->start(env, audioRecord, sampleRate, channelCount,
                                  modelData, modelLen);

    if (elems != nullptr) {
        env->ReleaseByteArrayElements(modelBytes, elems, JNI_ABORT);
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeStop(JNIEnv* /*env*/,
                                                           jobject /*thiz*/,
                                                           jlong handle) {
    AudioEngine* engine = toEngine(handle);
    if (engine != nullptr) engine->stop();
}

extern "C" JNIEXPORT void JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeDestroy(JNIEnv* /*env*/,
                                                              jobject /*thiz*/,
                                                              jlong handle) {
    delete toEngine(handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeFramesCaptured(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    AudioEngine* engine = toEngine(handle);
    return engine ? static_cast<jlong>(engine->framesCaptured()) : 0;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeFramesDropped(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    AudioEngine* engine = toEngine(handle);
    return engine ? static_cast<jlong>(engine->framesDropped()) : 0;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeUnderrunFrames(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    AudioEngine* engine = toEngine(handle);
    return engine ? static_cast<jlong>(engine->underrunFrames()) : 0;
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_codexsd_vocalremover_audio_AudioEngine_nativeCaptureRms(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    AudioEngine* engine = toEngine(handle);
    return engine ? engine->captureRms() : 0.0f;
}
