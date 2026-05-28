#ifndef VOCALREMOVER_STEREOSPECTROGRAMPROCESSOR_H
#define VOCALREMOVER_STEREOSPECTROGRAMPROCESSOR_H

#include <complex>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "AudioProcessor.h"
#include "RingBuffer.h"
#include "Stft.h"
#include "StereoSeparator.h"

namespace vocalremover {

// Stereo-preserving STFT-domain processor. Mirrors SpectrogramProcessor's
// FIFO/prime/slide/overlap-add scaffolding but keeps the LEFT and RIGHT channels
// distinct end to end, so a StereoSeparator (e.g. center extraction) can compare
// the two channels per bin. Runs on the processing thread, never the audio
// callback. Output is delayed by ~fftSize samples; process() does not allocate
// after init().
class StereoSpectrogramProcessor : public AudioProcessor {
public:
    StereoSpectrogramProcessor(size_t fftSize, size_t hop,
                               std::unique_ptr<StereoSeparator> separator);

    bool init(int sampleRate, int channelCount, const EngineConfig& config,
              int& latencySamplesOut) override;
    EngineCapabilities capabilities() const override;
    void process(float* buffer, size_t frames, int channelCount) override;
    const char* name() const override { return name_.c_str(); }

    size_t latencyFrames() const { return stft_.fftSize(); }

private:
    // One channel's sliding-window analysis + overlap-add state.
    struct Channel {
        std::vector<float> analysisBuf;  // length fftSize
        std::vector<float> synth;        // length fftSize
        std::vector<float> ola;          // length fftSize
        std::vector<std::complex<float>> specIn;
        std::vector<std::complex<float>> specOut;
        std::unique_ptr<SpscRingBuffer<float>> inFifo;
        std::unique_ptr<SpscRingBuffer<float>> outFifo;
    };

    void analyzeEmit();  // analyze L+R, separate, synth, OLA, emit hop on both

    Stft stft_;
    std::unique_ptr<StereoSeparator> separator_;
    std::string name_;

    float olaNorm_ = 1.0f;
    bool primed_ = false;

    Channel left_;
    Channel right_;
    std::vector<float> scratchL_;  // deinterleave scratch for one block
    std::vector<float> scratchR_;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_STEREOSPECTROGRAMPROCESSOR_H
