#ifndef VOCALREMOVER_STFT_H
#define VOCALREMOVER_STFT_H

#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

#include "Fft.h"

namespace vocalremover {

// Short-time Fourier transform helper: a periodic Hann window plus forward and
// inverse transforms over a single frame. Streaming overlap-add state lives in
// the caller (the separator processor); this class is the per-frame transform.
//
// analyze():   real frame[fftSize] -> windowed -> FFT -> numBins() complex bins
// synthesize(): numBins() complex bins -> IFFT -> windowed -> real frame[fftSize]
//
// Applying the window on both analysis and synthesis means correct overlap-add
// reconstruction requires dividing the summed output by the overlap-added
// squared window. For a Hann window and hop = fftSize/4 that constant is 1.5
// (see windowOlaNorm()).
class Stft {
public:
    Stft(size_t fftSize, size_t hop)
        : fftSize_(fftSize),
          hop_(hop),
          fft_(fftSize),
          window_(fftSize),
          scratchTime_(fftSize),
          scratchFreq_(fftSize) {
        for (size_t n = 0; n < fftSize_; ++n) {
            window_[n] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) *
                                                 static_cast<float>(n) /
                                                 static_cast<float>(fftSize_)));
        }
    }

    size_t fftSize() const { return fftSize_; }
    size_t hop() const { return hop_; }
    size_t numBins() const { return fftSize_ / 2 + 1; }
    const std::vector<float>& window() const { return window_; }

    // Overlap-add normalization constant for the configured Hann window and hop
    // (sum of squared window values across overlapping frames at one output
    // sample). Valid when the hop divides the window and satisfies COLA.
    float windowOlaNorm() const {
        float sum = 0.0f;
        for (size_t k = 0; k * hop_ < fftSize_; ++k) {
            const float w = window_[k * hop_];
            sum += w * w;
        }
        // The value is identical for every output sample under COLA, so summing
        // the window taps at one reference sample suffices.
        return sum;
    }

    void analyze(const float* frame, std::complex<float>* spectrum) const {
        for (size_t n = 0; n < fftSize_; ++n) {
            scratchTime_[n] = std::complex<float>(frame[n] * window_[n], 0.0f);
        }
        fft_.forward(scratchTime_.data(), scratchFreq_.data());
        for (size_t b = 0; b < numBins(); ++b) spectrum[b] = scratchFreq_[b];
    }

    void synthesize(const std::complex<float>* spectrum, float* frame) const {
        // Rebuild the full Hermitian-symmetric spectrum before the inverse FFT.
        scratchFreq_[0] = spectrum[0];
        for (size_t b = 1; b < numBins(); ++b) {
            scratchFreq_[b] = spectrum[b];
            scratchFreq_[fftSize_ - b] = std::conj(spectrum[b]);
        }
        fft_.inverse(scratchFreq_.data(), scratchTime_.data());
        for (size_t n = 0; n < fftSize_; ++n) {
            frame[n] = scratchTime_[n].real() * window_[n];
        }
    }

private:
    size_t fftSize_;
    size_t hop_;
    Fft fft_;
    std::vector<float> window_;
    mutable std::vector<std::complex<float>> scratchTime_;
    mutable std::vector<std::complex<float>> scratchFreq_;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_STFT_H
