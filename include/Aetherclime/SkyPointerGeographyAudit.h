#pragma once

#include "Aetherclime/RegionGeometry.h"
#include "Aetherclime/RegionEdgeAudit.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace aetherclime {

// Alpha-12: READ-ONLY copies of the weather region geometry and headers from
// the current exterior CELL's XCLR region list. Never retain game pointers.
// This type also serves the Alpha-10 cell cache, avoiding duplicate scans.
struct SkyPointerCandidateGeometry {
    std::uint32_t regionFormId{0};
    std::uint32_t worldspaceFormId{0};
    std::uint8_t priority{0};
    bool overrideFlag{false};
    std::vector<RegionPolygon> polygons;
    bool incomplete{false};
};

enum class SkyPointerRelation {
    CacheUnavailable,
    PointerNull,
    PointerNotListed,
    InsideRecordedPolygon,
    OutsideRecordedPolygons,
    OnRecordedBoundary,
    UncertainGeometry
};

[[nodiscard]] inline const char* SkyPointerRelationName(SkyPointerRelation status) noexcept {
    switch (status) {
    case SkyPointerRelation::CacheUnavailable: return "cache-unavailable";
    case SkyPointerRelation::PointerNull: return "pointer-null";
    case SkyPointerRelation::PointerNotListed: return "pointer-not-in-cell-candidates";
    case SkyPointerRelation::InsideRecordedPolygon: return "inside-recorded-polygon";
    case SkyPointerRelation::OutsideRecordedPolygons: return "outside-recorded-polygons";
    case SkyPointerRelation::OnRecordedBoundary: return "on-recorded-boundary";
    case SkyPointerRelation::UncertainGeometry: return "uncertain-geometry";
    }
    return "unknown";
}

struct SkyPointerGeographyAudit {
    SkyPointerRelation pointerRelation{SkyPointerRelation::CacheUnavailable};
    bool cacheComplete{false};
    bool pointerListed{false};
    std::uint8_t pointerPriority{0};
    bool pointerOverride{false};
    std::size_t totalCandidates{0};
    std::size_t insideCount{0};
    std::size_t outsideCount{0};
    std::size_t uncertainCount{0};
    std::size_t topPriorityCandidateCount{0};
    // A geometric priority HYPOTHESIS, not evidence of engine selection.
    std::uint32_t priorityLeaderFormId{0};
    std::uint8_t priorityLeaderPriority{0};
    bool pointerMatchesPriorityLeader{false};
    bool pointerEdgeMeasured{false};
    double pointerNearestRecordedEdgeRawUnits{0.0};
};

// Run only when the already-opted-in weather correlation probe EMITS a report.
// Assesses cached record geometry; no engine calls, weather selection or RPLI
// fall-off model. An unlisted Sky pointer is *not* treated as an error: CELL
// XCLR candidates need not enumerate all engine state involved in selection.
[[nodiscard]] inline SkyPointerGeographyAudit AuditSkyPointerGeography(
    std::span<const SkyPointerCandidateGeometry> regions,
    std::uint32_t skyPointerRegionFormId,
    const std::optional<RegionPoint2>& position,
    std::uint32_t playerWorldspaceFormId,
    bool candidateSnapshotComplete) noexcept {
    SkyPointerGeographyAudit audit;
    audit.totalCandidates = regions.size();
    if (!candidateSnapshotComplete || !position || !IsFinitePoint(*position) ||
        playerWorldspaceFormId == 0) {
        return audit;
    }
    audit.cacheComplete = true;
    audit.pointerRelation = skyPointerRegionFormId == 0
        ? SkyPointerRelation::PointerNull : SkyPointerRelation::PointerNotListed;
    const SkyPointerCandidateGeometry* top = nullptr;
    const SkyPointerCandidateGeometry* pointer = nullptr;
    RegionFootprintStatus pointerFootprint = RegionFootprintStatus::MissingGeometry;
    double pointerEdge = std::numeric_limits<double>::infinity();

    for (const auto& region : regions) {
        const auto relation = AssessRegionFootprint(
            region.polygons, position,
            region.worldspaceFormId != 0
                ? std::optional<bool>{region.worldspaceFormId == playerWorldspaceFormId}
                : std::nullopt,
            region.incomplete);
        if (relation.status == RegionFootprintStatus::InsideRecordedPolygon) {
            ++audit.insideCount;
            if (!top || region.priority > top->priority) {
                top = &region;
                audit.topPriorityCandidateCount = 1;
            } else if (region.priority == top->priority) {
                ++audit.topPriorityCandidateCount;
            }
        } else if (relation.status == RegionFootprintStatus::OutsideRecordedPolygons ||
                   relation.status == RegionFootprintStatus::WorldspaceMismatch) {
            ++audit.outsideCount;
        } else {
            ++audit.uncertainCount;
        }
        if (skyPointerRegionFormId != 0 && region.regionFormId == skyPointerRegionFormId) {
            pointer = &region;
            pointerFootprint = relation.status;
        }
    }

    // Do not report a priority leader when geometry is incomplete/ambiguous
    // or multiple recorded inside candidates have the same top priority.
    if (audit.uncertainCount == 0 && top && audit.topPriorityCandidateCount == 1) {
        audit.priorityLeaderFormId = top->regionFormId;
        audit.priorityLeaderPriority = top->priority;
    }
    if (!pointer) return audit;

    audit.pointerListed = true;
    audit.pointerPriority = pointer->priority;
    audit.pointerOverride = pointer->overrideFlag;
    audit.pointerMatchesPriorityLeader = audit.priorityLeaderFormId != 0 &&
        audit.priorityLeaderFormId == skyPointerRegionFormId;
    switch (pointerFootprint) {
    case RegionFootprintStatus::InsideRecordedPolygon:
        audit.pointerRelation = SkyPointerRelation::InsideRecordedPolygon;
        break;
    case RegionFootprintStatus::OutsideRecordedPolygons:
        audit.pointerRelation = SkyPointerRelation::OutsideRecordedPolygons;
        break;
    case RegionFootprintStatus::OnRecordedBoundary:
        audit.pointerRelation = SkyPointerRelation::OnRecordedBoundary;
        break;
    default:
        audit.pointerRelation = SkyPointerRelation::UncertainGeometry;
        break;
    }
    if ((audit.pointerRelation == SkyPointerRelation::InsideRecordedPolygon ||
         audit.pointerRelation == SkyPointerRelation::OutsideRecordedPolygons ||
         audit.pointerRelation == SkyPointerRelation::OnRecordedBoundary) &&
        !pointer->incomplete) {
        // Distance to the nearest *recorded* segment, NOT to the outer edge
        // of a polygon union or any inferred engine fall-off boundary.
        for (const auto& polygon : pointer->polygons) {
            if (polygon.size() < 3) continue;
            for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
                const double d = DistanceToRecordedSegment(*position, polygon[j], polygon[i]);
                if (std::isfinite(d) && d < pointerEdge) pointerEdge = d;
            }
        }
        if (std::isfinite(pointerEdge)) {
            audit.pointerEdgeMeasured = true;
            audit.pointerNearestRecordedEdgeRawUnits = pointerEdge;
        }
    }
    return audit;
}

} // namespace aetherclime
