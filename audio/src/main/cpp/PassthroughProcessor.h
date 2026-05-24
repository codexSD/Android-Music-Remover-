#ifndef VOCALREMOVER_PASSTHROUGHPROCESSOR_H
#define VOCALREMOVER_PASSTHROUGHPROCESSOR_H

#include "AudioProcessor.h"

namespace vocalremover {

// Identity processor used in Phase 1 to validate the end-to-end capture ->
// output path before any DSP is introduced. Leaves the buffer untouched.
class PassthroughProcessor : public AudioProcessor {
public:
    void prepare(int /*sampleRate*/, int /*channelCount*/) override {}

    void process(float* /*buffer*/, size_t /*frames*/, int /*channelCount*/) override {
        // Intentionally does nothing: output equals input.
    }

    const char* name() const override { return "passthrough"; }
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_PASSTHROUGHPROCESSOR_H
