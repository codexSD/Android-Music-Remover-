#ifndef VOCALREMOVER_AUDIOENGINE_H
#define VOCALREMOVER_AUDIOENGINE_H

#include <jni.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "AudioProcessor.h"
#include "AudioRecordReader.h"
#include "OutputStream.h"
#include "ProcessingThread.h"
#include "RingBuffer.h"

namespace vocalremover {

// Top-level owner of the audio path:
//
//   AudioRecord ─▶ ringIn ─▶ ProcessingThread ─▶ ringOut ─▶ Oboe callback
//   (capture)               (AudioProcessor)              (output)
//
// The processor is selected at start(): if model bytes are supplied and ONNX
// support is compiled in, it is a spectrogram separator (Band-SCNet); otherwise
// it is a passthrough. Either way the capture/output plumbing is identical.
class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    // Starts the pipeline. `audioRecord` is a configured android.media
    // .AudioRecord (PCM float). `modelData`/`modelLen` optionally provide an
    // ONNX model; pass nullptr/0 for passthrough.
    bool start(JNIEnv* env, jobject audioRecord, int sampleRate, int channelCount,
               const uint8_t* modelData, size_t modelLen);
    void stop();
    bool isRunning() const { return running_; }

    uint64_t framesCaptured() const;
    uint64_t framesDropped() const;
    uint64_t underrunFrames() const;

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
