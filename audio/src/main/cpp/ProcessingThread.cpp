#include "ProcessingThread.h"

#include <pthread.h>

#include <chrono>

#include "Log.h"

namespace vocalremover {

ProcessingThread::ProcessingThread(SpscRingBuffer<float>* input,
                                   SpscRingBuffer<float>* output,
                                   AudioProcessor* processor, int channelCount,
                                   size_t blockFrames)
    : input_(input),
      output_(output),
      processor_(processor),
      channelCount_(channelCount),
      blockFrames_(blockFrames),
      blockSamples_(blockFrames * channelCount),
      block_(blockFrames * channelCount) {}

ProcessingThread::~ProcessingThread() { stop(); }

bool ProcessingThread::start() {
    if (running_.exchange(true)) return false;
    thread_ = std::thread(&ProcessingThread::run, this);
    return true;
}

void ProcessingThread::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

void ProcessingThread::run() {
    pthread_setname_np(pthread_self(), "vr_inference");

    while (running_.load(std::memory_order_relaxed)) {
        if (input_->readAvailable() < blockSamples_) {
            // Not enough captured audio yet; back off briefly. The rings absorb
            // this latency, and a short sleep keeps the core mostly idle.
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }

        input_->read(block_.data(), blockSamples_);
        processor_->process(block_.data(), blockFrames_, channelCount_);

        // Push the processed block out, spinning if the output ring is briefly
        // full (the audio callback drains it every burst).
        size_t off = 0;
        while (off < blockSamples_ && running_.load(std::memory_order_relaxed)) {
            off += output_->write(block_.data() + off, blockSamples_ - off);
            if (off < blockSamples_) std::this_thread::yield();
        }
        framesProcessed_.fetch_add(blockFrames_, std::memory_order_relaxed);
    }
}

}  // namespace vocalremover
