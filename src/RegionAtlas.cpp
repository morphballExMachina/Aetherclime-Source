#include "Aetherclime/RegionAtlas.h"

#include <cmath>
#include <limits>

namespace aetherclime {
std::string_view AtlasStatusName(AtlasStatus status) noexcept {
    switch (status) {
    case AtlasStatus::Disabled: return "disabled";
    case AtlasStatus::NoExteriorPoint: return "no-valid-exterior-point";
    case AtlasStatus::OutsideCoverage: return "outside-atlas-coverage";
    case AtlasStatus::MatchedProvisional: return "provisional-atlas-zone";
    case AtlasStatus::ConflictingRegions: return "overlapping-atlas-regions";
    case AtlasStatus::UnconfiguredProfile: return "atlas-profile-unconfigured";
    }
    return "unknown";
}

AtlasAssessment EvaluateRegionAtlas(const Config& config,
                                    const ClimateRegionContext& context) noexcept {
    AtlasAssessment out;
    if (!config.geographicFallbackEnabled || !config.geographicAtlasEnabled) return out;
    if (!context.hasCell || context.interior || !context.exteriorPosition ||
        context.exteriorPosition->worldspaceFormId == 0 ||
        !std::isfinite(context.exteriorPosition->xRaw) ||
        !std::isfinite(context.exteriorPosition->yRaw)) {
        out.status = AtlasStatus::NoExteriorPoint;
        return out;
    }
    const auto& point = *context.exteriorPosition;
    out.status = AtlasStatus::OutsideCoverage;
    const ProvisionalAtlasZone* winner = nullptr;
    double smallestArea = std::numeric_limits<double>::infinity();
    bool conflicting = false;
    for (const auto& zone : config.provisionalAtlasZones) {
        if (zone.worldspaceFormId != point.worldspaceFormId ||
            !std::isfinite(zone.minXRaw) || !std::isfinite(zone.maxXRaw) ||
            !std::isfinite(zone.minYRaw) || !std::isfinite(zone.maxYRaw) ||
            zone.minXRaw >= zone.maxXRaw || zone.minYRaw >= zone.maxYRaw ||
            point.xRaw < zone.minXRaw || point.xRaw > zone.maxXRaw ||
            point.yRaw < zone.minYRaw || point.yRaw > zone.maxYRaw) continue;
        ++out.matchingZones;
        if (winner && winner->regionKey != zone.regionKey) conflicting = true;
        const double area = (zone.maxXRaw-zone.minXRaw) * (zone.maxYRaw-zone.minYRaw);
        if (!winner || area < smallestArea ||
            (area == smallestArea && zone.label < winner->label)) {
            winner = &zone;
            smallestArea = area;
        }
    }
    if (!winner) return out;
    if (conflicting) {
        out.status = AtlasStatus::ConflictingRegions;
        return out;
    }
    if (!config.regions.contains(winner->regionKey)) {
        out.status = AtlasStatus::UnconfiguredProfile;
        return out;
    }
    out.status = AtlasStatus::MatchedProvisional;
    out.regionKey = winner->regionKey;
    out.zoneLabel = winner->label;
    out.worldspaceFormId = winner->worldspaceFormId;
    out.minXRaw = winner->minXRaw;
    out.maxXRaw = winner->maxXRaw;
    out.minYRaw = winner->minYRaw;
    out.maxYRaw = winner->maxYRaw;
    return out;
}
} // namespace aetherclime
