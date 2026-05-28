#include "AudioRecordReader.h"

#include <pthread.h>

#include <cmath>

#include "JniSupport.h"
#include "Log.h"

namespace vocalremover {

namespace {
// android.media.AudioRecord.READ_BLOCKING
constexpr jint kReadBlocking = 0;
}  // namespace

AudioRecordReader::AudioRecordReader(JNIEnv* env, jobject audioRecord,
                                     int channelCount, int chunkFrames,
                                     SpscRingBuffer<float>* output)
    : channelCount_(channelCount),
      transferFloats_(chunkFrames * channelCount),
      output_(output) {
    audioRecordGlobal_ = env->NewGlobalRef(audioRecord);

    jclass cls = env->GetObjectClass(audioRecord);
    readFloatMethod_ = env->GetMethodID(cls, "read", "([FIII)I");
    startRecordingMethod_ = env->GetMethodID(cls, "startRecording", "()V");
    stopMethod_ = env->GetMethodID(cls, "stop", "()V");
    env->DeleteLocalRef(cls);

    jfloatArray localArray = env->NewFloatArray(transferFloats_);
    transferArrayGlobal_ = static_cast<jfloatArray>(env->NewGlobalRef(localArray));
    env->DeleteLocalRef(localArray);

    scratch_.resize(static_cast<size_t>(transferFloats_));
}

AudioRecordReader::~AudioRecordReader() {
    stop();
    ScopedJniEnv scoped;
    if (scoped.valid()) {
        JNIEnv* env = scoped.env();
        if (transferArrayGlobal_ != nullptr) env->DeleteGlobalRef(transferArrayGlobal_);
        if (audioRecordGlobal_ != nullptr) env->DeleteGlobalRef(audioRecordGlobal_);
    }
    transferArrayGlobal_ = nullptr;
    audioRecordGlobal_ = nullptr;
}

bool AudioRecordReader::start() {
    if (running_.exchange(true)) {
        return false;  // already running
    }
    if (readFloatMethod_ == nullptr || startRecordingMethod_ == nullptr) {
        VR_LOGE("AudioRecordReader: missing AudioRecord method ids");
        running_.store(false);
        return false;
    }
    thread_ = std::thread(&AudioRecordReader::captureLoop, this);
    return true;
}

void AudioRecordReader::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void AudioRecordReader::captureLoop() {
    pthread_setname_np(pthread_self(), "vr_capture");

    ScopedJniEnv scoped;
    if (!scoped.valid()) {
        VR_LOGE("AudioRecordReader: capture thread could not attach to JVM");
        running_.store(false);
        return;
    }
    JNIEnv* env = scoped.env();

    env->CallVoidMethod(audioRecordGlobal_, startRecordingMethod_);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        VR_LOGE("AudioRecordReader: startRecording() threw");
        running_.store(false);
        return;
    }

    while (running_.load(std::memory_order_relaxed)) {
        const jint floatsRead = env->CallIntMethod(
            audioRecordGlobal_, readFloatMethod_, transferArrayGlobal_, 0,
            transferFloats_, kReadBlocking);

        if (floatsRead <= 0) {
            // Negative values are AudioRecord error codes; zero means no data.
            if (floatsRead < 0) {
                readErrors_.fetch_add(1, std::memory_order_relaxed);
                VR_LOGW("AudioRecord.read returned error %d", floatsRead);
            }
            continue;
        }

        env->GetFloatArrayRegion(transferArrayGlobal_, 0, floatsRead, scratch_.data());

        double sumSq = 0.0;
        for (jint i = 0; i < floatsRead; ++i) {
            sumSq += static_cast<double>(scratch_[i]) * scratch_[i];
        }
        captureRms_.store(static_cast<float>(std::sqrt(sumSq / floatsRead)),
                          std::memory_order_relaxed);

        const size_t written = output_->write(scratch_.data(),
                                               static_cast<size_t>(floatsRead));
        const auto frames = static_cast<uint64_t>(floatsRead) /
                            static_cast<uint64_t>(channelCount_);
        framesCaptured_.fetch_add(frames, std::memory_order_relaxed);
        if (written < static_cast<size_t>(floatsRead)) {
            const uint64_t droppedFloats = static_cast<uint64_t>(floatsRead) - written;
            framesDropped_.fetch_add(droppedFloats / channelCount_,
                                     std::memory_order_relaxed);
        }
    }

    env->CallVoidMethod(audioRecordGlobal_, stopMethod_);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

}  // namespace vocalremover
