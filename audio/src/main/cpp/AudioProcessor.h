#ifndef VOCALREMOVER_AUDIOPROCESSOR_H
#define VOCALREMOVER_AUDIOPROCESSOR_H

#include <cstddef>

#include "EngineConfig.h"

namespace vocalremover {

// Transforms a block of interleaved float audio in place. This is the seam
// between the fixed capture/output plumbing and the swappable processing stage
// (DSP center-extraction, ONNX separator, passthrough).
//
// The pipeline reaches every engine only through this contract; engines are
// chosen by a registry from an id decided in the Kotlin layer. Changing engine
// is done by fully stopping the pipeline and constructing a new one — there is
// no live swap, so process() never has to be concurrency-safe against teardown.
class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;

    // Called once off the real-time thread before the first process(), with the
    // negotiated stream parameters and engine config. This is where all
    // allocation and resource loading happens. Returns false on unrecoverable
    // failure (the resolver then falls back to the next engine).
    // `latencySamplesOut` reports the algorithmic latency introduced, per channel.
    virtual bool init(int sampleRate, int channelCount, const EngineConfig& config,
                      int& latencySamplesOut) = 0;

    // Static capability descriptor; valid after construction, before init().
    virtual EngineCapabilities capabilities() const = 0;

    // Process exactly `frames` interleaved frames in place. Must be real-time
    // safe: no allocation, no locks, no blocking I/O.
    virtual void process(float* buffer, size_t frames, int channelCount) = 0;

    // Human-readable name for logging/telemetry.
    virtual const char* name() const = 0;
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_AUDIOPROCESSOR_H
