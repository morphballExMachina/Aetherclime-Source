#include "Aetherclime/GeographicFallbackReason.h"
#include "Aetherclime/GeographicEvidence.h"

namespace aetherclime {

std::string_view GeographicFallbackReasonName(GeographicFallbackReason reason) noexcept {
    switch (reason) {
    case GeographicFallbackReason::NotFallback: return "not-fallback";
    case GeographicFallbackReason::GeographicFallbackDisabled: return "geographic-fallback-disabled";
    case GeographicFallbackReason::AmbiguousGeographicEvidence: return "geographic-evidence-ambiguous";
    case GeographicFallbackReason::UnconfiguredGeographicProfile: return "geographic-profile-unconfigured";
    case GeographicFallbackReason::AmbiguousLocationEvidence: return "location-evidence-ambiguous";
    case GeographicFallbackReason::UnconfiguredLocationProfile: return "location-profile-unconfigured";
    case GeographicFallbackReason::ContinuityDisabled: return "continuity-disabled";
    case GeographicFallbackReason::NoVerifiedContinuityOrigin: return "no-verified-continuity-origin";
    case GeographicFallbackReason::ContinuityOriginExpired: return "continuity-origin-expired";
    case GeographicFallbackReason::ContinuityOriginTooFar: return "continuity-origin-too-far";
    case GeographicFallbackReason::NoApplicableGeographicEvidence: return "no-applicable-geographic-evidence";
    case GeographicFallbackReason::Unclassified: return "not-evaluated";
    }
    return "unknown";
}

GeographicFallbackFinding ClassifyGeographicFallback(
    ClimateRegionSource effectiveSource, ClimateRegionSource directSource,
    ContinuityStatus continuityStatus, bool geographicFallbackEnabled) noexcept {
    GeographicFallbackFinding finding{.directSource = directSource,
                                      .continuityStatus = continuityStatus};
    if (ClassifyGeographicEvidence(effectiveSource) != GeographicEvidenceTier::Fallback) {
        finding.reason = GeographicFallbackReason::NotFallback;
    } else if (directSource == ClimateRegionSource::FallbackGeographyAmbiguous) {
        finding.reason = GeographicFallbackReason::AmbiguousGeographicEvidence;
    } else if (directSource == ClimateRegionSource::FallbackGeographyUnconfigured) {
        finding.reason = GeographicFallbackReason::UnconfiguredGeographicProfile;
    } else if (directSource == ClimateRegionSource::FallbackAmbiguous) {
        finding.reason = GeographicFallbackReason::AmbiguousLocationEvidence;
    } else if (directSource == ClimateRegionSource::FallbackUnconfigured) {
        finding.reason = GeographicFallbackReason::UnconfiguredLocationProfile;
    } else if (!geographicFallbackEnabled) {
        finding.reason = GeographicFallbackReason::GeographicFallbackDisabled;
    } else if (continuityStatus == ContinuityStatus::Disabled) {
        finding.reason = GeographicFallbackReason::ContinuityDisabled;
    } else if (continuityStatus == ContinuityStatus::NoVerifiedOrigin) {
        finding.reason = GeographicFallbackReason::NoVerifiedContinuityOrigin;
    } else if (continuityStatus == ContinuityStatus::Expired ||
               continuityStatus == ContinuityStatus::DifferentCalendarDay) {
        finding.reason = GeographicFallbackReason::ContinuityOriginExpired;
    } else if (continuityStatus == ContinuityStatus::TooFar ||
               continuityStatus == ContinuityStatus::DifferentWorldspace) {
        finding.reason = GeographicFallbackReason::ContinuityOriginTooFar;
    } else {
        finding.reason = GeographicFallbackReason::NoApplicableGeographicEvidence;
    }
    return finding;
}

} // namespace aetherclime
