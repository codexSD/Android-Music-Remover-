#include "AudioEngine.h"

#include "EngineRegistry.h"
#include "Log.h"

namespace vocalremover {

namespace {
// Number of frames requested per AudioRecord.read(). ~21ms at 48kHz.
constexpr int kCaptureChunkFrames = 1024;

// Processing-thread block size. ~10ms at 48kHz; small enough to keep latency
// low, large enough to amortize per-block overhead.
constexpr size_t kProcessBlockFrames = 512;

// Standing capacity per ring. 200ms absorbs scheduling jitter between the
// capture, processing, and output stages without adding much latency.
constexpr double kRingCapacitySeconds = 0.2;

size_t nextPowerOfTwo(size_t v) {
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}
}  // namespace

AudioEngine::~AudioEngine() { stop(); }

size_t AudioEngine::ringCapacityFor(int sampleRate, int channelCount) {
    const auto target = static_cast<size_t>(
        static_cast<double>(sampleRate) * channelCount * kRingCapacitySeconds);
    return nextPowerOfTwo(target);
}

bool AudioEngine::start(JNIEnv* env, jobject audioRecord, int sampleRate,
                        int channelCount, const std::string& engineId,
                        const EngineConfig& config, const uint8_t* modelData,
                        size_t modelLen) {
    if (running_) {
        VR_LOGW("AudioEngine.start ignored: already running");
        return false;
    }
    sampleRate_ = sampleRate;
    channelCount_ = channelCount;

    // Build the engine the Kotlin resolver chose. Selection lives in Kotlin; the
    // registry only constructs. A null/failed result is reported so the resolver
    // can fall back to the next engine.
    EngineRegistry registry;
    registerBuiltinEngines(registry, modelData, modelLen);
    processor_ = registry.create(engineId, config);
    if (!processor_) {
        VR_LOGE("AudioEngine.start: unknown or failed engine '%s'", engineId.c_str());
        stop();
        return false;
    }
    int latencySamples = 0;
    if (!processor_->init(sampleRate, channelCount, config, latencySamples)) {
        VR_LOGE("AudioEngine.start: engine '%s' init failed", engineId.c_str());
        stop();
        return false;
    }

    const size_t cap = ringCapacityFor(sampleRate, channelCount);
    ringIn_ = std::make_unique<SpscRingBuffer<float>>(cap);
    ringOut_ = std::make_unique<SpscRingBuffer<float>>(cap);

    output_ = std::make_unique<OutputStream>(sampleRate, channelCount, ringOut_.get());
    if (!output_->open()) {
        VR_LOGE("AudioEngine.start: output open failed");
        stop();
        return false;
    }

    processing_ = std::make_unique<ProcessingThread>(
        ringIn_.get(), ringOut_.get(), processor_.get(), channelCount,
        kProcessBlockFrames);

    reader_ = std::make_unique<AudioRecordReader>(env, audioRecord, channelCount,
                                                  kCaptureChunkFrames, ringIn_.get());

    if (!output_->start()) {
        VR_LOGE("AudioEngine.start: output start failed");
        stop();
        return false;
    }
    if (!processing_->start(sampleRate)) {
        VR_LOGE("AudioEngine.start: processing thread failed to start");
        stop();
        return false;
    }
    if (!reader_->start()) {
        VR_LOGE("AudioEngine.start: capture start failed");
        stop();
        return false;
    }

    running_ = true;
    VR_LOGI("AudioEngine started (engine=%s, processor=%s, latency=%d)",
            engineId.c_str(), processor_->name(), latencySamples);
    return true;
}

void AudioEngine::stop() {
    // Tear down in pipeline order: stop feeding, stop processing, stop draining.
    if (reader_) {
        reader_->stop();
        reader_.reset();
    }
    if (processing_) {
        processing_->stop();
        processing_.reset();
    }
    if (output_) {
        output_->stop();
        output_->close();
        output_.reset();
    }
    processor_.reset();
    ringIn_.reset();
    ringOut_.reset();
    running_ = false;
}

uint64_t AudioEngine::framesCaptured() const {
    return reader_ ? reader_->framesCaptured() : 0;
}

uint64_t AudioEngine::framesDropped() const {
    return reader_ ? reader_->framesDropped() : 0;
}

uint64_t AudioEngine::underrunFrames() const {
    return output_ ? output_->underrunFrames() : 0;
}

uint64_t AudioEngine::deadlineViolations() const {
    return processing_ ? processing_->deadlineViolations() : 0;
}

float AudioEngine::captureRms() const {
    return reader_ ? reader_->captureRms() : 0.0f;
}

}  // namespace vocalremover
