#include "SpectrogramProcessor.h"

#include <cmath>
#include <memory>
#include <vector>

#include "Separator.h"
#include "TinyTest.h"

using vocalremover::IdentitySeparator;
using vocalremover::Separator;
using vocalremover::SpectrogramProcessor;

namespace {

// Multiplies every bin by a constant gain, to prove the separator output is
// actually applied through synthesis/overlap-add.
class ScaleSeparator : public Separator {
public:
    explicit ScaleSeparator(float g) : gain_(g) {}
    void prepare(int, size_t) override {}
    void process(const std::complex<float>* in, std::complex<float>* out,
                 size_t numBins) override {
        for (size_t b = 0; b < numBins; ++b) out[b] = in[b] * gain_;
    }
    const char* name() const override { return "scale"; }

private:
    float gain_;
};

std::vector<float> makeStereoSignal(size_t frames) {
    std::vector<float> buf(frames * 2);
    for (size_t i = 0; i < frames; ++i) {
        const float s = 0.5f * std::sin(0.043f * i) + 0.25f * std::sin(0.17f * i + 0.4f);
        buf[2 * i] = s;
        buf[2 * i + 1] = s;  // identical L/R so the mono downmix is lossless
    }
    return buf;
}

// Runs the signal through the processor block by block (mimicking the inference
// thread draining the capture ring) and returns the interleaved stereo output.
std::vector<float> run(SpectrogramProcessor& proc, std::vector<float> stereo,
                       size_t blockFrames) {
    proc.prepare(48000, 2);
    const size_t totalFrames = stereo.size() / 2;
    for (size_t start = 0; start < totalFrames; start += blockFrames) {
        const size_t n = std::min(blockFrames, totalFrames - start);
        proc.process(stereo.data() + start * 2, n, 2);
    }
    return stereo;  // processed in place
}

// Best-delay residual: the processor introduces algorithmic latency, so we
// search a small delay range for the alignment that minimizes error and return
// the relative RMS residual over a steady-state interior window.
double relativeResidual(const std::vector<float>& out, const std::vector<float>& ref,
                        float expectedGain, size_t maxDelay) {
    const size_t k0 = 2000, k1 = 3500;  // interior, away from start/end
    double best = 1e30;
    for (size_t d = 0; d <= maxDelay; ++d) {
        double err = 0.0;
        for (size_t k = k0; k < k1; ++k) {
            const double o = out[(d + k) * 2];        // left channel
            const double r = ref[k * 2] * expectedGain;
            err += (o - r) * (o - r);
        }
        best = std::min(best, err);
    }
    double energy = 0.0;
    for (size_t k = k0; k < k1; ++k) {
        const double r = ref[k * 2] * expectedGain;
        energy += r * r;
    }
    return std::sqrt(best / energy);
}

}  // namespace

TEST(SpectrogramProcessor, IdentityReconstructsThroughOverlapAdd) {
    const auto ref = makeStereoSignal(6000);
    SpectrogramProcessor proc(256, 64, std::make_unique<IdentitySeparator>());
    const auto out = run(proc, ref, /*blockFrames=*/100);
    // Identity separator: output should reconstruct the input (gain 1.0).
    CHECK(relativeResidual(out, ref, 1.0f, /*maxDelay=*/600) < 0.02);
}

TEST(SpectrogramProcessor, ReportsWindowLatency) {
    SpectrogramProcessor proc(256, 64, std::make_unique<IdentitySeparator>());
    CHECK_EQ(proc.latencyFrames(), static_cast<size_t>(256));
}

TEST(SpectrogramProcessor, SeparatorGainIsApplied) {
    const auto ref = makeStereoSignal(6000);
    SpectrogramProcessor proc(256, 64, std::make_unique<ScaleSeparator>(0.5f));
    const auto out = run(proc, ref, /*blockFrames=*/128);
    // Output should match the input scaled by 0.5.
    CHECK(relativeResidual(out, ref, 0.5f, /*maxDelay=*/600) < 0.03);
}

TEST(SpectrogramProcessor, HandlesBlockSizeLargerThanWindow) {
    const auto ref = makeStereoSignal(6000);
    SpectrogramProcessor proc(256, 64, std::make_unique<IdentitySeparator>());
    const auto out = run(proc, ref, /*blockFrames=*/1024);  // block > fftSize
    CHECK(relativeResidual(out, ref, 1.0f, /*maxDelay=*/600) < 0.02);
}
