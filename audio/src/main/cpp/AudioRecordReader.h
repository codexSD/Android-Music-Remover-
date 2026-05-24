#ifndef VOCALREMOVER_AUDIORECORDREADER_H
#define VOCALREMOVER_AUDIORECORDREADER_H

#include <jni.h>

#include <atomic>
#include <thread>
#include <vector>

#include "RingBuffer.h"

namespace vocalremover {

// Drives the capture side of the pipeline. Owns a thread that repeatedly calls
// AudioRecord.read() (PCM float) via a cached JNI method id and pushes the
// captured interleaved frames into the supplied ring buffer.
//
// The AudioRecord object itself is created in Kotlin (where MediaProjection and
// AudioPlaybackCaptureConfiguration live) and handed down via JNI; this class
// only reads from it.
class AudioRecordReader {
public:
    // `audioRecord` is a local ref to an android.media.AudioRecord. A global
    // ref is taken internally. `chunkFrames` is the number of frames requested
    // per read() call. `output` must outlive this reader.
    AudioRecordReader(JNIEnv* env, jobject audioRecord, int channelCount,
                      int chunkFrames, SpscRingBuffer<float>* output);
    ~AudioRecordReader();

    AudioRecordReader(const AudioRecordReader&) = delete;
    AudioRecordReader& operator=(const AudioRecordReader&) = delete;

    bool start();
    void stop();

    // Diagnostics, updated from the capture thread.
    uint64_t framesCaptured() const { return framesCaptured_.load(std::memory_order_relaxed); }
    uint64_t framesDropped() const { return framesDropped_.load(std::memory_order_relaxed); }
    uint64_t readErrors() const { return readErrors_.load(std::memory_order_relaxed); }

private:
    void captureLoop();

    jobject audioRecordGlobal_ = nullptr;     // global ref
    jfloatArray transferArrayGlobal_ = nullptr;  // global ref, JVM-side scratch
    jmethodID readFloatMethod_ = nullptr;
    jmethodID startRecordingMethod_ = nullptr;
    jmethodID stopMethod_ = nullptr;

    const int channelCount_;
    const int transferFloats_;  // chunkFrames * channelCount

    SpscRingBuffer<float>* output_;
    std::vector<float> scratch_;  // native copy target

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::atomic<uint64_t> framesCaptured_{0};
    std::atomic<uint64_t> framesDropped_{0};
    std::atomic<uint64_t> readErrors_{0};
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_AUDIORECORDREADER_H
