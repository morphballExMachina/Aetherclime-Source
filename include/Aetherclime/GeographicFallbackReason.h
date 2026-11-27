#pragma once

#include "Aetherclime/RegionalContinuity.h"

#include <cstdint>
#include <string_view>

namespace aetherclime {

// Alpha-35: a *diagnostic gate*, not a proved spatial/casual reason for a
// fallback climate. The resolver and the provisional continuation may each
// reject evidence for independent reasons.
enum class GeographicFallbackReason : std::uint8_t {
    NotFallback,
    GeographicFallbackDisabled,
    AmbiguousGeographicEvidence,
    UnconfiguredGeographicProfile,
    AmbiguousLocationEvidence,
    UnconfiguredLocationProfile,
    ContinuityDisabled,
    NoVerifiedContinuityOrigin,
    ContinuityOriginExpired,
    ContinuityOriginTooFar,
    NoApplicableGeographicEvidence,
    Unclassified
};

[[nodiscard]] std::string_view GeographicFallbackReasonName(GeographicFallbackReason) noexcept;

struct GeographicFallbackFinding {
    GeographicFallbackReason reason{GeographicFallbackReason::Unclassified};
    ClimateRegionSource directSource{ClimateRegionSource::FallbackNoEvidence};
    ContinuityStatus continuityStatus{ContinuityStatus::NoVerifiedOrigin};
    double originDistanceRawUnits{-1.0}; // -1: not evaluated, never a survey radius
    double originAgeGameHours{-1.0};
};

[[nodiscard]] GeographicFallbackFinding ClassifyGeographicFallback(
    ClimateRegionSource effectiveSource,
    ClimateRegionSource directSource,
    ContinuityStatus continuityStatus,
    bool geographicFallbackEnabled) noexcept;

} // namespace aetherclime
