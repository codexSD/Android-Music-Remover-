#include "OnnxSeparator.h"

#include <array>
#include <cmath>

#include "Log.h"
#include "nnapi_provider_factory.h"

namespace vocalremover {

namespace {
// Best-effort: append the NNAPI execution provider. Returns true if it was
// accepted; on failure ORT simply runs on CPU.
bool tryAppendNnapi(Ort::SessionOptions& options) {
    const uint32_t nnapiFlags = 0;  // default flags
    OrtStatus* status =
        OrtSessionOptionsAppendExecutionProvider_Nnapi(options, nnapiFlags);
    if (status != nullptr) {
        Ort::GetApi().ReleaseStatus(status);
        VR_LOGW("NNAPI EP unavailable; using CPU");
        return false;
    }
    return true;
}
}  // namespace

OnnxSeparator::OnnxSeparator(Ort::Env&& env, Ort::Session&& session)
    : env_(std::move(env)), session_(std::move(session)) {}

std::unique_ptr<OnnxSeparator> OnnxSeparator::create(const uint8_t* modelData,
                                                     size_t modelLen) {
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "vocalremover");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
        tryAppendNnapi(options);

        Ort::Session session(env, modelData, modelLen, options);
        // Input/output names are resolved later in prepare() from the stored
        // session, once numBins is known.
        return std::unique_ptr<OnnxSeparator>(
            new OnnxSeparator(std::move(env), std::move(session)));
    } catch (const Ort::Exception& e) {
        VR_LOGE("OnnxSeparator.create failed: %s", e.what());
        return nullptr;
    }
}

void OnnxSeparator::prepare(int /*sampleRate*/, size_t numBins) {
    numBins_ = numBins;
    magnitude_.assign(numBins, 0.0f);
    mask_.assign(numBins, 1.0f);
    memInfo_ = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    Ort::AllocatorWithDefaultOptions alloc;
    inputName_ = session_.GetInputNameAllocated(0, alloc).get();
    outputName_ = session_.GetOutputNameAllocated(0, alloc).get();
    inputNames_[0] = inputName_.c_str();
    outputNames_[0] = outputName_.c_str();
}

void OnnxSeparator::process(const std::complex<float>* in, std::complex<float>* out,
                            size_t numBins) {
    if (numBins != numBins_ || numBins_ == 0) {
        for (size_t b = 0; b < numBins; ++b) out[b] = in[b];
        return;
    }

    for (size_t b = 0; b < numBins; ++b) {
        magnitude_[b] = std::abs(in[b]);
    }

    try {
        const std::array<int64_t, 2> shape{1, static_cast<int64_t>(numBins_)};
        Ort::Value input = Ort::Value::CreateTensor<float>(
            memInfo_, magnitude_.data(), numBins_, shape.data(), shape.size());
        Ort::Value output = Ort::Value::CreateTensor<float>(
            memInfo_, mask_.data(), numBins_, shape.data(), shape.size());

        session_.Run(Ort::RunOptions{nullptr}, inputNames_, &input, 1, outputNames_,
                     &output, 1);

        for (size_t b = 0; b < numBins; ++b) {
            out[b] = in[b] * mask_[b];
        }
    } catch (const Ort::Exception& e) {
        VR_LOGW("OnnxSeparator.process failed (%s); passthrough frame", e.what());
        for (size_t b = 0; b < numBins; ++b) out[b] = in[b];
    }
}

}  // namespace vocalremover
