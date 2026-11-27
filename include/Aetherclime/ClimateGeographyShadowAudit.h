#pragma once

#include "Aetherclime/ClimateRegionResolver.h"

#include <cstdint>
#include <string_view>

namespace aetherclime {

// Alpha-18: single-shot audit of the geographic fallback with location-name
// evidence *excluded from the shadow only*. The effective climate decision is
// calculated independently, with all the original evidence unchanged.
// Calling this does not modify Config, the game, or the actual climate sample.
enum class ClimateShadowStatus : std::uint8_t {
    AnchorSelected,
    NoApplicableAnchor,
    AmbiguousAnchorOverlap,
    AnchorProfileUnconfigured,
    GeographyDisabled,
    ExcludedNoCell,
    ExcludedInterior,
    ExcludedNoWorldPosition,
    ExcludedInvalidWorldPosition
};

[[nodiscard]] std::string_view ClimateShadowStatusName(ClimateShadowStatus status) noexcept;

struct ClimateGeographyShadowAudit {
    ClimateRegionResolution effective{};  // real decision; used by game climate
    ClimateRegionResolution shadow{};     // counterfactual, never applied
    ClimateShadowStatus status{ClimateShadowStatus::ExcludedNoCell};
    bool shadowEvaluated{false};
    bool profileComparisonAvailable{false};
    bool sameProfile{false};
    bool positionAvailable{false};
    std::uint32_t worldspaceFormId{0};
    double xRaw{0.0};
    double yRaw{0.0};
};

// Intended for load/save diagnostics only. Reuses the existing bounded anchor
// resolver and reads NO TESRegion, RE::Sky, weather forms or other game state.
[[nodiscard]] ClimateGeographyShadowAudit AuditClimateGeography(
    const Config& config, const ClimateRegionContext& context);

} // namespace aetherclime
