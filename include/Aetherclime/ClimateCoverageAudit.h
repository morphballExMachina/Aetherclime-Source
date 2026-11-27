#pragma once

#include "Aetherclime/ClimateRegionResolver.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace aetherclime {

// Bounded, non-authoritative geometric coverage diagnostics. This never
// changes the effective resolver, weather records, or environment API.
// "Reference neighborhood" means a user-configured sample circle, NOT the
// real boundary of a Skyrim hold, even if a location-name classifier agrees.
enum class ClimateCoverageStatus : std::uint8_t {
    ExcludedNoCell,
    ExcludedInterior,
    ExcludedNoWorldPosition,
    ExcludedInvalidPosition,
    Disabled,
    NoReferenceForWorldspace,
    OutsideSampleNeighborhoods,
    WithinOneReference,
    WithinSameProfileOverlap,
    ConflictingProfileOverlap
};

[[nodiscard]] std::string_view ClimateCoverageStatusName(ClimateCoverageStatus status) noexcept;

struct ClimateCoverageAudit {
    ClimateCoverageStatus status{ClimateCoverageStatus::ExcludedNoCell};
    std::uint32_t worldspaceFormId{0};
    double xRaw{0.0};
    double yRaw{0.0};
    // The engine reports raw game coordinate Z; no scaling or sea-level datum
    // is inferred, no weather or temperature model is changed.
    double zRaw{0.0};
    bool rawZAvailable{false};

    std::size_t worldspaceAnchors{0};
    std::size_t inRangeAnchors{0};
    std::size_t distinctInRangeProfiles{0};
    std::string nearestAnchorLabel{};
    std::string nearestRegionKey{};
    double nearestDistanceRawUnits{-1.0};
    double nearestRadiusRawUnits{-1.0};
    // Positive: inside nearest-CENTER anchor's radius. Negative: outside.
    // This is the nearest reference center, not necessarily the closest edge.
    double nearestSignedMarginRawUnits{-1.0};
    // Minimum distance to ANY configured sample circle boundary in worldspace.
    double nearestBoundaryDistanceRawUnits{-1.0};
    bool nearSampleEdge{false};
};

// For a real player observation, only supply valid exterior worldspace XYZ.
// Process is bounded to 64 configured anchors; only call on load/save.
[[nodiscard]] ClimateCoverageAudit AuditClimateCoverage(
    const Config& config, const ClimateRegionContext& context);

} // namespace aetherclime
