#pragma once

#include "Aetherclime/ClimateRegionResolver.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace aetherclime {

// Alpha-25: local UI identity is independent of climate profile resolution.
// A Skyrim BGSLocation name may cover many exterior cells. It is evidence of
// a named area, not proof that the player stands at a precise point/landmark.
enum class LocalNameSource : std::uint8_t {
    MissingCell, InteriorCellName, NearestLocation, AncestorLocation,
    RegionFallback, UnnamedInterior
};
[[nodiscard]] std::string_view LocalNameSourceName(LocalNameSource) noexcept;
enum class LocalNameCertainty : std::uint8_t {
    Unavailable, RegionalOnly, NamedAreaRecord, ActiveInteriorCell
};
[[nodiscard]] std::string_view LocalNameCertaintyName(LocalNameCertainty) noexcept;

struct LocalIdentityInput {
    bool hasCell{false};
    bool interior{false};
    std::uint32_t cellFormId{0};
    std::string_view activeCellName{};
    // Nearest first. These are borrowed for this call only.
    std::span<const ClimateLocationEvidence> locationChain{};
    std::string_view regionDisplayName{};
    std::string_view regionKey{};
};

struct LocalIdentity {
    std::string name;
    LocalNameSource source{LocalNameSource::MissingCell};
    LocalNameCertainty certainty{LocalNameCertainty::Unavailable};
    std::uint32_t cellFormId{0};
    std::uint32_t namedLocationFormId{0};
    int locationDepth{-1};
    bool interior{false};
    bool exactInteriorCell{false};
};

[[nodiscard]] LocalIdentity ResolveLocalIdentity(const LocalIdentityInput& input);

}  // namespace aetherclime
