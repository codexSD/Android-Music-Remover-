#ifndef VOCALREMOVER_AUDIOENGINE_H
#define VOCALREMOVER_AUDIOENGINE_H

#include <jni.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "AudioProcessor.h"
#include "AudioRecordReader.h"
#include "EngineConfig.h"
#include "OutputStream.h"
#include "ProcessingThread.h"
#include "RingBuffer.h"

namespace vocalremover {

// Top-level owner of the audio path:
//
//   AudioRecord ─▶ ringIn ─▶ ProcessingThread ─▶ ringOut ─▶ Oboe callback
//   (capture)               (AudioProcessor)              (output)
//
// The processor is built at start() from a registry, by the engine id the Kotlin
// resolver decided on; the pipeline contains no selection logic. Changing engine
// is done by stop() + start() with a new id (no live swap). Either way the
// capture/output plumbing is identical.
class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    // Starts the pipeline. `audioRecord` is a configured android.media
    // .AudioRecord (PCM float). `engineId` selects the engine from the registry
    // (e.g. "dsp-center", "ml-bandscnet", "passthrough"). `config` carries small
    // per-engine tunables. `modelData`/`modelLen` optionally provide an ONNX
    // model for the ML engine; pass nullptr/0 otherwise. Returns false if the
    // engine is unknown or fails to init (the resolver then falls back).
    bool start(JNIEnv* env, jobject audioRecord, int sampleRate, int channelCount,
               const std::string& engineId, const EngineConfig& config,
               const uint8_t* modelData, size_t modelLen);
    void stop();
    bool isRunning() const { return running_; }

    uint64_t framesCaptured() const;
    uint64_t framesDropped() const;
    uint64_t underrunFrames() const;
    uint64_t deadlineViolations() const;
    float captureRms() const;

private:
    static size_t ringCapacityFor(int sampleRate, int channelCount);

    bool running_ = false;
    int sampleRate_ = 0;
    int channelCount_ = 0;

    std::unique_ptr<SpscRingBuffer<float>> ringIn_;
    std::unique_ptr<SpscRingBuffer<float>> ringOut_;
    std::unique_ptr<AudioProcessor> processor_;
    std::unique_ptr<ProcessingThread> processing_;
    std::unique_ptr<OutputStream> output_;
    std::unique_ptr<AudioRecordReader> reader_;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_AUDIOENGINE_H
