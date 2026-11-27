#pragma once

#include "Aetherclime/WeatherCorrelationProbe.h"

#include <cstdint>
#include <optional>

namespace aetherclime {

// Alpha-14: chronology of *observations*, never a claim of event causality
// or the exact times of engine decisions between periodic samples.
enum class RegionPointerTimingStatus {
    None,
    Baseline,
    Event,
    ResetUnavailable,
    ResetGeography,
    ResetWorldspace,
    ResetClock
};

[[nodiscard]] inline const char* RegionPointerTimingStatusName(RegionPointerTimingStatus status) noexcept {
    switch (status) {
    case RegionPointerTimingStatus::None: return "none";
    case RegionPointerTimingStatus::Baseline: return "baseline";
    case RegionPointerTimingStatus::Event: return "event";
    case RegionPointerTimingStatus::ResetUnavailable: return "reset-unavailable";
    case RegionPointerTimingStatus::ResetGeography: return "reset-geography";
    case RegionPointerTimingStatus::ResetWorldspace: return "reset-worldspace";
    case RegionPointerTimingStatus::ResetClock: return "reset-clock";
    }
    return "unknown";
}

struct RegionPointerTimingInput {
    WeatherSkyReading sky{};   // scalar fields already sampled by Alpha-11
    std::uint32_t cellFormId{0};
    std::uint32_t sampledPolygonTransitions{0};
    bool geographyDiscontinuity{false};
    double sessionSeconds{-1.0};  // monotonic observation clock
};

struct RegionPointerTimingDecision {
    RegionPointerTimingStatus status{RegionPointerTimingStatus::None};
    bool report{false};
    bool exhausted{false};
    bool cellChanged{false};
    bool skyPointerChanged{false};
    bool weatherPairChanged{false};
    bool overrideChanged{false};
    bool simultaneousSample{false};  // multiple events seen at ONE sample, order unknown
    std::uint32_t sampledPolygonTransitions{0};
    std::uint32_t previousCellFormId{0};
    std::uint32_t previousSkyPointerFormId{0};
    double sampleGapSeconds{-1.0};
    // Event-pair intervals are between the timestamps of OBSERVED samples.
    // -1 means this observation session has no usable prior marker.
    double pointerSecondsAfterBoundary{-1.0};
    double pointerSecondsAfterCell{-1.0};
    double weatherSecondsAfterPointer{-1.0};
    double weatherSecondsAfterBoundary{-1.0};
    double boundarySecondsAfterPointer{-1.0};
    std::uint32_t totalReports{0};
};

// No Skyrim APIs, allocations, world scans or retained engine pointers.
// Called only by the existing opt-in, two-second movement/weather sampler.
// A stationary sample can still detect a Sky change. All history is cleared
// after interior, worldspace, clock or movement continuity resets.
class RegionPointerTimingProbe {
public:
    explicit RegionPointerTimingProbe(std::uint32_t maxReports = 48) noexcept;
    void Reset() noexcept;
    [[nodiscard]] RegionPointerTimingDecision Observe(const RegionPointerTimingInput& input) noexcept;
    [[nodiscard]] std::uint32_t Reports() const noexcept { return reports_; }

private:
    struct Previous {
        std::uint32_t worldspaceFormId{0};
        std::uint32_t cellFormId{0};
        std::uint32_t currentWeatherFormId{0};
        std::uint32_t lastWeatherFormId{0};
        std::uint32_t overrideWeatherFormId{0};
        std::uint32_t skyRegionFormId{0};
        double seconds{0.0};
    };
    std::optional<Previous> previous_;
    double lastCellTime_{-1.0};
    double lastBoundaryTime_{-1.0};
    double lastPointerTime_{-1.0};
    double lastWeatherPairTime_{-1.0};
    std::uint32_t maxReports_{48};
    std::uint32_t reports_{0};
    bool exhausted_{false};
};

} // namespace aetherclime
