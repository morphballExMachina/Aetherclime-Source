#pragma once
#include "Aetherclime/NativeRegionAtlas.h"
#include <optional>
#include <string_view>

namespace aetherclime {
// Geometry-based modeling evidence, not political boundaries or measured ecology.
enum class BiomeEvidence : std::uint32_t { None, Unique, Overlap, OverlapEdge, Nearby };
struct BiomeIdentity {
    std::string biomeA, biomeB;
    const char* status{"unavailable"};
    bool available{};
    double weightA{}, weightB{}, gapDistanceRaw{-1}, gapRunnerUpRaw{-1};
    BiomeEvidence evidence{BiomeEvidence::None};
};
const char* BiomeEvidenceName(BiomeEvidence) noexcept;
// Pure selection from cached atlas evidence. Independent of temperature-profile
// availability and biomeClimate.enabled. Gap policy still comes from config.
BiomeIdentity ResolveBiomeIdentity(const Config&, const NativeAtlasResult&);
bool ValidBiomeIdentity(const BiomeIdentity&) noexcept;
// Default query semantics abstain on nearby extrapolation. V2 still reports it
// explicitly for native consumers that choose to inspect provisional evidence.
std::optional<double> BiomeShare(const BiomeIdentity&, std::string_view key) noexcept;
} // namespace aetherclime
