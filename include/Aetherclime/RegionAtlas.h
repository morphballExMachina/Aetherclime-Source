#pragma once

#include "Aetherclime/ClimateRegionResolver.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace aetherclime {

// Alpha-28. These are opt-in, surveyed RESEARCH patches. They are NOT
// verified hold polygons, and cannot confer region or weather trust.
enum class AtlasStatus : std::uint8_t {
    Disabled, NoExteriorPoint, OutsideCoverage, MatchedProvisional,
    ConflictingRegions, UnconfiguredProfile
};
[[nodiscard]] std::string_view AtlasStatusName(AtlasStatus status) noexcept;

struct AtlasAssessment {
    AtlasStatus status{AtlasStatus::Disabled};
    std::string regionKey{};
    std::string zoneLabel{};
    std::size_t matchingZones{0};
    std::uint32_t worldspaceFormId{0};
    double minXRaw{0.0}, maxXRaw{0.0}, minYRaw{0.0}, maxYRaw{0.0};
    [[nodiscard]] bool provisional() const noexcept {
        return status == AtlasStatus::MatchedProvisional;
    }
};

// Stateless, O(number of bounded zones); no engine pointers, hooks, or cache.
// Evaluated only for explicitly enabled exterior-worldspace coordinates.
// Even when several patches overlap, different profile keys MUST produce
// ambiguity instead of selecting a patch by order or smallest area.
[[nodiscard]] AtlasAssessment EvaluateRegionAtlas(
    const Config& config, const ClimateRegionContext& context) noexcept;

} // namespace aetherclime
