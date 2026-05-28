#include "StereoSpectrogramProcessor.h"

#include <algorithm>
#include <cstring>

namespace vocalremover {

namespace {
size_t nextPow2(size_t v) {
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

void initChannel(size_t fftSize, size_t numBins, size_t cap,
                 std::vector<float>& analysisBuf, std::vector<float>& synth,
                 std::vector<float>& ola, std::vector<std::complex<float>>& specIn,
                 std::vector<std::complex<float>>& specOut,
                 std::unique_ptr<SpscRingBuffer<float>>& inFifo,
                 std::unique_ptr<SpscRingBuffer<float>>& outFifo) {
    analysisBuf.assign(fftSize, 0.0f);
    synth.assign(fftSize, 0.0f);
    ola.assign(fftSize, 0.0f);
    specIn.assign(numBins, std::complex<float>(0.0f, 0.0f));
    specOut.assign(numBins, std::complex<float>(0.0f, 0.0f));
    inFifo = std::make_unique<SpscRingBuffer<float>>(cap);
    outFifo = std::make_unique<SpscRingBuffer<float>>(cap);
}
}  // namespace

StereoSpectrogramProcessor::StereoSpectrogramProcessor(
    size_t fftSize, size_t hop, std::unique_ptr<StereoSeparator> separator)
    : stft_(fftSize, hop),
      separator_(std::move(separator)),
      name_(std::string("stereo-spectrogram(") + separator_->name() + ")") {}

bool StereoSpectrogramProcessor::init(int sampleRate, int /*channelCount*/,
                                      const EngineConfig& /*config*/,
                                      int& latencySamplesOut) {
    olaNorm_ = stft_.windowOlaNorm();
    primed_ = false;
    const size_t fftSize = stft_.fftSize();
    const size_t numBins = stft_.numBins();
    const size_t cap = nextPow2(fftSize * 8);

    initChannel(fftSize, numBins, cap, left_.analysisBuf, left_.synth, left_.ola,
                left_.specIn, left_.specOut, left_.inFifo, left_.outFifo);
    initChannel(fftSize, numBins, cap, right_.analysisBuf, right_.synth, right_.ola,
                right_.specIn, right_.specOut, right_.inFifo, right_.outFifo);

    separator_->prepare(sampleRate, numBins);
    latencySamplesOut = static_cast<int>(fftSize);
    return true;
}

EngineCapabilities StereoSpectrogramProcessor::capabilities() const {
    return EngineCapabilities{name_.c_str(), 0, static_cast<int>(stft_.fftSize()),
                              /*needsStereo=*/true};
}

void StereoSpectrogramProcessor::analyzeEmit() {
    stft_.analyze(left_.analysisBuf.data(), left_.specIn.data());
    stft_.analyze(right_.analysisBuf.data(), right_.specIn.data());

    separator_->process(left_.specIn.data(), right_.specIn.data(), left_.specOut.data(),
                        right_.specOut.data(), stft_.numBins());

    stft_.synthesize(left_.specOut.data(), left_.synth.data());
    stft_.synthesize(right_.specOut.data(), right_.synth.data());

    const size_t fftSize = stft_.fftSize();
    const size_t hop = stft_.hop();
    for (size_t n = 0; n < fftSize; ++n) {
        left_.ola[n] += left_.synth[n];
        right_.ola[n] += right_.synth[n];
    }

    // The first `hop` samples of each channel are finalized; normalize and emit.
    for (size_t n = 0; n < hop; ++n) {
        float l = left_.ola[n] / olaNorm_;
        float r = right_.ola[n] / olaNorm_;
        left_.outFifo->write(&l, 1);
        right_.outFifo->write(&r, 1);
    }
    std::memmove(left_.ola.data(), left_.ola.data() + hop, (fftSize - hop) * sizeof(float));
    std::fill(left_.ola.begin() + (fftSize - hop), left_.ola.end(), 0.0f);
    std::memmove(right_.ola.data(), right_.ola.data() + hop, (fftSize - hop) * sizeof(float));
    std::fill(right_.ola.begin() + (fftSize - hop), right_.ola.end(), 0.0f);
}

void StereoSpectrogramProcessor::process(float* buffer, size_t frames, int channelCount) {
    const size_t fftSize = stft_.fftSize();
    const size_t hop = stft_.hop();

    if (scratchL_.size() < frames) {
        scratchL_.resize(frames);
        scratchR_.resize(frames);
    }

    // Deinterleave this block into per-channel scratch and enqueue. Mono input
    // is duplicated to both channels so the engine still functions.
    if (channelCount == 2) {
        for (size_t i = 0; i < frames; ++i) {
            scratchL_[i] = buffer[2 * i];
            scratchR_[i] = buffer[2 * i + 1];
        }
    } else {
        std::memcpy(scratchL_.data(), buffer, frames * sizeof(float));
        std::memcpy(scratchR_.data(), buffer, frames * sizeof(float));
    }
    left_.inFifo->write(scratchL_.data(), frames);
    right_.inFifo->write(scratchR_.data(), frames);

    // Consume as many hop-sized advances as the input allows (both channels move
    // in lockstep, so checking one FIFO is sufficient).
    while (true) {
        if (!primed_) {
            if (left_.inFifo->readAvailable() < fftSize) break;
            left_.inFifo->read(left_.analysisBuf.data(), fftSize);
            right_.inFifo->read(right_.analysisBuf.data(), fftSize);
            primed_ = true;
        } else {
            if (left_.inFifo->readAvailable() < hop) break;
            std::memmove(left_.analysisBuf.data(), left_.analysisBuf.data() + hop,
                         (fftSize - hop) * sizeof(float));
            std::memmove(right_.analysisBuf.data(), right_.analysisBuf.data() + hop,
                         (fftSize - hop) * sizeof(float));
            left_.inFifo->read(left_.analysisBuf.data() + (fftSize - hop), hop);
            right_.inFifo->read(right_.analysisBuf.data() + (fftSize - hop), hop);
        }
        analyzeEmit();
    }

    // Emit `frames` output frames, padding with silence until the pipeline has
    // filled its latency.
    for (size_t i = 0; i < frames; ++i) {
        float l = 0.0f;
        float r = 0.0f;
        left_.outFifo->read(&l, 1);
        right_.outFifo->read(&r, 1);
        if (channelCount == 2) {
            buffer[2 * i] = l;
            buffer[2 * i + 1] = r;
        } else {
            buffer[i] = 0.5f * (l + r);
        }
    }
}

}  // namespace vocalremover
