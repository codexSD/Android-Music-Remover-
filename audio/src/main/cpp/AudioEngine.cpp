#include "AudioEngine.h"

#include "Log.h"
#include "PassthroughProcessor.h"

namespace vocalremover {

namespace {
// Number of frames requested per AudioRecord.read(). ~21ms at 48kHz; small
// enough to keep latency low, large enough to amortize the JNI call.
constexpr int kCaptureChunkFrames = 1024;

// Standing ring-buffer capacity target. 200ms absorbs scheduling jitter
// between the capture thread and the output callback without adding much
// latency (the buffer self-regulates near the capture chunk size).
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
                        int channelCount) {
    if (running_) {
        VR_LOGW("AudioEngine.start ignored: already running");
        return false;
    }
    sampleRate_ = sampleRate;
    channelCount_ = channelCount;

    ring_ = std::make_unique<SpscRingBuffer<float>>(
        ringCapacityFor(sampleRate, channelCount));
    processor_ = std::make_unique<PassthroughProcessor>();

    output_ = std::make_unique<OutputStream>(sampleRate, channelCount, ring_.get(),
                                             processor_.get());
    if (!output_->open()) {
        VR_LOGE("AudioEngine.start: output open failed");
        stop();
        return false;
    }

    reader_ = std::make_unique<AudioRecordReader>(env, audioRecord, channelCount,
                                                  kCaptureChunkFrames, ring_.get());

    if (!output_->start()) {
        VR_LOGE("AudioEngine.start: output start failed");
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
    if (reader_) {
        reader_->stop();
        reader_.reset();
    }
    if (output_) {
        output_->stop();
        output_->close();
        output_.reset();
    }
    processor_.reset();
    ring_.reset();
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
