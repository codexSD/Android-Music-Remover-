#include "CenterSeparator.h"

#include <cmath>
#include <complex>
#include <vector>

#include "TinyTest.h"

using cf = std::complex<float>;
using vocalremover::CenterExtractSeparator;

namespace {
// Magnitude of the per-bin mask the separator effectively applies, recovered as
// |out| / |in| on the left channel.
float maskOf(CenterExtractSeparator& sep, cf l, cf r) {
    cf li[1] = {l}, ri[1] = {r}, lo[1], ro[1];
    sep.process(li, ri, lo, ro, 1);
    const float lm = std::abs(l);
    return lm > 0 ? std::abs(lo[0]) / lm : 0.0f;
}
}  // namespace

TEST(CenterSeparator, CenteredBinPreserved) {
    CenterExtractSeparator sep(2.0f, 0.0f);
    // Identical L and R == perfectly centered -> mask ~1.
    const float m = maskOf(sep, cf(0.7f, 0.1f), cf(0.7f, 0.1f));
    CHECK_NEAR(m, 1.0f, 1e-4);
}

TEST(CenterSeparator, HardPannedBinSuppressed) {
    CenterExtractSeparator sep(2.0f, 0.0f);
    // Energy only in L -> coherence ~0 -> mask ~0.
    const float m = maskOf(sep, cf(0.9f, 0.0f), cf(0.0f, 0.0f));
    CHECK(m < 1e-3f);
}

TEST(CenterSeparator, PartiallyPannedIsAttenuated) {
    CenterExtractSeparator sep(2.0f, 0.0f);
    // |L| = 1, |R| = 0.5 -> sim = 2*0.5/(1.25) = 0.8 -> mask = 0.8^2 = 0.64.
    const float m = maskOf(sep, cf(1.0f, 0.0f), cf(0.5f, 0.0f));
    CHECK_NEAR(m, 0.64f, 1e-3);
}

TEST(CenterSeparator, HigherSharpnessRejectsMoreOffCenter) {
    CenterExtractSeparator soft(1.0f, 0.0f);
    CenterExtractSeparator hard(4.0f, 0.0f);
    const cf l(1.0f, 0.0f), r(0.5f, 0.0f);  // partially panned
    CHECK(maskOf(hard, l, r) < maskOf(soft, l, r));
}

TEST(CenterSeparator, FloorIsRespected) {
    CenterExtractSeparator sep(2.0f, 0.25f);
    // Hard-panned would be ~0, but the floor clamps the mask up to 0.25.
    const float m = maskOf(sep, cf(0.9f, 0.0f), cf(0.0f, 0.0f));
    CHECK_NEAR(m, 0.25f, 1e-3);
}

TEST(CenterSeparator, SilenceProducesNoNaN) {
    CenterExtractSeparator sep(2.0f, 0.0f);
    cf li[1] = {cf(0, 0)}, ri[1] = {cf(0, 0)}, lo[1], ro[1];
    sep.process(li, ri, lo, ro, 1);
    CHECK(std::isfinite(lo[0].real()) && std::isfinite(lo[0].imag()));
    CHECK(std::isfinite(ro[0].real()) && std::isfinite(ro[0].imag()));
}

TEST(CenterSeparator, PreservesPhase) {
    CenterExtractSeparator sep(2.0f, 0.0f);
    // Centered but with a phase: output should be input scaled by a real mask,
    // so the argument is unchanged.
    const cf l(0.3f, 0.4f), r(0.3f, 0.4f);
    cf li[1] = {l}, ri[1] = {r}, lo[1], ro[1];
    sep.process(li, ri, lo, ro, 1);
    CHECK_NEAR(std::arg(lo[0]), std::arg(l), 1e-4);
}
