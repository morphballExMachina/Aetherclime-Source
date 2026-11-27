#include "Aetherclime/GeographicEvidence.h"

namespace aetherclime {

GeographicEvidenceTier ClassifyGeographicEvidence(ClimateRegionSource source) noexcept {
    switch (source) {
    case ClimateRegionSource::LocationDisplayName:
    case ClimateRegionSource::LocationEditorId:
        return GeographicEvidenceTier::NamedLocation;
    case ClimateRegionSource::GeographicAnchorProximity:
        return GeographicEvidenceTier::AnchorProximity;
    case ClimateRegionSource::ProvisionalRecentExterior:
    case ClimateRegionSource::ProvisionalSurveySample:
    case ClimateRegionSource::ProvisionalAtlasZone:
        return GeographicEvidenceTier::Provisional;
    case ClimateRegionSource::FallbackGeographyAmbiguous:
    case ClimateRegionSource::FallbackGeographyUnconfigured:
    case ClimateRegionSource::FallbackNoEvidence:
    case ClimateRegionSource::FallbackUnmatched:
    case ClimateRegionSource::FallbackAmbiguous:
    case ClimateRegionSource::FallbackUnconfigured:
        return GeographicEvidenceTier::Fallback;
    }
    // Safe default for any later unrecognized source variant.
    return GeographicEvidenceTier::Fallback;
}

std::string_view GeographicEvidenceTierName(GeographicEvidenceTier tier) noexcept {
    switch (tier) {
    case GeographicEvidenceTier::Fallback: return "fallback";
    case GeographicEvidenceTier::Provisional: return "provisional";
    case GeographicEvidenceTier::AnchorProximity: return "anchor-proximity";
    case GeographicEvidenceTier::NamedLocation: return "named-location";
    }
    return "unknown";
}

bool IsWeakTemperatureEvidence(ClimateRegionSource source) noexcept {
    const auto tier = ClassifyGeographicEvidence(source);
    return tier == GeographicEvidenceTier::Fallback || tier == GeographicEvidenceTier::Provisional;
}

GeographicEvidenceChange CompareGeographicEvidence(
    ClimateRegionSource previous, ClimateRegionSource current, bool comparable) noexcept {
    if (!comparable) return GeographicEvidenceChange::NotComparable;
    if (previous == current) return GeographicEvidenceChange::SameSource;
    const auto before = ClassifyGeographicEvidence(previous);
    const auto after = ClassifyGeographicEvidence(current);
    if (before == after) return GeographicEvidenceChange::SameTierDifferentSource;
    // Tier ordering is useful only to describe a transition in evidence
    // provenance; it is not a probability or a geographic containment score.
    return static_cast<int>(after) < static_cast<int>(before) ?
        GeographicEvidenceChange::TowardWeaker : GeographicEvidenceChange::TowardStronger;
}

std::string_view GeographicEvidenceChangeName(GeographicEvidenceChange value) noexcept {
    switch (value) {
    case GeographicEvidenceChange::NotComparable: return "not-comparable";
    case GeographicEvidenceChange::SameSource: return "same-source";
    case GeographicEvidenceChange::SameTierDifferentSource: return "same-tier-source-change";
    case GeographicEvidenceChange::TowardWeaker: return "toward-weaker-provenance";
    case GeographicEvidenceChange::TowardStronger: return "toward-stronger-provenance";
    }
    return "unknown";
}

} // namespace aetherclime
