#pragma once

#include <optional>
#include <string_view>

namespace aetherclime {

// This is a classification of Skyrim's CELL record, not a claim about
// the building's insulation, altitude, heating, or whether it has a roof.
enum class CellEnvironment {
    Unknown,
    Interior,
    Exterior
};

// Possible means outdoor weather could affect the player. An exterior cell
// can still have physical shelter, which we do not detect in v0.2.2.
enum class OutdoorWeatherExposure {
    Unknown,
    NotApplicable,
    Possible
};

struct EnvironmentContext {
    CellEnvironment cell = CellEnvironment::Unknown;
    OutdoorWeatherExposure outdoorWeatherExposure = OutdoorWeatherExposure::Unknown;
};

// A missing CELL must stay unknown; do not mistake it for an interior.
[[nodiscard]] constexpr EnvironmentContext ClassifyEnvironment(
    std::optional<bool> isInteriorCell) noexcept {
    if (!isInteriorCell.has_value()) {
        return {};
    }
    if (*isInteriorCell) {
        return {CellEnvironment::Interior, OutdoorWeatherExposure::NotApplicable};
    }
    return {CellEnvironment::Exterior, OutdoorWeatherExposure::Possible};
}

[[nodiscard]] constexpr std::string_view ToString(CellEnvironment kind) noexcept {
    switch (kind) {
    case CellEnvironment::Interior: return "interior";
    case CellEnvironment::Exterior: return "exterior";
    default: return "unknown";
    }
}

[[nodiscard]] constexpr std::string_view ToString(OutdoorWeatherExposure exposure) noexcept {
    switch (exposure) {
    case OutdoorWeatherExposure::NotApplicable: return "not-applicable";
    case OutdoorWeatherExposure::Possible: return "possible";
    default: return "unknown";
    }
}

} // namespace aetherclime
