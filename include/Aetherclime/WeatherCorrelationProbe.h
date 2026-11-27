#pragma once

#include <cstdint>
#include <optional>

namespace aetherclime {

// Alpha-11: snapshots of RE::Sky public fields, not proof of weather-region
// selection, causality, or completion of the engine's weather transition.
// Acquired only by the explicitly enabled, two-second movement sampler.
struct WeatherSkyReading {
    bool exterior{false};
    bool skyAvailable{false};
    std::uint32_t worldspaceFormId{0};
    std::uint32_t currentWeatherFormId{0};
    std::uint32_t lastWeatherFormId{0};
    std::uint32_t overrideWeatherFormId{0};
    std::uint32_t skyRegionPointerFormId{0};
    float currentWeatherPct{0.0f};
};

enum class WeatherCorrelationStatus {
    None,
    Baseline,
    WeatherIdsChanged,
    SkyRegionPointerChanged,
    BoundarySampled,
    BoundaryAndWeatherIdsChanged,
    FollowUp10s,
    FollowUp30s,
    ResetUnavailable,
    ResetWorldspace,
    ResetGeography
};

[[nodiscard]] inline const char* WeatherCorrelationStatusName(WeatherCorrelationStatus value) noexcept {
    switch (value) {
    case WeatherCorrelationStatus::None: return "none";
    case WeatherCorrelationStatus::Baseline: return "baseline";
    case WeatherCorrelationStatus::WeatherIdsChanged: return "weather-ids-changed";
    case WeatherCorrelationStatus::SkyRegionPointerChanged: return "sky-region-pointer-changed";
    case WeatherCorrelationStatus::BoundarySampled: return "boundary-sampled";
    case WeatherCorrelationStatus::BoundaryAndWeatherIdsChanged: return "boundary-and-weather-ids-changed";
    case WeatherCorrelationStatus::FollowUp10s: return "follow-up-10s";
    case WeatherCorrelationStatus::FollowUp30s: return "follow-up-30s";
    case WeatherCorrelationStatus::ResetUnavailable: return "reset-unavailable";
    case WeatherCorrelationStatus::ResetWorldspace: return "reset-worldspace";
    case WeatherCorrelationStatus::ResetGeography: return "reset-geography";
    }
    return "unknown";
}

struct WeatherCorrelationDecision {
    WeatherCorrelationStatus status{WeatherCorrelationStatus::None};
    bool report{false};
    bool exhausted{false};
    bool weatherIdsChanged{false};
    bool skyRegionPointerChanged{false};
    bool blendValueValid{false};
    std::uint32_t boundaryTransitions{0};
    std::uint32_t previousCurrentWeatherFormId{0};
    std::uint32_t previousLastWeatherFormId{0};
    double secondsSinceLastCrossing{-1.0};
    std::uint32_t totalReports{0};
};

// All timestamps use seconds since the current game observation epoch.
// Reads have bounded reporting: baseline, weather ID/sky region pointer changes,
// crossings, and at most two timed post-crossing observations (10/30s).
// Timed follow-ups work even when the player stops moving. Never performs
// region discovery, modifies weather, or receives game engine pointers.
class WeatherCorrelationProbe {
public:
    explicit WeatherCorrelationProbe(std::uint32_t maxReports = 64) noexcept;
    void Reset() noexcept;
    [[nodiscard]] WeatherCorrelationDecision Observe(
        const WeatherSkyReading& reading, double secondsSinceSessionStart,
        std::uint32_t sampledBoundaryTransitions = 0,
        bool geographyDiscontinuity = false) noexcept;
    [[nodiscard]] std::uint32_t Reports() const noexcept { return reports_; }

private:
    std::optional<WeatherSkyReading> previous_;
    double lastCrossingTime_{-1.0};
    unsigned nextFollowUp_{0};
    std::uint32_t maxReports_{64};
    std::uint32_t reports_{0};
    bool exhausted_{false};
};

} // namespace aetherclime
