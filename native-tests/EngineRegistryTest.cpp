#include "EngineRegistry.h"

#include <memory>

#include "EngineConfig.h"
#include "PassthroughProcessor.h"
#include "TinyTest.h"

using vocalremover::AudioProcessor;
using vocalremover::EngineConfig;
using vocalremover::EngineRegistry;
using vocalremover::PassthroughProcessor;

TEST(EngineRegistry, UnknownIdReturnsNull) {
    EngineRegistry r;
    CHECK(!r.has("nope"));
    CHECK(r.create("nope", EngineConfig{}) == nullptr);
}

TEST(EngineRegistry, RegisteredFactoryConstructsAndSeesConfig) {
    EngineRegistry r;
    int seen = -1;
    r.registerEngine("probe", [&seen](const EngineConfig& c) {
        seen = c.getInt("x", -1);
        return std::unique_ptr<AudioProcessor>(new PassthroughProcessor());
    });
    CHECK(r.has("probe"));

    EngineConfig cfg;
    cfg.set("x", "99");
    auto p = r.create("probe", cfg);
    CHECK(p != nullptr);
    CHECK_EQ(seen, 99);
}

TEST(EngineRegistry, FactoryMayReturnNullForFailure) {
    EngineRegistry r;
    r.registerEngine("fails", [](const EngineConfig&) {
        return std::unique_ptr<AudioProcessor>(nullptr);
    });
    CHECK(r.has("fails"));
    CHECK(r.create("fails", EngineConfig{}) == nullptr);
}

TEST(EngineRegistry, BuiltinsProvideDspAndPassthrough) {
    EngineRegistry r;
    registerBuiltinEngines(r, /*modelData=*/nullptr, /*modelLen=*/0);
    CHECK(r.has("passthrough"));
    CHECK(r.has("dsp-center"));

    auto dsp = r.create("dsp-center", EngineConfig{});
    CHECK(dsp != nullptr);
    int latency = -1;
    CHECK(dsp->init(48000, 2, EngineConfig{}, latency));
    CHECK(dsp->capabilities().needsStereo);

    auto pass = r.create("passthrough", EngineConfig{});
    CHECK(pass != nullptr);

    // Without ONNX support compiled in (host build), the ML engine isn't registered.
    CHECK(!r.has("ml-bandscnet"));
}
