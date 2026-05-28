#include "StereoSpectrogramProcessor.h"

#include <cmath>
#include <memory>
#include <vector>

#include "CenterSeparator.h"
#include "EngineConfig.h"
#include "StereoSeparator.h"
#include "TinyTest.h"

using vocalremover::CenterExtractSeparator;
using vocalremover::StereoIdentitySeparator;
using vocalremover::StereoSpectrogramProcessor;

namespace {

std::vector<float> run(StereoSpectrogramProcessor& proc, std::vector<float> stereo,
                       size_t blockFrames) {
    int latency = -1;
    proc.init(48000, 2, vocalremover::EngineConfig{}, latency);
    const size_t totalFrames = stereo.size() / 2;
    for (size_t start = 0; start < totalFrames; start += blockFrames) {
        const size_t n = std::min(blockFrames, totalFrames - start);
        proc.process(stereo.data() + start * 2, n, 2);
    }
    return stereo;  // processed in place
}

// Relative RMS residual between output channel `ch` and a reference mono signal,
// over a steady-state interior window, searching a small delay for alignment.
double residualVsMono(const std::vector<float>& out, int ch,
                      const std::vector<float>& mono, size_t maxDelay) {
    const size_t k0 = 2000, k1 = 3500;
    double best = 1e30;
    for (size_t d = 0; d <= maxDelay; ++d) {
        double err = 0.0;
        for (size_t k = k0; k < k1; ++k) {
            const double o = out[(d + k) * 2 + ch];
            const double r = mono[k];
            err += (o - r) * (o - r);
        }
        best = std::min(best, err);
    }
    double energy = 0.0;
    for (size_t k = k0; k < k1; ++k) energy += mono[k] * mono[k];
    return std::sqrt(best / energy);
}

}  // namespace

TEST(StereoSpectrogramProcessor, IdentityReconstructsBothChannels) {
    const size_t frames = 6000;
    std::vector<float> in(frames * 2);
    std::vector<float> refL(frames), refR(frames);
    for (size_t i = 0; i < frames; ++i) {
        refL[i] = 0.5f * std::sin(0.05f * i);
        refR[i] = 0.4f * std::sin(0.11f * i + 0.3f);  // different from L
        in[2 * i] = refL[i];
        in[2 * i + 1] = refR[i];
    }
    StereoSpectrogramProcessor proc(256, 64, std::make_unique<StereoIdentitySeparator>());
    const auto out = run(proc, in, 100);
    CHECK(residualVsMono(out, 0, refL, 600) < 0.02);  // left preserved
    CHECK(residualVsMono(out, 1, refR, 600) < 0.02);  // right preserved
}

TEST(StereoSpectrogramProcessor, CenterExtractionKeepsCenteredRemovesPanned) {
    const size_t frames = 6000;
    std::vector<float> in(frames * 2);
    std::vector<float> centered(frames), inputL(frames);
    for (size_t i = 0; i < frames; ++i) {
        const float c = 0.5f * std::sin(0.05f * i);          // centered (L==R)
        const float p = 0.45f * std::sin(0.33f * i + 0.2f);  // panned (L only)
        centered[i] = c;
        inputL[i] = c + p;
        in[2 * i] = c + p;  // L = centered + panned
        in[2 * i + 1] = c;  // R = centered only
    }
    StereoSpectrogramProcessor proc(256, 64,
                                    std::make_unique<CenterExtractSeparator>(2.0f, 0.0f));
    const auto out = run(proc, in, 128);

    // The output left channel should track the centered component...
    CHECK(residualVsMono(out, 0, centered, 600) < 0.15);
    // ...and the panned energy that was in the input L must be substantial,
    // proving the previous assertion isn't trivially satisfied.
    CHECK(residualVsMono(in, 0, centered, 0) > 0.3);
}
