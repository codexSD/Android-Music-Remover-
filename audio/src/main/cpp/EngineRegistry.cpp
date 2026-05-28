#include "EngineRegistry.h"

#include "CenterSeparator.h"
#include "PassthroughProcessor.h"
#include "StereoSpectrogramProcessor.h"

#ifdef USE_ONNXRUNTIME
#include "OnnxSeparator.h"
#include "Separator.h"
#include "SpectrogramProcessor.h"
#endif

namespace vocalremover {

void registerBuiltinEngines(EngineRegistry& registry, const uint8_t* modelData,
                            size_t modelLen) {
    registry.registerEngine("passthrough", [](const EngineConfig&) {
        return std::unique_ptr<AudioProcessor>(new PassthroughProcessor());
    });

    registry.registerEngine("dsp-center", [](const EngineConfig& cfg) {
        const size_t fft = static_cast<size_t>(cfg.getInt("dsp.fftSize", 2048));
        const size_t hop = static_cast<size_t>(cfg.getInt("dsp.hop", 512));
        const float sharpness = cfg.getFloat("dsp.center.sharpness", 2.0f);
        const float floor = cfg.getFloat("dsp.center.floor", 0.0f);
        return std::unique_ptr<AudioProcessor>(new StereoSpectrogramProcessor(
            fft, hop, std::make_unique<CenterExtractSeparator>(sharpness, floor)));
    });

#ifdef USE_ONNXRUNTIME
    registry.registerEngine(
        "ml-bandscnet", [modelData, modelLen](const EngineConfig& cfg)
                            -> std::unique_ptr<AudioProcessor> {
            if (modelData == nullptr || modelLen == 0) return nullptr;
            auto separator = OnnxSeparator::create(modelData, modelLen);
            if (!separator) return nullptr;
            const size_t fft = static_cast<size_t>(cfg.getInt("ml.fftSize", 2048));
            const size_t hop = static_cast<size_t>(cfg.getInt("ml.hop", 512));
            return std::unique_ptr<AudioProcessor>(
                new SpectrogramProcessor(fft, hop, std::move(separator)));
        });
#else
    (void)modelData;
    (void)modelLen;
#endif
}

}  // namespace vocalremover
