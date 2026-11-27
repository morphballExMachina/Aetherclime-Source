#pragma once

#include <cmath>
#include <optional>
#include <string_view>

namespace aetherclime {

// World-position diagnostics are deliberately distinct from elevation.
// Skyrim stores coordinates in engine units, with a worldspace-dependent
// reference origin. Z must not become a climate lapse-rate input until both
// a unit conversion and a reference altitude have been calibrated.
struct RawPosition {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

enum class PositionStatus {
    Available,
    UnknownCell,
    InteriorLocalOnly,
    MissingWorldspace,
    NonFinite
};

struct PositionDiagnostic {
    PositionStatus status{PositionStatus::UnknownCell};
    std::optional<RawPosition> worldUnits;
};

[[nodiscard]] inline PositionDiagnostic CheckWorldPosition(
    bool hasCell, bool interior, bool hasWorldspace, RawPosition raw) noexcept {
    if (!hasCell) return {PositionStatus::UnknownCell, std::nullopt};
    if (interior) return {PositionStatus::InteriorLocalOnly, std::nullopt};
    if (!hasWorldspace) return {PositionStatus::MissingWorldspace, std::nullopt};
    if (!std::isfinite(raw.x) || !std::isfinite(raw.y) || !std::isfinite(raw.z)) {
        return {PositionStatus::NonFinite, std::nullopt};
    }
    return {PositionStatus::Available, raw};
}

[[nodiscard]] inline std::string_view PositionStatusName(PositionStatus status) noexcept {
    switch (status) {
    case PositionStatus::Available: return "exterior-world-coordinates";
    case PositionStatus::UnknownCell: return "no-cell";
    case PositionStatus::InteriorLocalOnly: return "interior-local-coordinates-excluded";
    case PositionStatus::MissingWorldspace: return "no-worldspace";
    case PositionStatus::NonFinite: return "invalid-coordinates";
    }
    return "unknown";
}

}  // namespace aetherclime
