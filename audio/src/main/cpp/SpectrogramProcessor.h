#ifndef VOCALREMOVER_SPECTROGRAMPROCESSOR_H
#define VOCALREMOVER_SPECTROGRAMPROCESSOR_H

#include <complex>
#include <cstddef>
#include <memory>
#include <vector>

#include "AudioProcessor.h"
#include "RingBuffer.h"
#include "Separator.h"
#include "Stft.h"

namespace vocalremover {

// Streaming STFT-domain processor. Runs on the inference thread (never the
// audio callback). For each block of interleaved audio it:
//
//   stereo -> mono -> sliding-window analysis (Hann, hop) -> Separator
//          -> synthesis -> overlap-add -> mono -> stereo
//
// Output is delayed by ~fftSize samples (the algorithmic latency); during the
// initial fill it emits silence. Internally uses fixed-size buffers so process()
// does not allocate after prepare().
class SpectrogramProcessor : public AudioProcessor {
public:
    SpectrogramProcessor(size_t fftSize, size_t hop, std::unique_ptr<Separator> separator);

    bool init(int sampleRate, int channelCount, const EngineConfig& config,
              int& latencySamplesOut) override;
    EngineCapabilities capabilities() const override;
    void process(float* buffer, size_t frames, int channelCount) override;
    const char* name() const override { return name_.c_str(); }

    // Algorithmic latency in frames (one analysis window).
    size_t latencyFrames() const { return stft_.fftSize(); }

private:
    void analyzeEmit();  // analyze analysisBuf_, separate, synth, OLA, emit hop

    Stft stft_;
    std::unique_ptr<Separator> separator_;
    std::string name_;

    int channelCount_ = 2;
    float olaNorm_ = 1.0f;
    bool primed_ = false;

    std::vector<float> mono_;          // scratch for downmix of one block
    std::vector<float> analysisBuf_;   // sliding window, length fftSize
    std::vector<float> synth_;         // synthesis frame, length fftSize
    std::vector<float> ola_;           // overlap-add accumulator, length fftSize
    std::vector<std::complex<float>> specIn_;
    std::vector<std::complex<float>> specOut_;

    std::unique_ptr<SpscRingBuffer<float>> inFifo_;   // mono input queue
    std::unique_ptr<SpscRingBuffer<float>> outFifo_;  // mono output queue
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_SPECTROGRAMPROCESSOR_H
