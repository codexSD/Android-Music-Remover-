#ifndef VOCALREMOVER_SEPARATOR_H
#define VOCALREMOVER_SEPARATOR_H

#include <complex>
#include <cstddef>

namespace vocalremover {

// Maps the complex spectrum of one analysis frame to the separated (vocals)
// spectrum. This is the model boundary: SpectrogramProcessor owns the STFT and
// overlap-add and calls a Separator per frame, so the DSP plumbing is testable
// without any model and the model implementation (ONNX) stays isolated.
class Separator {
public:
    virtual ~Separator() = default;

    // Called on the processing thread before the first process() with the
    // stream sample rate and the number of frequency bins (fftSize/2 + 1).
    virtual void prepare(int sampleRate, size_t numBins) = 0;

    // Write the separated spectrum for one frame. `in` and `out` are numBins
    // long and may alias. Must be allocation- and lock-free.
    virtual void process(const std::complex<float>* in, std::complex<float>* out,
                         size_t numBins) = 0;

    virtual const char* name() const = 0;
};

// Pass-through separator: out == in. Used to validate the STFT/overlap-add
// reconstruction independently of any model.
class IdentitySeparator : public Separator {
public:
    void prepare(int /*sampleRate*/, size_t /*numBins*/) override {}
    void process(const std::complex<float>* in, std::complex<float>* out,
                 size_t numBins) override {
        if (in != out) {
            for (size_t b = 0; b < numBins; ++b) out[b] = in[b];
        }
    }
    const char* name() const override { return "identity"; }
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_SEPARATOR_H
