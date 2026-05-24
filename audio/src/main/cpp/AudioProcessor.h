#ifndef VOCALREMOVER_AUDIOPROCESSOR_H
#define VOCALREMOVER_AUDIOPROCESSOR_H

#include <cstddef>

namespace vocalremover {

// Transforms a block of interleaved float audio in place. This is the seam
// between the fixed capture/output plumbing and the swappable DSP stage.
//
// Phase 1 ships PassthroughProcessor (identity). Phase 2 will add a processor
// that wraps the Band-SCNet ONNX session for vocal isolation. Keeping the
// contract this small means the capture/output pipeline never has to change as
// the DSP evolves.
class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;

    // Called once on the audio thread before the first process() call, with
    // the stream parameters the pipeline will use.
    virtual void prepare(int sampleRate, int channelCount) = 0;

    // Process exactly `frames` interleaved frames in place. Must be
    // real-time safe: no allocation, no locks, no blocking I/O.
    virtual void process(float* buffer, size_t frames, int channelCount) = 0;

    // Human-readable name for logging/telemetry.
    virtual const char* name() const = 0;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_AUDIOPROCESSOR_H
