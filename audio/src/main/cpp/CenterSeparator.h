#ifndef VOCALREMOVER_CENTERSEPARATOR_H
#define VOCALREMOVER_CENTERSEPARATOR_H

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>

#include "StereoSeparator.h"

namespace vocalremover {

// DSP center-extraction separator. In most mixes the lead vocal is panned to the
// centre, so it has near-equal magnitude in L and R, while instruments are spread
// across the stereo field. Per frequency bin we estimate how "centred" the energy
// is and keep centred content, attenuating the rest.
//
// This needs no model file, so it is the default engine. It is pure math and is
// unit tested on the host.
//
// Per bin, with complex L and R:
//   coherence  sim = 2|L||R| / (|L|^2 + |R|^2 + eps)   in [0, 1]
//      -> 1 when |L| == |R| (centred), -> 0 when energy is on one side (panned)
//   mask       m   = clamp(sim^sharpness, floor, 1)
//   output     Lout = L * m,  Rout = R * m             (phase preserved)
class CenterExtractSeparator : public StereoSeparator {
public:
    explicit CenterExtractSeparator(float sharpness = 2.0f, float floor = 0.0f)
        : sharpness_(sharpness < 1.0f ? 1.0f : sharpness),
          floor_(std::clamp(floor, 0.0f, 1.0f)) {}

    void prepare(int /*sampleRate*/, size_t /*numBins*/) override {}

    void process(const std::complex<float>* lIn, const std::complex<float>* rIn,
                 std::complex<float>* lOut, std::complex<float>* rOut,
                 size_t numBins) override {
        constexpr float kEps = 1e-12f;
        for (size_t b = 0; b < numBins; ++b) {
            const float lm = std::abs(lIn[b]);
            const float rm = std::abs(rIn[b]);
            const float denom = lm * lm + rm * rm + kEps;
            const float sim = (2.0f * lm * rm) / denom;  // in [0, 1]
            float m = std::pow(sim, sharpness_);
            if (m < floor_) m = floor_;
            lOut[b] = lIn[b] * m;
            rOut[b] = rIn[b] * m;
        }
    }

    const char* name() const override { return "center"; }

private:
    float sharpness_;
    float floor_;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_CENTERSEPARATOR_H
