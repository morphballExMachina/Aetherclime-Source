#include "Aetherclime/RegionAtlasDataAudit.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {
namespace {
// Atlas resolution uses inclusive [min,max] bounds. A shared edge or corner
// therefore overlaps and can be ambiguous for a character standing on it.
[[nodiscard]] bool Intersects(const ProvisionalAtlasZone& a,
                              const ProvisionalAtlasZone& b) noexcept {
    return a.worldspaceFormId == b.worldspaceFormId &&
           a.minXRaw <= b.maxXRaw && b.minXRaw <= a.maxXRaw &&
           a.minYRaw <= b.maxYRaw && b.minYRaw <= a.maxYRaw;
}

[[nodiscard]] bool Intersects(const ProvisionalAtlasZone& zone,
                              const ProvisionalClimateSample& point) noexcept {
    if (zone.worldspaceFormId != point.worldspaceFormId) return false;
    const double x = std::clamp(point.xRaw, zone.minXRaw, zone.maxXRaw);
    const double y = std::clamp(point.yRaw, zone.minYRaw, zone.maxYRaw);
    return std::hypot(point.xRaw - x, point.yRaw - y) <= point.radiusRawUnits;
}
} // namespace

std::vector<AtlasDataFinding> AuditAtlasData(const Config& config) {
    std::vector<AtlasDataFinding> findings;
    const auto& zones = config.provisionalAtlasZones;
    for (std::size_t i = 0; i < zones.size(); ++i) {
        const auto& a = zones[i];
        for (std::size_t j = i + 1; j < zones.size(); ++j) {
            const auto& b = zones[j];
            if (a.worldspaceFormId != b.worldspaceFormId) continue;
            if (a.label == b.label) {
                findings.push_back({AtlasDataFindingKind::DuplicateAtlasLabel,
                    a.label, b.label, a.worldspaceFormId});
            }
            if (Intersects(a, b)) {
                findings.push_back({a.regionKey != b.regionKey
                        ? AtlasDataFindingKind::ConflictingAtlasProfiles
                        : AtlasDataFindingKind::SameProfileAtlasOverlap,
                    a.label, b.label, a.worldspaceFormId});
            }
        }
        // Conflicting provisional survey circles are independently guarded
        // by the existing resolver. Surface them BEFORE shipping the data.
        for (const auto& sample : config.provisionalSurveySamples) {
            if (a.regionKey != sample.regionKey && Intersects(a, sample)) {
                findings.push_back({AtlasDataFindingKind::ConflictingAtlasSurvey,
                    a.label, sample.label, a.worldspaceFormId});
            }
        }
    }
    return findings;
}

} // namespace aetherclime
