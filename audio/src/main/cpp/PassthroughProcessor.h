#ifndef VOCALREMOVER_PASSTHROUGHPROCESSOR_H
#define VOCALREMOVER_PASSTHROUGHPROCESSOR_H

#include "AudioProcessor.h"

namespace vocalremover {

// Identity processor: validates the end-to-end capture -> output path and serves
// as the terminal fallback engine. Leaves the buffer untouched.
class PassthroughProcessor : public AudioProcessor {
public:
    bool init(int /*sampleRate*/, int /*channelCount*/, const EngineConfig& /*config*/,
              int& latencySamplesOut) override {
        latencySamplesOut = 0;
        return true;
    }

    EngineCapabilities capabilities() const override {
        return EngineCapabilities{"passthrough", 0, 0, /*needsStereo=*/false};
    }

    void process(float* /*buffer*/, size_t /*frames*/, int /*channelCount*/) override {
        // Intentionally does nothing: output equals input.
    }

    const char* name() const override { return "passthrough"; }
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_PASSTHROUGHPROCESSOR_H
