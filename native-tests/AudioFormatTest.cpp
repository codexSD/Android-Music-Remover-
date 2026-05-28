#include "AudioFormat.h"

#include <cstdint>

#include "TinyTest.h"

namespace af = vocalremover::audioformat;

TEST(AudioFormat, Int16ToFloatRoundsToUnitRange) {
    int16_t in[4] = {0, 16384, -16384, -32768};
    float out[4] = {0};
    af::int16ToFloat(in, out, 4);
    CHECK_NEAR(out[0], 0.0f, 1e-6);
    CHECK_NEAR(out[1], 0.5f, 1e-4);
    CHECK_NEAR(out[2], -0.5f, 1e-4);
    CHECK_NEAR(out[3], -1.0f, 1e-6);  // -32768 maps exactly to -1.0
}

TEST(AudioFormat, FloatToInt16Clamps) {
    float in[4] = {0.0f, 1.5f, -1.5f, 0.5f};
    int16_t out[4] = {0};
    af::floatToInt16(in, out, 4);
    CHECK_EQ(out[0], static_cast<int16_t>(0));
    CHECK_EQ(out[1], static_cast<int16_t>(32767));   // clamped to +full scale
    CHECK_EQ(out[2], static_cast<int16_t>(-32767));  // clamped to -full scale
    CHECK(out[3] > 16000 && out[3] < 16600);
}

TEST(AudioFormat, RoundTripInt16PreservesValuesApproximately) {
    int16_t in[5] = {0, 100, -100, 20000, -20000};
    float mid[5] = {0};
    int16_t back[5] = {0};
    af::int16ToFloat(in, mid, 5);
    af::floatToInt16(mid, back, 5);
    for (int i = 0; i < 5; ++i) {
        // One LSB of tolerance from the asymmetric 32768/32767 scaling.
        const int diff = static_cast<int>(in[i]) - static_cast<int>(back[i]);
        CHECK(diff >= -1 && diff <= 1);
    }
}

TEST(AudioFormat, StereoToMonoAverages) {
    // Two frames: L/R pairs.
    float in[4] = {1.0f, 0.0f, 0.4f, 0.6f};
    float out[2] = {0};
    af::stereoToMono(in, out, 2);
    CHECK_NEAR(out[0], 0.5f, 1e-6);
    CHECK_NEAR(out[1], 0.5f, 1e-6);
}

TEST(AudioFormat, MonoToStereoDuplicates) {
    float in[2] = {0.25f, -0.75f};
    float out[4] = {0};
    af::monoToStereo(in, out, 2);
    CHECK_NEAR(out[0], 0.25f, 1e-6);
    CHECK_NEAR(out[1], 0.25f, 1e-6);
    CHECK_NEAR(out[2], -0.75f, 1e-6);
    CHECK_NEAR(out[3], -0.75f, 1e-6);
}

TEST(AudioFormat, MonoStereoMonoRoundTrip) {
    float mono[3] = {0.1f, -0.2f, 0.3f};
    float stereo[6] = {0};
    float back[3] = {0};
    af::monoToStereo(mono, stereo, 3);
    af::stereoToMono(stereo, back, 3);
    for (int i = 0; i < 3; ++i) CHECK_NEAR(back[i], mono[i], 1e-6);
}
