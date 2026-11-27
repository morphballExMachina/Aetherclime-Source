#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace aetherclime {
// Presentation only: does not control the climate model or public API.
enum class HudMode : std::uint8_t { Off, Minimal, Detailed, Debug };
enum class HudTemperatureUnit : std::uint8_t { Celsius, Fahrenheit, Both };
struct HudSettings {
    HudMode mode{HudMode::Off};
    HudTemperatureUnit temperatureUnit{HudTemperatureUnit::Celsius};
    double cooldownSeconds{90.0}; // real elapsed seconds, not game hours
    double loadGraceSeconds{8.0};
    double sampleIntervalSeconds{2.0}; // minimum spacing between owned snapshots
    double temperatureDeltaC{3.0}; // Detailed/Debug; always Celsius difference
    double heartbeatSeconds{0.0}; // 0 = disabled; sampled events only
    bool sampleOnMenuClose{true}; // selected native UI close events; no timer
    bool showOnFirstObservation{true};
    bool showProvisionalEstimates{false};
    bool debugInitializationMessage{true};
};
std::string_view HudModeName(HudMode) noexcept;
std::optional<HudMode> ParseHudMode(std::string_view) noexcept;
std::string_view HudTemperatureUnitName(HudTemperatureUnit) noexcept;
std::optional<HudTemperatureUnit> ParseHudTemperatureUnit(std::string_view) noexcept;
bool ValidHudSettings(const HudSettings&) noexcept;
} // namespace aetherclime
