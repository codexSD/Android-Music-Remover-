#ifndef VOCALREMOVER_ENGINEREGISTRY_H
#define VOCALREMOVER_ENGINEREGISTRY_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "AudioProcessor.h"
#include "EngineConfig.h"

namespace vocalremover {

// Maps a stable engine identifier to a factory that constructs (but does not
// init) that engine. The pipeline looks an engine up by id at start time. The id
// is decided in the Kotlin layer; the registry contains no selection logic.
//
// Adding an engine = implement AudioProcessor + add one registerEngine() call in
// registerBuiltinEngines(). Nothing else changes.
class EngineRegistry {
public:
    using Factory = std::function<std::unique_ptr<AudioProcessor>(const EngineConfig&)>;

    void registerEngine(const char* id, Factory factory) {
        factories_[id] = std::move(factory);
    }

    bool has(const std::string& id) const {
        return factories_.find(id) != factories_.end();
    }

    // Constructs the engine (init() is the caller's responsibility). Returns
    // nullptr if the id is unknown or the factory itself fails.
    std::unique_ptr<AudioProcessor> create(const std::string& id,
                                           const EngineConfig& config) const {
        auto it = factories_.find(id);
        if (it == factories_.end()) return nullptr;
        return it->second(config);
    }

private:
    std::unordered_map<std::string, Factory> factories_;
};

// Registers the built-in engines: "passthrough", "dsp-center", and (only when
// ONNX support is compiled in and model bytes are supplied) "ml-bandscnet".
// The ML factory closes over the model bytes, which must stay valid for the
// duration of the create() call. Pass nullptr/0 when no model is available.
void registerBuiltinEngines(EngineRegistry& registry, const uint8_t* modelData,
                            size_t modelLen);

}  // namespace vocalremover

#endif  // VOCALREMOVER_ENGINEREGISTRY_H
