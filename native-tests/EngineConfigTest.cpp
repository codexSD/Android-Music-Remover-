#include "EngineConfig.h"

#include "TinyTest.h"

using vocalremover::EngineConfig;

TEST(EngineConfig, ReturnsFallbackForAbsentKey) {
    EngineConfig c;
    CHECK_NEAR(c.getFloat("missing", 1.5f), 1.5f, 1e-6);
    CHECK_EQ(c.getInt("missing", 7), 7);
    CHECK(c.getBool("missing", true));
}

TEST(EngineConfig, ParsesValues) {
    EngineConfig c;
    c.set("sharpness", "2.5");
    c.set("fft", "2048");
    c.set("flag", "true");
    CHECK_NEAR(c.getFloat("sharpness", 0.0f), 2.5f, 1e-6);
    CHECK_EQ(c.getInt("fft", 0), 2048);
    CHECK(c.getBool("flag", false));
}

TEST(EngineConfig, GarbageFallsBack) {
    EngineConfig c;
    c.set("n", "notanumber");
    CHECK_EQ(c.getInt("n", 42), 42);
    CHECK_NEAR(c.getFloat("n", 4.2f), 4.2f, 1e-6);
}

TEST(EngineConfig, LastWriteWins) {
    EngineConfig c;
    c.set("k", "1");
    c.set("k", "2");
    CHECK_EQ(c.getInt("k", 0), 2);
    CHECK_EQ(c.size(), static_cast<size_t>(1));
}

TEST(EngineConfig, BoolVariants) {
    EngineConfig c;
    c.set("a", "1");
    c.set("b", "yes");
    c.set("c", "0");
    c.set("d", "false");
    CHECK(c.getBool("a", false));
    CHECK(c.getBool("b", false));
    CHECK(!c.getBool("c", true));
    CHECK(!c.getBool("d", true));
}
