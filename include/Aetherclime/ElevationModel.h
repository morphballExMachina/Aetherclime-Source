#pragma once

#include "Aetherclime/Climate.h"
#include "Aetherclime/ClimateRegionResolver.h"

#include <string>
#include <string_view>

namespace aetherclime {

enum class ElevationStatus {
    Eligible, ExcludedInterior, MissingExteriorCoordinates, MissingCalibration,
    OutsideCalibrationNeighborhood, ConflictingCalibrations, InvalidCoordinates,
    UnknownProfile
};

[[nodiscard]] std::string_view ElevationStatusName(ElevationStatus status) noexcept;

struct ElevationAssessment {
    ElevationStatus status{ElevationStatus::MissingExteriorCoordinates};
    bool enabled{false};
    bool eligible{false};
    bool applied{false};
    bool clamped{false};
    std::string profileKey;
    std::string referenceLabel;
    double rawZ{0.0};
    double referenceZRaw{0.0};
    double distanceXYRaw{0.0};
    double riseMeters{0.0};          // signed, relative to reference, not sea level
    double effectiveRiseMeters{0.0}; // nonnegative after cap, passed to Climate::Calculate
    double projectedOffsetC{0.0};    // nonpositive, physically modeled correction
    double actualOffsetC{0.0};       // zero unless enabled and eligible
};

// This never modifies records or interprets world Z as absolute elevation.
// If multiple equally-near calibration references disagree, it declines to
// apply an elevation correction. Only matching worldspace+climate keys qualify.
[[nodiscard]] ElevationAssessment AssessElevation(
    const Config& config, const ClimateRegionContext& context,
    std::string_view resolvedProfile);

} // namespace aetherclime
