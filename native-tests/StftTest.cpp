#include "Stft.h"

#include <cmath>
#include <complex>
#include <vector>

#include "TinyTest.h"

using vocalremover::Stft;
using cf = std::complex<float>;

TEST(Stft, HannWindowEndpointsAndPeak) {
    Stft stft(256, 64);
    const auto& w = stft.window();
    CHECK_NEAR(w.front(), 0.0f, 1e-6);
    CHECK_NEAR(w[128], 1.0f, 1e-5);  // peak at the centre
    CHECK_EQ(stft.numBins(), static_cast<size_t>(129));
}

TEST(Stft, HannQuarterHopOlaNormIsThreeHalves) {
    Stft stft(256, 64);  // hop = fftSize / 4
    CHECK_NEAR(stft.windowOlaNorm(), 1.5f, 1e-5);
}

TEST(Stft, SingleFrameMagnitudePeakAtToneBin) {
    const size_t n = 512;
    Stft stft(n, n / 4);
    std::vector<float> frame(n);
    const size_t toneBin = 20;
    for (size_t i = 0; i < n; ++i) {
        frame[i] = std::cos(2.0f * static_cast<float>(M_PI) * toneBin * i / n);
    }
    std::vector<cf> spec(stft.numBins());
    stft.analyze(frame.data(), spec.data());

    size_t peak = 0;
    float peakMag = -1.0f;
    for (size_t b = 0; b < stft.numBins(); ++b) {
        const float m = std::abs(spec[b]);
        if (m > peakMag) { peakMag = m; peak = b; }
    }
    CHECK_EQ(peak, toneBin);
}

// The core guarantee: analysis -> (identity) -> synthesis with overlap-add and
// the right normalization reconstructs the input in the steady-state region.
TEST(Stft, OverlapAddReconstructsSignal) {
    const size_t fftSize = 256;
    const size_t hop = fftSize / 4;
    Stft stft(fftSize, hop);
    const float norm = stft.windowOlaNorm();

    const size_t len = 4096;
    std::vector<float> input(len);
    for (size_t i = 0; i < len; ++i) {
        input[i] = 0.6f * std::sin(0.05f * i) + 0.3f * std::sin(0.21f * i + 0.7f);
    }

    std::vector<float> output(len, 0.0f);
    std::vector<cf> spec(stft.numBins());
    std::vector<float> synth(fftSize);

    for (size_t start = 0; start + fftSize <= len; start += hop) {
        stft.analyze(input.data() + start, spec.data());  // identity in between
        stft.synthesize(spec.data(), synth.data());
        for (size_t n = 0; n < fftSize; ++n) output[start + n] += synth[n];
    }

    // Compare in the steady-state interior, where every overlapping frame is
    // present, after dividing out the overlap-added squared window.
    int failures = 0;
    for (size_t i = fftSize; i < len - fftSize; ++i) {
        if (std::fabs(output[i] / norm - input[i]) > 1e-3f) ++failures;
    }
    CHECK_EQ(failures, 0);
}
