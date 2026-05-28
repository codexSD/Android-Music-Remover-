#ifndef VOCALREMOVER_ENGINECONFIG_H
#define VOCALREMOVER_ENGINECONFIG_H

#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace vocalremover {

// Opaque key/value configuration handed to an engine at init time. Built off the
// real-time thread (in the Kotlin resolver, marshalled across JNI). Each engine
// reads only the keys it understands; unknown keys are ignored. Values are
// strings parsed once at init — never on the audio path.
//
// Model weights are deliberately NOT carried here (too large); they travel on a
// dedicated binary channel. This map is for small tunables only.
class EngineConfig {
public:
    void set(std::string key, std::string value) {
        for (auto& kv : kv_) {
            if (kv.first == key) {
                kv.second = std::move(value);
                return;
            }
        }
        kv_.emplace_back(std::move(key), std::move(value));
    }

    const std::string* find(const char* key) const {
        for (const auto& kv : kv_) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    float getFloat(const char* key, float fallback) const {
        const std::string* v = find(key);
        if (v == nullptr || v->empty()) return fallback;
        char* end = nullptr;
        const float parsed = std::strtof(v->c_str(), &end);
        return end == v->c_str() ? fallback : parsed;
    }

    int getInt(const char* key, int fallback) const {
        const std::string* v = find(key);
        if (v == nullptr || v->empty()) return fallback;
        char* end = nullptr;
        const long parsed = std::strtol(v->c_str(), &end, 10);
        return end == v->c_str() ? fallback : static_cast<int>(parsed);
    }

    bool getBool(const char* key, bool fallback) const {
        const std::string* v = find(key);
        if (v == nullptr) return fallback;
        return *v == "1" || *v == "true" || *v == "TRUE" || *v == "yes";
    }

    size_t size() const { return kv_.size(); }

private:
    std::vector<std::pair<std::string, std::string>> kv_;
};

// Static description of what an engine needs from the pipeline, queryable after
// construction and before init(). `identifier` must point at storage that
// outlives the descriptor (e.g. a string literal or a member std::string).
struct EngineCapabilities {
    const char* identifier = "";
    size_t requiredChunkFrames = 0;  // 0 = any chunk (engine buffers internally)
    int latencySamples = 0;          // introduced algorithmic latency, per channel
    bool needsStereo = false;        // true if it requires distinct L/R input
};

}  // namespace vocalremover

#endif  // VOCALREMOVER_ENGINECONFIG_H
