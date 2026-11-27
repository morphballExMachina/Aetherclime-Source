#pragma once

#include "Aetherclime/Climate.h"
#include "Aetherclime/ClimateRegionResolver.h"
#include <optional>
#include <string>
#include <cstdint>
#include <string_view>

namespace aetherclime {
// Secondary, explicitly estimated values. These must NEVER be quietly
// promoted into the legacy V1/V2 trusted/verified data contracts.
enum class OutdoorEstimateBasis : std::uint32_t {
    Unavailable, TrustedRegional, ProvisionalRegional, NativeBiome,
    NearbyBiome, BroadTamriel, GenericWorldspace, NearbyClimateReference
};
std::string_view OutdoorEstimateBasisName(OutdoorEstimateBasis) noexcept;
struct OutdoorEstimate {
    OutdoorEstimateBasis basis{OutdoorEstimateBasis::Unavailable};
    std::optional<double> airC;
    bool regionTrusted{false};
    bool weatherVerified{false};
    // Informational only; never establishes regional containment or weather trust.
    std::string referenceLabel;
    double referenceDistanceRawUnits{-1.0};
    double referenceInfluence{0.0};
    std::uint32_t contributingReferences{0};
};

// Called only after a settled cell sample. biomePreviewC must originate from
// native polygon evidence rather than an unverified region name. No weather
// verification or hold identity is implied by any fallback tier.
OutdoorEstimate EstimateOutdoorClimate(
    const Config&, const Sample&, const Reading&, bool trustedRegion,
    bool provisionalRegion, std::optional<double> biomePreviewC,
    bool biomeNearby, const std::optional<ClimateWorldPoint>& worldPosition);

} // namespace aetherclime
