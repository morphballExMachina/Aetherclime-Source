#pragma once

#include "Aetherclime/RegionTransitionProbe.h"
#include "Aetherclime/WeatherCorrelationProbe.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace aetherclime {

// Alpha-15: correlate pointer switches with observed crossings of the EXACT
// old/new pointer FormIDs. Crossing evidence is not a weather selection rule.
enum class RegionSpecificStatus {
    None,
    Baseline,
    PointerChanged,
    RelatedCrossing,
    PointerAndCrossing,
    ResetUnavailable,
    ResetGeography,
    ResetWorldspace,
    ResetClock
};

[[nodiscard]] inline const char* RegionSpecificStatusName(RegionSpecificStatus status) noexcept {
    switch (status) {
    case RegionSpecificStatus::None: return "none";
    case RegionSpecificStatus::Baseline: return "baseline";
    case RegionSpecificStatus::PointerChanged: return "pointer-changed";
    case RegionSpecificStatus::RelatedCrossing: return "related-region-crossed";
    case RegionSpecificStatus::PointerAndCrossing: return "pointer-and-crossing-same-sample";
    case RegionSpecificStatus::ResetUnavailable: return "reset-unavailable";
    case RegionSpecificStatus::ResetGeography: return "reset-geography";
    case RegionSpecificStatus::ResetWorldspace: return "reset-worldspace";
    case RegionSpecificStatus::ResetClock: return "reset-clock";
    }
    return "unknown";
}

struct RegionSpecificCorrelationInput {
    WeatherSkyReading sky{};
    std::uint32_t cellFormId{0};
    std::span<const BoundaryTransition> sampledCrossings{}; // borrowed only for Observe()
    bool geographyDiscontinuity{false};
    double sessionSeconds{-1.0};
};

// The most recently sampled crossing of one SPECIFIC region. "Available"
// means historical evidence exists; "nearInTime" is a descriptive <=10s
// window, not proof that one event caused the other.
struct RegionSpecificCrossingEvidence {
    bool available{false};
    RegionFootprintStatus from{RegionFootprintStatus::MissingGeometry};
    RegionFootprintStatus to{RegionFootprintStatus::MissingGeometry};
    double secondsBeforeCurrentSample{-1.0};
    bool nearInTime{false};
};

struct RegionSpecificRelatedCrossing {
    bool available{false};
    std::uint32_t regionFormId{0};
    RegionFootprintStatus from{RegionFootprintStatus::MissingGeometry};
    RegionFootprintStatus to{RegionFootprintStatus::MissingGeometry};
    bool matchesCurrentPointer{false};
    bool matchesLatestSwitchFrom{false};
    bool matchesLatestSwitchTo{false};
    double secondsAfterPointerSwitch{-1.0}; // only if latest switch involved this region
};

struct RegionSpecificCorrelationDecision {
    RegionSpecificStatus status{RegionSpecificStatus::None};
    bool report{false};
    bool exhausted{false};
    bool pointerChanged{false};
    bool cellChanged{false};
    bool weatherPairChanged{false};
    bool sameSampleCrossingAndPointer{false};
    std::uint32_t previousPointer{0};
    std::uint32_t currentPointer{0};
    std::uint32_t previousCell{0};
    std::uint32_t cell{0};
    std::uint32_t allCrossingCount{0};
    // On a pointer change: compare only crossings of the actual from/to IDs,
    // NOT the most recently crossed polygon in general.
    RegionSpecificCrossingEvidence previousPointerCrossing{};
    RegionSpecificCrossingEvidence currentPointerCrossing{};
    // At most two distinct region IDs can be relevant to the last pointer switch.
    std::array<RegionSpecificRelatedCrossing, 2> relatedCrossings{};
    std::uint32_t relatedCrossingCount{0};
    double sampleGapSeconds{-1.0};
    double secondsSinceLastPointerSwitch{-1.0};
    std::uint32_t reports{0};
};

// O(32 + crossings) worst case at the existing *experimental* 2-second
// sampler, and no allocations or retained engine pointers. History is
// bounded to 32 distinct region IDs and reset across discontinuities,
// interiors, worldspace changes, and game loads.
class RegionSpecificCorrelationProbe {
public:
    explicit RegionSpecificCorrelationProbe(std::uint32_t maxReports = 48) noexcept;
    void Reset() noexcept;
    [[nodiscard]] RegionSpecificCorrelationDecision Observe(const RegionSpecificCorrelationInput& in) noexcept;

private:
    struct Sample {
        std::uint32_t worldspace{0};
        std::uint32_t cell{0};
        std::uint32_t pointer{0};
        std::uint32_t currentWeather{0};
        std::uint32_t lastWeather{0};
        double seconds{0.0};
    };
    struct RegionHistory {
        std::uint32_t region{0};
        RegionFootprintStatus from{RegionFootprintStatus::MissingGeometry};
        RegionFootprintStatus to{RegionFootprintStatus::MissingGeometry};
        double seconds{0.0};
    };
    std::optional<Sample> previous_;
    std::array<RegionHistory, 32> history_{};
    std::uint32_t latestSwitchFrom_{0};
    std::uint32_t latestSwitchTo_{0};
    double latestSwitchSeconds_{-1.0};
    std::uint32_t maxReports_{48};
    std::uint32_t reports_{0};
    bool exhausted_{false};

    void ClearHistory() noexcept;
    void Remember(const BoundaryTransition& crossing, double atSeconds) noexcept;
    [[nodiscard]] RegionSpecificCrossingEvidence LastCrossing(std::uint32_t region,
                                                               double now) const noexcept;
};

} // namespace aetherclime
