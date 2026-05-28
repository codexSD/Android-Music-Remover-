#include "ProcessingThread.h"

#include <pthread.h>

#include <chrono>

#include "DeadlineMonitor.h"
#include "Log.h"

namespace vocalremover {

namespace {
// A block is "late" if processing it takes more than this fraction of the time
// the block represents; that many consecutive late blocks trips the monitor.
constexpr double kOverrunRatio = 0.9;
constexpr int kOverrunWindow = 32;  // ~0.3s of sustained overruns at 512/48k
}  // namespace

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

bool ProcessingThread::start(int sampleRate) {
    if (running_.exchange(true)) return false;
    thread_ = std::thread(&ProcessingThread::run, this, sampleRate);
    return true;
}

void ProcessingThread::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

void ProcessingThread::run(int sampleRate) {
    pthread_setname_np(pthread_self(), "vr_inference");

    // Wall-clock time one block of audio represents; the engine must process a
    // block in less than this on average or the output ring will starve.
    const uint64_t budgetNs =
        sampleRate > 0
            ? static_cast<uint64_t>(blockFrames_) * 1'000'000'000ULL / sampleRate
            : 0;
    DeadlineMonitor monitor(budgetNs, kOverrunRatio, kOverrunWindow);

    while (running_.load(std::memory_order_relaxed)) {
        if (input_->readAvailable() < blockSamples_) {
            // Not enough captured audio yet; back off briefly. The rings absorb
            // this latency, and a short sleep keeps the core mostly idle.
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }

        input_->read(block_.data(), blockSamples_);

        const auto t0 = std::chrono::steady_clock::now();
        processor_->process(block_.data(), blockFrames_, channelCount_);
        const auto t1 = std::chrono::steady_clock::now();
        const uint64_t elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        if (budgetNs > 0 && monitor.record(elapsedNs)) {
            deadlineViolations_.fetch_add(1, std::memory_order_relaxed);
        }

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
