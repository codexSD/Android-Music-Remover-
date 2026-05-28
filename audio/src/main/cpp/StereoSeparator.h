#ifndef VOCALREMOVER_STEREOSEPARATOR_H
#define VOCALREMOVER_STEREOSEPARATOR_H

#include <complex>
#include <cstddef>

namespace vocalremover {

// Like Separator, but sees the LEFT and RIGHT spectra of one analysis frame
// separately. This is required for stereo-aware techniques (center extraction
// compares L and R per bin); the mono Separator cannot express it because the
// pipeline downmixes before reaching it.
class StereoSeparator {
public:
    virtual ~StereoSeparator() = default;

    virtual void prepare(int sampleRate, size_t numBins) = 0;

    // Per-frame: L/R input spectra -> L/R output spectra. `*In` and `*Out` are
    // numBins long; in/out may alias. Must be allocation- and lock-free.
    virtual void process(const std::complex<float>* lIn, const std::complex<float>* rIn,
                         std::complex<float>* lOut, std::complex<float>* rOut,
                         size_t numBins) = 0;

    virtual const char* name() const = 0;
};

// Pass-through stereo separator: out == in for both channels. Used to validate
// the stereo STFT / overlap-add reconstruction independently of any masking.
class StereoIdentitySeparator : public StereoSeparator {
public:
    void prepare(int /*sampleRate*/, size_t /*numBins*/) override {}
    void process(const std::complex<float>* lIn, const std::complex<float>* rIn,
                 std::complex<float>* lOut, std::complex<float>* rOut,
                 size_t numBins) override {
        for (size_t b = 0; b < numBins; ++b) {
            lOut[b] = lIn[b];
            rOut[b] = rIn[b];
        }
    }
    const char* name() const override { return "stereo-identity"; }
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_STEREOSEPARATOR_H
