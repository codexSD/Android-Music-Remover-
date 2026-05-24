#ifndef VOCALREMOVER_FFT_H
#define VOCALREMOVER_FFT_H

#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace vocalremover {

// Iterative radix-2 Cooley-Tukey FFT for power-of-two sizes. Twiddle factors
// and the bit-reversal permutation are precomputed in the constructor so that
// forward()/inverse() do no allocation and can run on the inference thread.
//
// Header-only and free of Android dependencies so it can be unit tested on a
// host. This is the transform under the STFT used by the separator.
class Fft {
public:
    explicit Fft(size_t n) : n_(n), bitrev_(n), twiddles_(n / 2) {
        // n must be a power of two.
        size_t bits = 0;
        while ((size_t{1} << bits) < n_) ++bits;
        for (size_t i = 0; i < n_; ++i) {
            size_t r = 0;
            for (size_t b = 0; b < bits; ++b) {
                if (i & (size_t{1} << b)) r |= (size_t{1} << (bits - 1 - b));
            }
            bitrev_[i] = r;
        }
        for (size_t k = 0; k < n_ / 2; ++k) {
            const double angle = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(n_);
            twiddles_[k] = std::complex<float>(static_cast<float>(std::cos(angle)),
                                               static_cast<float>(std::sin(angle)));
        }
    }

    size_t size() const { return n_; }

    void forward(const std::complex<float>* in, std::complex<float>* out) const {
        transform(in, out, /*inverse=*/false);
    }

    // Inverse transform, including the 1/N normalization.
    void inverse(const std::complex<float>* in, std::complex<float>* out) const {
        transform(in, out, /*inverse=*/true);
        const float scale = 1.0f / static_cast<float>(n_);
        for (size_t i = 0; i < n_; ++i) out[i] *= scale;
    }

private:
    void transform(const std::complex<float>* in, std::complex<float>* out,
                   bool inverse) const {
        for (size_t i = 0; i < n_; ++i) out[i] = in[bitrev_[i]];

        for (size_t len = 2; len <= n_; len <<= 1) {
            const size_t half = len / 2;
            const size_t step = n_ / len;
            for (size_t i = 0; i < n_; i += len) {
                for (size_t j = 0; j < half; ++j) {
                    std::complex<float> w = twiddles_[j * step];
                    if (inverse) w = std::conj(w);
                    const std::complex<float> u = out[i + j];
                    const std::complex<float> v = out[i + j + half] * w;
                    out[i + j] = u + v;
                    out[i + j + half] = u - v;
                }
            }
        }
    }

    size_t n_;
    std::vector<size_t> bitrev_;
    std::vector<std::complex<float>> twiddles_;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_FFT_H
