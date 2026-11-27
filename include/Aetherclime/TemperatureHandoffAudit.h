#pragma once

#include "Aetherclime/ClimateRegionResolver.h"

#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {

// Alpha-30: read-only observation of modeled air-temperature discontinuities
// at climate-profile / geographic-provenance handoffs. NOT a smoothing model.
// Neither trust flags, published temperatures, nor weather are modified.
enum class TemperatureHandoffStatus : std::uint8_t {
    NoExteriorEvidence,
    Baseline,
    DifferentWorldspace,
    DifferentCalendarDay,
    InvalidTime,
    TooOld,
    TooFar,
    WeatherChanged,
    Unchanged,
    SourceChangedSameProfile,
    ProfileChanged,
    SignificantProfileStep
};

[[nodiscard]] std::string_view TemperatureHandoffStatusName(TemperatureHandoffStatus) noexcept;

struct TemperatureHandoffSample {
    bool exterior{false};
    std::optional<ClimateWorldPoint> position{};
    Date date{};
    std::string regionKey;
    ClimateRegionSource source{ClimateRegionSource::FallbackNoEvidence};
    Weather weather{Weather::Unknown};
    double outdoorAirC{0.0}; // existing model result, never adjusted
};

struct TemperatureHandoffAssessment {
    TemperatureHandoffStatus status{TemperatureHandoffStatus::NoExteriorEvidence};
    double distanceRawUnits{-1.0};
    double ageGameHours{-1.0};
    double airDeltaC{0.0}; // current - previous, only for comparable samples
    std::optional<ClimateWorldPoint> previousPosition{}; // prior valid exterior; diagnostic only
    double previousOutdoorAirC{0.0}; // actual preceding sampled model reading, if available
    bool previousReadingAvailable{false};
    bool sourceChanged{false};
    bool profileChanged{false};
    bool significant{false};
    bool comparable{false};
    ClimateRegionSource previousSource{ClimateRegionSource::FallbackNoEvidence};
    std::string previousRegion;
};

class TemperatureHandoffAudit {
public:
    // These are DIAGNOSTIC adjacency limits, not climatic or hold boundaries.
    static constexpr double kMaxSampleDistanceRawUnits = 8192.0;
    static constexpr double kMaxSampleAgeGameHours = 0.5;
    static constexpr double kSignificantStepC = 4.0;

    void Reset() noexcept { previous_.reset(); }
    [[nodiscard]] TemperatureHandoffAssessment Observe(const TemperatureHandoffSample& current);

private:
    std::optional<TemperatureHandoffSample> previous_{};
};

} // namespace aetherclime
