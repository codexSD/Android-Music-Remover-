#ifndef VOCALREMOVER_OUTPUTSTREAM_H
#define VOCALREMOVER_OUTPUTSTREAM_H

#include <oboe/Oboe.h>

#include <atomic>
#include <cstdint>
#include <memory>

#include "AudioProcessor.h"
#include "RingBuffer.h"

namespace vocalremover {

// Oboe-backed low-latency output. Its data callback is the real-time consumer
// of the ring buffer: it pulls captured frames, runs them through the
// AudioProcessor, and hands them to the audio device. On underrun it emits
// silence rather than blocking, keeping the callback real-time safe.
class OutputStream : public oboe::AudioStreamDataCallback,
                     public oboe::AudioStreamErrorCallback {
public:
    OutputStream(int sampleRate, int channelCount, SpscRingBuffer<float>* input,
                 AudioProcessor* processor);
    ~OutputStream() override;

    bool open();
    bool start();
    void stop();
    void close();

    int sampleRate() const { return sampleRate_; }
    int channelCount() const { return channelCount_; }
    uint64_t underrunFrames() const { return underrunFrames_.load(std::memory_order_relaxed); }

    // oboe::AudioStreamDataCallback
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData,
                                          int32_t numFrames) override;

    // oboe::AudioStreamErrorCallback
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

private:
    int sampleRate_;
    int channelCount_;
    SpscRingBuffer<float>* input_;
    AudioProcessor* processor_;

    std::shared_ptr<oboe::AudioStream> stream_;
    std::atomic<uint64_t> underrunFrames_{0};
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_OUTPUTSTREAM_H
