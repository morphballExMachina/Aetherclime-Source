#pragma once

#include "Aetherclime/ClimateRegionResolver.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {

// Alpha-26: deliberately PROVISIONAL bridge across short, unlabelled exterior
// gaps. This is not an authoritative hold boundary, weather observation, or
// a reason to mark a climate region trusted. The last verified observation
// is fixed: a provisional result can NEVER renew or extend the anchor.
enum class ContinuityStatus : std::uint8_t {
    DirectEvidence, Disabled, NoVerifiedOrigin, NoExteriorPosition,
    ExplicitAmbiguity, DifferentWorldspace, DifferentCalendarDay,
    Expired, TooFar, Provisional, SurveySampleProvisional, AtlasZoneProvisional
};
[[nodiscard]] std::string_view ContinuityStatusName(ContinuityStatus status) noexcept;

struct ContinuityAssessment {
    ClimateRegionResolution resolution;
    ContinuityStatus status{ContinuityStatus::NoVerifiedOrigin};
    double distanceRawUnits{-1.0};
    double ageGameHours{-1.0};
    bool provisional() const noexcept { return resolution.provisional(); }
};

class RegionalContinuity {
public:
    static constexpr double kMaxDistanceRawUnits = 12288.0; // 3 exterior cells, NOT a hold radius
    static constexpr double kMaxAgeGameHours = 2.0;

    void Reset() noexcept { anchor_.reset(); }
    // Never observe an interior, ambiguous/unconfigured source, or a previous
    // provisional result. Called ONLY on verified exterior load/save/events.
    [[nodiscard]] bool ObserveVerified(const ClimateRegionResolution& direct,
                                      const ClimateRegionContext& context,
                                      const Date& date);
    [[nodiscard]] ContinuityAssessment Assess(const ClimateRegionResolution& direct,
                                             const ClimateRegionContext& context,
                                             const Date& date,
                                             bool enabled) const;
private:
    struct Anchor {
        std::string region;
        ClimateWorldPoint point;
        Date date;
    };
    std::optional<Anchor> anchor_;
};

} // namespace aetherclime
