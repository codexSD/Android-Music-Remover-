#ifndef VOCALREMOVER_PROCESSINGTHREAD_H
#define VOCALREMOVER_PROCESSINGTHREAD_H

#include <atomic>
#include <thread>
#include <vector>

#include "AudioProcessor.h"
#include "RingBuffer.h"

namespace vocalremover {

// The middle stage of the pipeline. Drains the capture ring in fixed blocks,
// runs the AudioProcessor (passthrough in Phase 1, the spectrogram/ONNX
// separator in Phase 2), and writes the result to the output ring.
//
// Keeping this on its own thread is what makes the design safe: model inference
// can take milliseconds, which would be catastrophic inside the Oboe callback,
// but here it only has to keep up on average while the two rings absorb jitter.
class ProcessingThread {
public:
    ProcessingThread(SpscRingBuffer<float>* input, SpscRingBuffer<float>* output,
                     AudioProcessor* processor, int channelCount, size_t blockFrames);
    ~ProcessingThread();

    ProcessingThread(const ProcessingThread&) = delete;
    ProcessingThread& operator=(const ProcessingThread&) = delete;

    bool start();
    void stop();

    uint64_t framesProcessed() const { return framesProcessed_.load(std::memory_order_relaxed); }

private:
    void run();

    SpscRingBuffer<float>* input_;
    SpscRingBuffer<float>* output_;
    AudioProcessor* processor_;
    const int channelCount_;
    const size_t blockFrames_;
    const size_t blockSamples_;

    std::vector<float> block_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> framesProcessed_{0};
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_PROCESSINGTHREAD_H
