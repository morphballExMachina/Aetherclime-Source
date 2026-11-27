#pragma once

#include "Aetherclime/ClimateRegionResolver.h"

#include <cstdint>
#include <string_view>

namespace aetherclime {

// Alpha-32: categorical PROVENANCE, not calibrated statistical confidence.
// These tiers describe where a climate profile came from. They never assert
// that a coordinate lies inside a verified hold/REGN boundary.
enum class GeographicEvidenceTier : std::uint8_t {
    Fallback,
    Provisional,
    AnchorProximity,
    NamedLocation
};
[[nodiscard]] GeographicEvidenceTier ClassifyGeographicEvidence(ClimateRegionSource source) noexcept;
[[nodiscard]] std::string_view GeographicEvidenceTierName(GeographicEvidenceTier tier) noexcept;

// Only provisional and fallback destinations qualify as weak geography for
// the existing shadow-only continuity eligibility gate. This function must
// never upgrade ClimateRegionResolution::matched() or any API trust flag.
[[nodiscard]] bool IsWeakTemperatureEvidence(ClimateRegionSource source) noexcept;

enum class GeographicEvidenceChange : std::uint8_t {
    NotComparable,
    SameSource,
    SameTierDifferentSource,
    TowardWeaker,
    TowardStronger
};
[[nodiscard]] GeographicEvidenceChange CompareGeographicEvidence(
    ClimateRegionSource previous, ClimateRegionSource current, bool comparable) noexcept;
[[nodiscard]] std::string_view GeographicEvidenceChangeName(GeographicEvidenceChange value) noexcept;

} // namespace aetherclime
