#include "Aetherclime/GeographicFallbackReason.h"
#include <cassert>
#include <iostream>

using namespace aetherclime;

namespace {
constexpr auto Fb = ClimateRegionSource::FallbackNoEvidence;
constexpr auto Anchor = ClimateRegionSource::GeographicAnchorProximity;

GeographicFallbackReason Reason(ClimateRegionSource effective, ClimateRegionSource direct,
                                ContinuityStatus continuity, bool enabled = true) {
    return ClassifyGeographicFallback(effective, direct, continuity, enabled).reason;
}

void ClassifiesOnlyObservedGates() {
    assert(Reason(Anchor, Fb, ContinuityStatus::Provisional) ==
           GeographicFallbackReason::NotFallback);
    assert(Reason(Fb, Fb, ContinuityStatus::NoVerifiedOrigin) ==
           GeographicFallbackReason::NoVerifiedContinuityOrigin);
    assert(Reason(Fb, Fb, ContinuityStatus::Expired) ==
           GeographicFallbackReason::ContinuityOriginExpired);
    assert(Reason(Fb, Fb, ContinuityStatus::DifferentCalendarDay) ==
           GeographicFallbackReason::ContinuityOriginExpired);
    assert(Reason(Fb, Fb, ContinuityStatus::TooFar) ==
           GeographicFallbackReason::ContinuityOriginTooFar);
    assert(Reason(Fb, Fb, ContinuityStatus::DifferentWorldspace) ==
           GeographicFallbackReason::ContinuityOriginTooFar);
    assert(Reason(Fb, Fb, ContinuityStatus::Disabled) ==
           GeographicFallbackReason::ContinuityDisabled);
    assert(Reason(Fb, Fb, ContinuityStatus::NoVerifiedOrigin, false) ==
           GeographicFallbackReason::GeographicFallbackDisabled);
    assert(Reason(ClimateRegionSource::FallbackGeographyAmbiguous,
                  ClimateRegionSource::FallbackGeographyAmbiguous,
                  ContinuityStatus::ExplicitAmbiguity) ==
           GeographicFallbackReason::AmbiguousGeographicEvidence);
    assert(Reason(ClimateRegionSource::FallbackGeographyUnconfigured,
                  ClimateRegionSource::FallbackGeographyUnconfigured,
                  ContinuityStatus::ExplicitAmbiguity) ==
           GeographicFallbackReason::UnconfiguredGeographicProfile);
    assert(Reason(ClimateRegionSource::FallbackAmbiguous,
                  ClimateRegionSource::FallbackAmbiguous,
                  ContinuityStatus::ExplicitAmbiguity) ==
           GeographicFallbackReason::AmbiguousLocationEvidence);
    assert(Reason(ClimateRegionSource::FallbackUnconfigured,
                  ClimateRegionSource::FallbackUnconfigured,
                  ContinuityStatus::ExplicitAmbiguity) ==
           GeographicFallbackReason::UnconfiguredLocationProfile);
    assert(Reason(Fb, Fb, ContinuityStatus::NoExteriorPosition) ==
           GeographicFallbackReason::NoApplicableGeographicEvidence);
    const auto finding = ClassifyGeographicFallback(Fb, Fb,
        ContinuityStatus::NoVerifiedOrigin, true);
    assert(finding.directSource == Fb);
    assert(finding.continuityStatus == ContinuityStatus::NoVerifiedOrigin);
    assert(GeographicFallbackReasonName(finding.reason) == "no-verified-continuity-origin");
}
}

int main() {
    ClassifiesOnlyObservedGates();
    std::cout << "Alpha-35 fallback evidence gate tests passed\n";
}
