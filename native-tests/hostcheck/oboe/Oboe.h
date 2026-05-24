// HOST SYNTAX-CHECK STUB ONLY. Not used by the Android build (the real Oboe
// headers come from the com.google.oboe:oboe prefab package). This mirrors just
// the slice of the Oboe API the engine uses, so OutputStream.{h,cpp} can be
// compiled with -fsyntax-only on a plain host. It is intentionally NOT a
// faithful or complete Oboe definition.
#ifndef HOSTCHECK_OBOE_OBOE_H
#define HOSTCHECK_OBOE_OBOE_H

#include <cstdint>
#include <memory>

namespace oboe {

enum class Direction { Output, Input };
enum class PerformanceMode { LowLatency, None, PowerSaving };
enum class SharingMode { Exclusive, Shared };
enum class AudioFormat { Float, I16 };
enum class SampleRateConversionQuality { None, Low, Medium, High, Best };
enum class Usage { Media, Game };
enum class ContentType { Music, Speech };
enum class Result { OK, ErrorBase };
enum class DataCallbackResult { Continue, Stop };

inline const char* convertToText(Result) { return "Result"; }

class AudioStream {
public:
    virtual ~AudioStream() = default;
    int32_t getSampleRate() { return 48000; }
    int32_t getChannelCount() { return 2; }
    SharingMode getSharingMode() { return SharingMode::Exclusive; }
    int32_t getFramesPerBurst() { return 192; }
    Result requestStart() { return Result::OK; }
    Result requestStop() { return Result::OK; }
    Result stop() { return Result::OK; }
    Result close() { return Result::OK; }
};

class AudioStreamDataCallback {
public:
    virtual ~AudioStreamDataCallback() = default;
    virtual DataCallbackResult onAudioReady(AudioStream* stream, void* audioData,
                                            int32_t numFrames) = 0;
};

class AudioStreamErrorCallback {
public:
    virtual ~AudioStreamErrorCallback() = default;
    virtual void onErrorAfterClose(AudioStream* /*stream*/, Result /*error*/) {}
};

class AudioStreamBuilder {
public:
    AudioStreamBuilder* setDirection(Direction) { return this; }
    AudioStreamBuilder* setPerformanceMode(PerformanceMode) { return this; }
    AudioStreamBuilder* setSharingMode(SharingMode) { return this; }
    AudioStreamBuilder* setFormat(AudioFormat) { return this; }
    AudioStreamBuilder* setChannelCount(int) { return this; }
    AudioStreamBuilder* setSampleRate(int) { return this; }
    AudioStreamBuilder* setSampleRateConversionQuality(SampleRateConversionQuality) { return this; }
    AudioStreamBuilder* setUsage(Usage) { return this; }
    AudioStreamBuilder* setContentType(ContentType) { return this; }
    AudioStreamBuilder* setDataCallback(AudioStreamDataCallback*) { return this; }
    AudioStreamBuilder* setErrorCallback(AudioStreamErrorCallback*) { return this; }
    Result openStream(std::shared_ptr<AudioStream>& out) {
        out = std::make_shared<AudioStream>();
        return Result::OK;
    }
};

}  // namespace oboe

#endif
