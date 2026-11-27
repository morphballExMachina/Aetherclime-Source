#pragma once

#include "Aetherclime/WeatherCorrelationProbe.h"

#include <cstdint>
#include <optional>

namespace aetherclime {

// Alpha-13: scalar copies of RE::Sky fields, sampled only through Alpha-9's
// opt-in, bounded movement cadence. A pointer is represented by a FormID;
// no game pointers or weather records are retained or modified.
enum class SkyLifecycleStatus {
    None,
    Baseline,
    WeatherPairChanged,
    LastPointerCleared,
    OverridePointerChanged,
    SkyRegionPointerChanged,
    BlendQuarterChanged,
    ResetUnavailable,
    ResetWorldspace,
    ResetGeography
};

[[nodiscard]] inline const char* SkyLifecycleStatusName(SkyLifecycleStatus value) noexcept {
    switch (value) {
    case SkyLifecycleStatus::None: return "none";
    case SkyLifecycleStatus::Baseline: return "baseline";
    case SkyLifecycleStatus::WeatherPairChanged: return "weather-pair-changed";
    case SkyLifecycleStatus::LastPointerCleared: return "last-pointer-cleared";
    case SkyLifecycleStatus::OverridePointerChanged: return "override-pointer-changed";
    case SkyLifecycleStatus::SkyRegionPointerChanged: return "sky-region-pointer-changed";
    case SkyLifecycleStatus::BlendQuarterChanged: return "blend-quarter-changed";
    case SkyLifecycleStatus::ResetUnavailable: return "reset-unavailable";
    case SkyLifecycleStatus::ResetWorldspace: return "reset-worldspace";
    case SkyLifecycleStatus::ResetGeography: return "reset-geography";
    }
    return "unknown";
}

enum class SkyWeatherPairState {
    Unavailable,
    CurrentOnly,
    CurrentAndLast
};

[[nodiscard]] inline const char* SkyWeatherPairStateName(SkyWeatherPairState state) noexcept {
    switch (state) {
    case SkyWeatherPairState::Unavailable: return "unavailable";
    case SkyWeatherPairState::CurrentOnly: return "current-only";
    case SkyWeatherPairState::CurrentAndLast: return "current-and-last";
    }
    return "unknown";
}

struct SkyLifecycleDecision {
    SkyLifecycleStatus status{SkyLifecycleStatus::None};
    SkyWeatherPairState pairState{SkyWeatherPairState::Unavailable};
    bool report{false};
    bool exhausted{false};
    bool pairChanged{false};
    bool lastPointerCleared{false};
    bool overrideChanged{false};
    bool skyRegionChanged{false};
    bool blendValid{false};
    bool blendQuarterChanged{false};
    int blendQuarter{-1}; // 0..4 only when valid; not an engine phase ID.
    int previousBlendQuarter{-1};
    std::uint32_t previousCurrent{0};
    std::uint32_t previousLast{0};
    std::uint32_t previousOverride{0};
    std::uint32_t previousSkyRegion{0};
    double secondsSincePairChange{-1.0};
    double secondsSincePreviousSample{-1.0};
    std::uint32_t totalReports{0};
};

// Reports only changes; blend progress is quantized into four coarse quarters
// to avoid per-sample logging. "Last pointer cleared" and "blend quarter"
// are observations, NOT assertions that the engine completed a transition.
// Invalid/interior/worldspace/teleport contexts reset the comparison state.
class SkyTransitionLifecycleProbe {
public:
    explicit SkyTransitionLifecycleProbe(std::uint32_t maxReports = 48) noexcept;
    void Reset() noexcept;
    [[nodiscard]] SkyLifecycleDecision Observe(
        const WeatherSkyReading& reading, double elapsedSeconds,
        bool geographyDiscontinuity = false) noexcept;
    [[nodiscard]] std::uint32_t Reports() const noexcept { return reports_; }

private:
    std::optional<WeatherSkyReading> previous_;
    double previousSampleTime_{-1.0};
    double weatherPairStartTime_{-1.0};
    std::uint32_t maxReports_{48};
    std::uint32_t reports_{0};
    bool exhausted_{false};
};

} // namespace aetherclime
