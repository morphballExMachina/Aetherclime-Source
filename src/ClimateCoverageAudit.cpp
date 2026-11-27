#include "Aetherclime/ClimateCoverageAudit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>

namespace aetherclime {

std::string_view ClimateCoverageStatusName(ClimateCoverageStatus status) noexcept {
    switch (status) {
    case ClimateCoverageStatus::ExcludedNoCell: return "excluded-no-cell";
    case ClimateCoverageStatus::ExcludedInterior: return "excluded-interior";
    case ClimateCoverageStatus::ExcludedNoWorldPosition: return "excluded-no-world-position";
    case ClimateCoverageStatus::ExcludedInvalidPosition: return "excluded-invalid-position";
    case ClimateCoverageStatus::Disabled: return "geography-disabled";
    case ClimateCoverageStatus::NoReferenceForWorldspace: return "no-reference-in-worldspace";
    case ClimateCoverageStatus::OutsideSampleNeighborhoods: return "outside-sample-neighborhoods";
    case ClimateCoverageStatus::WithinOneReference: return "inside-one-sample-neighborhood";
    case ClimateCoverageStatus::WithinSameProfileOverlap: return "overlap-same-profile";
    case ClimateCoverageStatus::ConflictingProfileOverlap: return "overlap-conflicting-profiles";
    }
    return "unknown";
}

ClimateCoverageAudit AuditClimateCoverage(
    const Config& config, const ClimateRegionContext& context) {
    ClimateCoverageAudit out;
    if (!context.hasCell) return out;
    if (context.interior) {
        out.status = ClimateCoverageStatus::ExcludedInterior;
        return out;
    }
    if (!context.exteriorPosition || context.exteriorPosition->worldspaceFormId == 0) {
        out.status = ClimateCoverageStatus::ExcludedNoWorldPosition;
        return out;
    }
    const auto& point = *context.exteriorPosition;
    if (!std::isfinite(point.xRaw) || !std::isfinite(point.yRaw) ||
        (point.zRaw && !std::isfinite(*point.zRaw))) {
        out.status = ClimateCoverageStatus::ExcludedInvalidPosition;
        return out;
    }
    out.worldspaceFormId = point.worldspaceFormId;
    out.xRaw = point.xRaw;
    out.yRaw = point.yRaw;
    if (point.zRaw) {
        out.rawZAvailable = true;
        out.zRaw = *point.zRaw;
    }
    if (!config.geographicFallbackEnabled) {
        out.status = ClimateCoverageStatus::Disabled;
        return out;
    }

    double nearestCenter = std::numeric_limits<double>::infinity();
    double nearestBoundary = std::numeric_limits<double>::infinity();
    // Config anchor count is validated <=64 by Climate; this independent
    // audit also bounds iteration to 64 even for raw test/config callers.
    std::size_t visited = 0;
    std::array<std::string_view, 64> inRangeProfileKeys{};
    for (const auto& anchor : config.geographicAnchors) {
        if (++visited > 64) break;
        if (anchor.worldspaceFormId != point.worldspaceFormId ||
            !std::isfinite(anchor.xRaw) || !std::isfinite(anchor.yRaw) ||
            !std::isfinite(anchor.radiusRawUnits) || anchor.radiusRawUnits <= 0.0) continue;
        const double dist = std::hypot(point.xRaw - anchor.xRaw, point.yRaw - anchor.yRaw);
        if (!std::isfinite(dist)) continue;
        ++out.worldspaceAnchors;
        const double boundary = std::abs(dist - anchor.radiusRawUnits);
        nearestBoundary = std::min(nearestBoundary, boundary);
        if (dist < nearestCenter) {
            nearestCenter = dist;
            out.nearestAnchorLabel = anchor.label;
            out.nearestRegionKey = anchor.regionKey;
            out.nearestDistanceRawUnits = dist;
            out.nearestRadiusRawUnits = anchor.radiusRawUnits;
            out.nearestSignedMarginRawUnits = anchor.radiusRawUnits - dist;
        }
        if (dist <= anchor.radiusRawUnits) {
            ++out.inRangeAnchors;
            // Count distinct profiles, not circles. Stack storage is bounded
            // to the existing 64-anchor maximum; no heap or nested XY scans.
            const std::string_view key = anchor.regionKey;
            if (std::find(inRangeProfileKeys.begin(),
                          inRangeProfileKeys.begin() + out.distinctInRangeProfiles,
                          key) == inRangeProfileKeys.begin() + out.distinctInRangeProfiles) {
                inRangeProfileKeys[out.distinctInRangeProfiles++] = key;
            }
        }
    }
    if (out.worldspaceAnchors == 0) {
        out.status = ClimateCoverageStatus::NoReferenceForWorldspace;
        return out;
    }
    out.nearestBoundaryDistanceRawUnits = nearestBoundary;
    // Fixed engineering threshold for a SAMPLE-circle edge, not a hold
    // border. Independent of radius so configurations remain comparable.
    out.nearSampleEdge = nearestBoundary <= 1024.0;
    if (out.inRangeAnchors == 0) {
        out.status = ClimateCoverageStatus::OutsideSampleNeighborhoods;
    } else if (out.distinctInRangeProfiles > 1) {
        out.status = ClimateCoverageStatus::ConflictingProfileOverlap;
    } else if (out.inRangeAnchors > 1) {
        out.status = ClimateCoverageStatus::WithinSameProfileOverlap;
    } else {
        out.status = ClimateCoverageStatus::WithinOneReference;
    }
    return out;
}

} // namespace aetherclime
