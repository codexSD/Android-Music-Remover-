#include "AudioEngine.h"

#include "Log.h"
#include "PassthroughProcessor.h"
#include "SpectrogramProcessor.h"

#ifdef USE_ONNXRUNTIME
#include "OnnxSeparator.h"
#endif

namespace vocalremover {

namespace {
// Number of frames requested per AudioRecord.read(). ~21ms at 48kHz.
constexpr int kCaptureChunkFrames = 1024;

// Processing-thread block size. ~10ms at 48kHz; small enough to keep latency
// low, large enough to amortize per-block overhead.
constexpr size_t kProcessBlockFrames = 512;

// STFT parameters for the separator (Phase 1 plan 2.2).
constexpr size_t kFftSize = 2048;
constexpr size_t kHop = 512;

// Standing capacity per ring. 200ms absorbs scheduling jitter between the
// capture, processing, and output stages without adding much latency.
constexpr double kRingCapacitySeconds = 0.2;

size_t nextPowerOfTwo(size_t v) {
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

std::unique_ptr<AudioProcessor> createProcessor(const uint8_t* modelData,
                                                size_t modelLen) {
    if (modelData != nullptr && modelLen > 0) {
#ifdef USE_ONNXRUNTIME
        auto separator = OnnxSeparator::create(modelData, modelLen);
        if (separator) {
            VR_LOGI("Using ONNX spectrogram separator");
            return std::make_unique<SpectrogramProcessor>(kFftSize, kHop,
                                                          std::move(separator));
        }
        VR_LOGW("ONNX model failed to load; falling back to passthrough");
#else
        VR_LOGW("Model supplied but ONNX support not compiled in; passthrough");
#endif
    }
    return std::make_unique<PassthroughProcessor>();
}
}  // namespace

AudioEngine::~AudioEngine() { stop(); }

size_t AudioEngine::ringCapacityFor(int sampleRate, int channelCount) {
    const auto target = static_cast<size_t>(
        static_cast<double>(sampleRate) * channelCount * kRingCapacitySeconds);
    return nextPowerOfTwo(target);
}

bool AudioEngine::start(JNIEnv* env, jobject audioRecord, int sampleRate,
                        int channelCount, const uint8_t* modelData, size_t modelLen) {
    if (running_) {
        VR_LOGW("AudioEngine.start ignored: already running");
        return false;
    }
    sampleRate_ = sampleRate;
    channelCount_ = channelCount;

    const size_t cap = ringCapacityFor(sampleRate, channelCount);
    ringIn_ = std::make_unique<SpscRingBuffer<float>>(cap);
    ringOut_ = std::make_unique<SpscRingBuffer<float>>(cap);

    processor_ = createProcessor(modelData, modelLen);
    processor_->prepare(sampleRate, channelCount);

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
    if (!processing_->start()) {
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
    VR_LOGI("AudioEngine started (processor=%s)", processor_->name());
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

}  // namespace vocalremover
