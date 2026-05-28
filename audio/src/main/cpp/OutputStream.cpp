#include "OutputStream.h"

#include <cstring>

#include "Log.h"

namespace vocalremover {

OutputStream::OutputStream(int sampleRate, int channelCount,
                           SpscRingBuffer<float>* input)
    : sampleRate_(sampleRate), channelCount_(channelCount), input_(input) {}

OutputStream::~OutputStream() { close(); }

bool OutputStream::open() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(channelCount_)
        ->setSampleRate(sampleRate_)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
        ->setUsage(oboe::Usage::Media)
        ->setContentType(oboe::ContentType::Music)
        ->setDataCallback(this)
        ->setErrorCallback(this);

    const oboe::Result result = builder.openStream(stream_);
    if (result != oboe::Result::OK) {
        VR_LOGE("Failed to open output stream: %s", oboe::convertToText(result));
        return false;
    }

    // The device may grant a different rate/sharing mode than requested; adopt
    // whatever was actually negotiated for logging/diagnostics.
    sampleRate_ = stream_->getSampleRate();
    channelCount_ = stream_->getChannelCount();
    VR_LOGI("Output stream open: rate=%d ch=%d sharing=%d frames/burst=%d",
            sampleRate_, channelCount_,
            static_cast<int>(stream_->getSharingMode()),
            stream_->getFramesPerBurst());
    return true;
}

bool OutputStream::start() {
    if (!stream_) return false;
    const oboe::Result result = stream_->requestStart();
    if (result != oboe::Result::OK) {
        VR_LOGE("Failed to start output stream: %s", oboe::convertToText(result));
        return false;
    }
    return true;
}

void OutputStream::stop() {
    if (stream_) {
        stream_->requestStop();
    }
}

void OutputStream::close() {
    if (stream_) {
        stream_->stop();
        stream_->close();
        stream_.reset();
    }
}

oboe::DataCallbackResult OutputStream::onAudioReady(oboe::AudioStream* /*stream*/,
                                                    void* audioData,
                                                    int32_t numFrames) {
    auto* out = static_cast<float*>(audioData);
    const size_t requested = static_cast<size_t>(numFrames) * channelCount_;

    const size_t got = input_->read(out, requested);
    if (got < requested) {
        // Underrun: pad the tail with silence so we never emit stale samples.
        std::memset(out + got, 0, (requested - got) * sizeof(float));
        const uint64_t missingFrames = (requested - got) / channelCount_;
        underrunFrames_.fetch_add(missingFrames, std::memory_order_relaxed);
    }
    return oboe::DataCallbackResult::Continue;
}

void OutputStream::onErrorAfterClose(oboe::AudioStream* /*stream*/, oboe::Result error) {
    VR_LOGW("Output stream error after close: %s", oboe::convertToText(error));
    // Device routing changed (e.g. headphones unplugged). Reopen on the same
    // parameters; the ring buffer keeps filling in the meantime.
    if (open()) {
        start();
    }
}

}  // namespace vocalremover
