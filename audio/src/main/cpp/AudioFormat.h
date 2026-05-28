#ifndef VOCALREMOVER_AUDIOFORMAT_H
#define VOCALREMOVER_AUDIOFORMAT_H

#include <cstddef>
#include <cstdint>

namespace vocalremover {

// PCM format conversion helpers. All functions are pure, branch-light, and
// allocation-free so they can run on the real-time audio path. They are kept
// header-only and free of Android dependencies so they can be unit tested on a
// host machine.
namespace audioformat {

constexpr float kInt16ToFloat = 1.0f / 32768.0f;
constexpr float kFloatToInt16 = 32767.0f;

inline float clampUnit(float v) {
    if (v > 1.0f) return 1.0f;
    if (v < -1.0f) return -1.0f;
    return v;
}

// Convert signed 16-bit PCM to normalized float in [-1, 1).
inline void int16ToFloat(const int16_t* in, float* out, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        out[i] = static_cast<float>(in[i]) * kInt16ToFloat;
    }
}

// Convert normalized float to signed 16-bit PCM with clamping.
inline void floatToInt16(const float* in, int16_t* out, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const float clamped = clampUnit(in[i]);
        out[i] = static_cast<int16_t>(clamped * kFloatToInt16);
    }
}

// Downmix interleaved stereo to mono by averaging the two channels.
// `frames` is the number of stereo frames; `in` holds 2*frames samples and
// `out` receives `frames` samples.
inline void stereoToMono(const float* in, float* out, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        out[i] = 0.5f * (in[2 * i] + in[2 * i + 1]);
    }
}

// Upmix mono to interleaved stereo by duplicating each sample.
inline void monoToStereo(const float* in, float* out, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        const float s = in[i];
        out[2 * i] = s;
        out[2 * i + 1] = s;
    }
}

}  // namespace audioformat
}  // namespace vocalremover

#endif  // VOCALREMOVER_AUDIOFORMAT_H
