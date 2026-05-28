#ifndef VOCALREMOVER_ONNXSEPARATOR_H
#define VOCALREMOVER_ONNXSEPARATOR_H

#include <complex>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Separator.h"

// This translation unit is only compiled when USE_ONNXRUNTIME is defined (the
// Gradle build wires in the ONNX Runtime headers/lib). The rest of the engine
// depends only on the Separator interface, so host builds need no ONNX.
#include <onnxruntime_cxx_api.h>

namespace vocalremover {

// Vocal-isolation separator backed by an ONNX model. The model contract is one
// analysis frame at a time:
//
//   input  "magnitude" : float[1, numBins]  (per-bin magnitude)
//   output "mask"      : float[1, numBins]  (per-bin soft mask in [0, 1])
//
// The mask is applied to the complex spectrum, preserving phase:
//   out[b] = in[b] * mask[b].
//
// Tries the NNAPI execution provider first (hardware acceleration) and falls
// back to CPU. If inference throws, it degrades to passthrough for that frame
// rather than crashing the audio path.
class OnnxSeparator : public Separator {
public:
    static std::unique_ptr<OnnxSeparator> create(const uint8_t* modelData,
                                                 size_t modelLen);

    void prepare(int sampleRate, size_t numBins) override;
    void process(const std::complex<float>* in, std::complex<float>* out,
                 size_t numBins) override;
    const char* name() const override { return "onnx-bandscnet"; }

private:
    OnnxSeparator(Ort::Env&& env, Ort::Session&& session);

    Ort::Env env_;
    Ort::SessionOptions sessionOptions_;
    Ort::Session session_;
    Ort::MemoryInfo memInfo_{nullptr};

    std::string inputName_;
    std::string outputName_;
    const char* inputNames_[1]{};
    const char* outputNames_[1]{};

    size_t numBins_ = 0;
    std::vector<float> magnitude_;  // model input scratch
    std::vector<float> mask_;       // model output scratch
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_ONNXSEPARATOR_H
