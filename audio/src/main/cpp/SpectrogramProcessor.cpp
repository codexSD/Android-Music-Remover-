#include "SpectrogramProcessor.h"

#include <algorithm>
#include <cstring>

#include "AudioFormat.h"

namespace vocalremover {

namespace {
size_t nextPow2(size_t v) {
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}
}  // namespace

SpectrogramProcessor::SpectrogramProcessor(size_t fftSize, size_t hop,
                                           std::unique_ptr<Separator> separator)
    : stft_(fftSize, hop),
      separator_(std::move(separator)),
      name_(std::string("spectrogram(") + separator_->name() + ")"),
      analysisBuf_(fftSize, 0.0f),
      synth_(fftSize, 0.0f),
      ola_(fftSize, 0.0f),
      specIn_(stft_.numBins()),
      specOut_(stft_.numBins()) {}

bool SpectrogramProcessor::init(int sampleRate, int channelCount,
                                const EngineConfig& /*config*/,
                                int& latencySamplesOut) {
    channelCount_ = channelCount;
    olaNorm_ = stft_.windowOlaNorm();
    primed_ = false;
    std::fill(analysisBuf_.begin(), analysisBuf_.end(), 0.0f);
    std::fill(ola_.begin(), ola_.end(), 0.0f);

    // Generously sized so a single process() block plus a full window never
    // overflows; sized once here so process() never allocates.
    const size_t cap = nextPow2(stft_.fftSize() * 8);
    inFifo_ = std::make_unique<SpscRingBuffer<float>>(cap);
    outFifo_ = std::make_unique<SpscRingBuffer<float>>(cap);

    separator_->prepare(sampleRate, stft_.numBins());
    latencySamplesOut = static_cast<int>(stft_.fftSize());
    return true;
}

EngineCapabilities SpectrogramProcessor::capabilities() const {
    return EngineCapabilities{name_.c_str(), 0, static_cast<int>(stft_.fftSize()),
                              /*needsStereo=*/false};
}

void SpectrogramProcessor::analyzeEmit() {
    stft_.analyze(analysisBuf_.data(), specIn_.data());
    separator_->process(specIn_.data(), specOut_.data(), stft_.numBins());
    stft_.synthesize(specOut_.data(), synth_.data());

    const size_t fftSize = stft_.fftSize();
    const size_t hop = stft_.hop();
    for (size_t n = 0; n < fftSize; ++n) ola_[n] += synth_[n];

    // The first `hop` samples are finalized: normalize and push to the output.
    float emit[1];
    for (size_t n = 0; n < hop; ++n) {
        emit[0] = ola_[n] / olaNorm_;
        outFifo_->write(emit, 1);
    }
    // Slide the accumulator left by hop and zero the exposed tail.
    std::memmove(ola_.data(), ola_.data() + hop, (fftSize - hop) * sizeof(float));
    std::fill(ola_.begin() + (fftSize - hop), ola_.end(), 0.0f);
}

void SpectrogramProcessor::process(float* buffer, size_t frames, int channelCount) {
    const size_t fftSize = stft_.fftSize();
    const size_t hop = stft_.hop();

    if (mono_.size() < frames) mono_.resize(frames);

    // Downmix this block to mono and enqueue it.
    if (channelCount == 2) {
        audioformat::stereoToMono(buffer, mono_.data(), frames);
    } else {
        std::memcpy(mono_.data(), buffer, frames * sizeof(float));
    }
    inFifo_->write(mono_.data(), frames);

    // Consume as many hop-sized advances as the input allows.
    while (true) {
        if (!primed_) {
            if (inFifo_->readAvailable() < fftSize) break;
            inFifo_->read(analysisBuf_.data(), fftSize);
            primed_ = true;
        } else {
            if (inFifo_->readAvailable() < hop) break;
            std::memmove(analysisBuf_.data(), analysisBuf_.data() + hop,
                         (fftSize - hop) * sizeof(float));
            inFifo_->read(analysisBuf_.data() + (fftSize - hop), hop);
        }
        analyzeEmit();
    }

    // Emit `frames` mono output samples, padding with silence until the
    // pipeline has filled its latency.
    for (size_t i = 0; i < frames; ++i) {
        float s = 0.0f;
        outFifo_->read(&s, 1);  // leaves s = 0 on underrun
        if (channelCount == 2) {
            buffer[2 * i] = s;
            buffer[2 * i + 1] = s;
        } else {
            buffer[i] = s;
        }
    }
}

}  // namespace vocalremover
