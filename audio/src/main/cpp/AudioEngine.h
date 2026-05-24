#ifndef VOCALREMOVER_AUDIOENGINE_H
#define VOCALREMOVER_AUDIOENGINE_H

#include <jni.h>

#include <cstdint>
#include <memory>

#include "AudioProcessor.h"
#include "AudioRecordReader.h"
#include "OutputStream.h"
#include "RingBuffer.h"

namespace vocalremover {

// Top-level owner of the Phase 1 audio path:
//
//   AudioRecord (capture thread)  ->  ring buffer  ->  Oboe callback (output)
//                                                          |
//                                                    AudioProcessor
//
// In Phase 1 the processor is a passthrough, proving the plumbing end to end.
// Phase 2 swaps in the Band-SCNet separator without touching this wiring.
class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    // Starts capture + output. `audioRecord` is an android.media.AudioRecord
    // configured by Kotlin (PCM float, given rate/channels). Returns false if
    // already running or if either stage fails to start.
    bool start(JNIEnv* env, jobject audioRecord, int sampleRate, int channelCount);
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

    std::unique_ptr<SpscRingBuffer<float>> ring_;
    std::unique_ptr<AudioProcessor> processor_;
    std::unique_ptr<OutputStream> output_;
    std::unique_ptr<AudioRecordReader> reader_;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_AUDIOENGINE_H
