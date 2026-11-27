#include "Aetherclime/ElevationModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aetherclime {

std::string_view ElevationStatusName(ElevationStatus status) noexcept {
    switch (status) {
    case ElevationStatus::Eligible: return "eligible-relative-reference";
    case ElevationStatus::ExcludedInterior: return "excluded-interior";
    case ElevationStatus::MissingExteriorCoordinates: return "missing-exterior-coordinates";
    case ElevationStatus::MissingCalibration: return "no-calibration-for-profile-worldspace";
    case ElevationStatus::OutsideCalibrationNeighborhood: return "outside-calibration-neighborhood";
    case ElevationStatus::ConflictingCalibrations: return "ambiguous-equidistant-references";
    case ElevationStatus::InvalidCoordinates: return "invalid-coordinates";
    case ElevationStatus::UnknownProfile: return "unknown-climate-profile";
    }
    return "unknown";
}

ElevationAssessment AssessElevation(const Config& config, const ClimateRegionContext& context,
                                     std::string_view resolvedProfile) {
    ElevationAssessment out;
    out.enabled = config.elevation.enabled;
    out.profileKey = std::string(resolvedProfile);
    const auto region = config.regions.find(out.profileKey);
    if (region == config.regions.end()) {
        out.status = ElevationStatus::UnknownProfile;
        return out;
    }
    if (context.interior) {
        out.status = ElevationStatus::ExcludedInterior;
        return out;
    }
    if (!context.hasCell || !context.exteriorPosition || !context.exteriorPosition->zRaw) {
        out.status = ElevationStatus::MissingExteriorCoordinates;
        return out;
    }
    const auto& pos = *context.exteriorPosition;
    if (!std::isfinite(pos.xRaw) || !std::isfinite(pos.yRaw) ||
        !std::isfinite(*pos.zRaw)) {
        out.status = ElevationStatus::InvalidCoordinates;
        return out;
    }
    out.rawZ = *pos.zRaw;
    const ElevationReference* best = nullptr;
    double bestDistance = std::numeric_limits<double>::infinity();
    bool candidate = false;
    bool ambiguous = false;
    // Defensively bounded in addition to Config validation.
    const auto limit = std::min<std::size_t>(config.elevation.references.size(), 32);
    for (std::size_t i = 0; i < limit; ++i) {
        const auto& ref = config.elevation.references[i];
        if (ref.regionKey != resolvedProfile || ref.worldspaceFormId != pos.worldspaceFormId ||
            !std::isfinite(ref.xRaw) || !std::isfinite(ref.yRaw) ||
            !std::isfinite(ref.zRaw) || !std::isfinite(ref.radiusRawUnits) ||
            ref.radiusRawUnits <= 0.0) continue;
        candidate = true;
        const auto dist = std::hypot(pos.xRaw - ref.xRaw, pos.yRaw - ref.yRaw);
        if (!std::isfinite(dist) || dist > ref.radiusRawUnits) continue;
        if (dist < bestDistance - 0.01) {
            bestDistance = dist;
            best = &ref;
            ambiguous = false;
        } else if (std::abs(dist - bestDistance) <= 0.01 && best &&
                   std::abs(ref.zRaw - best->zRaw) > 0.01) {
            ambiguous = true;
        }
    }
    if (!best) {
        out.status = candidate ? ElevationStatus::OutsideCalibrationNeighborhood
                               : ElevationStatus::MissingCalibration;
        return out;
    }
    if (ambiguous) {
        out.status = ElevationStatus::ConflictingCalibrations;
        return out;
    }
    if (!(std::isfinite(config.elevation.rawUnitsPerMeter) && config.elevation.rawUnitsPerMeter > 0.0 &&
          std::isfinite(config.elevation.maxRiseMeters) && config.elevation.maxRiseMeters > 0.0 &&
          std::isfinite(config.elevation.maxCoolingC) && config.elevation.maxCoolingC > 0.0 &&
          std::isfinite(region->second.altitudeLapseCPerKm) &&
          region->second.altitudeLapseCPerKm >= 0.0)) {
        out.status = ElevationStatus::InvalidCoordinates;
        return out;
    }
    out.status = ElevationStatus::Eligible;
    out.eligible = true;
    out.referenceLabel = best->label;
    out.referenceZRaw = best->zRaw;
    out.distanceXYRaw = bestDistance;
    out.riseMeters = (out.rawZ - out.referenceZRaw) / config.elevation.rawUnitsPerMeter;
    const auto rise = std::max(0.0, out.riseMeters);
    const auto lapse = region->second.altitudeLapseCPerKm;
    const auto capByCooling = lapse > 0.0 ? (config.elevation.maxCoolingC * 1000.0 / lapse) : rise;
    out.effectiveRiseMeters = std::min({rise, config.elevation.maxRiseMeters, capByCooling});
    out.clamped = out.effectiveRiseMeters < rise;
    out.projectedOffsetC = -lapse * out.effectiveRiseMeters / 1000.0;
    out.applied = config.elevation.enabled;
    out.actualOffsetC = out.applied ? out.projectedOffsetC : 0.0;
    return out;
}

} // namespace aetherclime
