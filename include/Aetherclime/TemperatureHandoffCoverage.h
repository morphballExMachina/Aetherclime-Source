#pragma once

#include "Aetherclime/GeographicEvidence.h"
#include "Aetherclime/TemperatureHandoffAudit.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace aetherclime {

// Alpha-33: diagnostic labels only. A model delta observed across two event
// samples is not proof that geography CAUSED that delta.
enum class TemperatureHandoffStepClass : std::uint8_t {
    NotComparable,
    SameProfile,
    ProfileStepBelowThreshold,
    SignificantTowardStronger,
    SignificantTowardWeaker,
    SignificantOther
};
[[nodiscard]] TemperatureHandoffStepClass ClassifyTemperatureHandoffStep(
    const TemperatureHandoffAssessment& assessment,
    ClimateRegionSource currentSource) noexcept;
[[nodiscard]] std::string_view TemperatureHandoffStepClassName(TemperatureHandoffStepClass) noexcept;

// All values are session-local event samples. These are NOT percentages of
// Skyrim mapped or sampled cells, and route counts are not spatial boundaries.
struct TemperatureHandoffCoverageSnapshot {
    std::uint64_t observations{0};
    std::uint64_t baselines{0};
    std::uint64_t invalidExteriorSamples{0};
    std::uint64_t rejectedComparisons{0};
    std::uint64_t comparablePairs{0};
    std::uint64_t fallbackSamples{0};
    std::uint64_t provisionalSamples{0};
    std::uint64_t anchorSamples{0};
    std::uint64_t namedSamples{0};
    std::uint64_t rejectedWorldspace{0};
    std::uint64_t rejectedDayOrTime{0};
    std::uint64_t rejectedDistanceOrAge{0};
    std::uint64_t rejectedWeather{0};
    std::uint64_t sourceSwitchSameProfile{0};
    std::uint64_t profileStepsBelowThreshold{0};
    std::uint64_t significantProfileSteps{0};
    std::uint64_t significantTowardStronger{0};
    std::uint64_t significantTowardWeaker{0};
    std::uint64_t significantOther{0};
    std::uint64_t fallbackToAnchor{0};
    std::uint64_t anchorToFallback{0};
    std::uint32_t uniqueTierRoutes{0};
    bool hasPeakProfileStep{false};
    double peakSignedProfileStepC{0.0};
    double peakDistanceRawUnits{0.0};
    double peakAgeGameHours{0.0};
    ClimateRegionSource peakPreviousSource{ClimateRegionSource::FallbackNoEvidence};
    ClimateRegionSource peakCurrentSource{ClimateRegionSource::FallbackNoEvidence};
    std::string peakPreviousRegion;
    std::string peakCurrentRegion;
};

class TemperatureHandoffCoverage {
public:
    void Reset() noexcept { data_ = {}; routesMask_ = 0; }
    void Record(const TemperatureHandoffSample& current, const TemperatureHandoffAssessment& assessment);
    [[nodiscard]] const TemperatureHandoffCoverageSnapshot& Snapshot() const noexcept { return data_; }

private:
    TemperatureHandoffCoverageSnapshot data_{};
    std::uint16_t routesMask_{0}; // bit index = previous tier * 4 + current tier
};

// Reserve the majority of the original 48 temperature-audit lines for profile
// steps, baselines, or explicit save/load. Routine same-profile source flicker
// cannot consume the remaining diagnostic budget. Counting never depends on
// whether a line is emitted.
class TemperatureHandoffReportBudget {
public:
    static constexpr std::uint32_t kTotalLimit = 48;
    static constexpr std::uint32_t kRoutineLimit = 18;
    void Reset() noexcept { total_ = 0; routine_ = 0; }
    [[nodiscard]] bool Allow(bool reportable, bool priority) noexcept;
    [[nodiscard]] std::uint32_t TotalEmitted() const noexcept { return total_; }
    [[nodiscard]] std::uint32_t RoutineEmitted() const noexcept { return routine_; }

private:
    std::uint32_t total_{0};
    std::uint32_t routine_{0};
};

} // namespace aetherclime
