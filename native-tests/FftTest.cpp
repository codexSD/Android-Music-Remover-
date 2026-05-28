#include "Fft.h"

#include <cmath>
#include <complex>
#include <vector>

#include "TinyTest.h"

using vocalremover::Fft;
using cf = std::complex<float>;

TEST(Fft, ImpulseHasFlatSpectrum) {
    const size_t n = 16;
    Fft fft(n);
    std::vector<cf> in(n, cf(0, 0)), out(n);
    in[0] = cf(1.0f, 0.0f);
    fft.forward(in.data(), out.data());
    for (size_t k = 0; k < n; ++k) {
        CHECK_NEAR(out[k].real(), 1.0f, 1e-4);
        CHECK_NEAR(out[k].imag(), 0.0f, 1e-4);
    }
}

TEST(Fft, DcInputConcentratesAtBinZero) {
    const size_t n = 8;
    Fft fft(n);
    std::vector<cf> in(n, cf(1.0f, 0.0f)), out(n);
    fft.forward(in.data(), out.data());
    CHECK_NEAR(out[0].real(), static_cast<float>(n), 1e-3);
    for (size_t k = 1; k < n; ++k) {
        CHECK_NEAR(std::abs(out[k]), 0.0f, 1e-3);
    }
}

TEST(Fft, SingleBinSinusoid) {
    const size_t n = 32;
    Fft fft(n);
    std::vector<cf> in(n), out(n);
    // A complex exponential at bin 3 should produce a single spike at bin 3.
    for (size_t i = 0; i < n; ++i) {
        const double a = 2.0 * M_PI * 3.0 * i / n;
        in[i] = cf(static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)));
    }
    fft.forward(in.data(), out.data());
    for (size_t k = 0; k < n; ++k) {
        const float mag = std::abs(out[k]);
        if (k == 3) {
            CHECK_NEAR(mag, static_cast<float>(n), 1e-2);
        } else {
            CHECK_NEAR(mag, 0.0f, 1e-2);
        }
    }
}

TEST(Fft, ForwardInverseRoundTrip) {
    const size_t n = 64;
    Fft fft(n);
    std::vector<cf> in(n), freq(n), back(n);
    for (size_t i = 0; i < n; ++i) {
        in[i] = cf(std::sin(0.3f * i) + 0.2f * std::cos(1.1f * i), 0.0f);
    }
    fft.forward(in.data(), freq.data());
    fft.inverse(freq.data(), back.data());
    for (size_t i = 0; i < n; ++i) {
        CHECK_NEAR(back[i].real(), in[i].real(), 1e-4);
        CHECK_NEAR(back[i].imag(), 0.0f, 1e-4);
    }
}
