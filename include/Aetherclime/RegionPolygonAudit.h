#pragma once

#include "Aetherclime/RegionEdgeAudit.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace aetherclime {

// Alpha-8: independent, read-only census of *every* recorded RPLI polygon.
// The region-wide Alpha-7 nearest-edge result remains unchanged. Individual
// polygon boundaries are NOT the same as the combined/union region boundary.
struct PolygonAuditEntry {
    std::size_t polygonIndex{0};        // zero-based in memory, one-based in logs
    std::size_t vertexCount{0};
    std::uint32_t recordedRPLI{0};
    PolygonRelation relation{PolygonRelation::Invalid};
    double nearestRecordedSegment{0.0};
    RPLIProximity proximity{RPLIProximity::NotComparable};
    bool measured{false};
};

struct RegionPolygonAudit {
    EdgeEvidenceStatus status{EdgeEvidenceStatus::MissingGeometry};
    std::vector<PolygonAuditEntry> polygons;
    std::size_t insideCount{0};
    std::size_t outsideCount{0};
    std::size_t boundaryCount{0};
    std::size_t nonZeroRPLICount{0};
};

[[nodiscard]] inline const char* PolygonRelationName(PolygonRelation relation) noexcept {
    switch (relation) {
    case PolygonRelation::Inside: return "inside";
    case PolygonRelation::Outside: return "outside";
    case PolygonRelation::Boundary: return "boundary";
    case PolygonRelation::Invalid: return "not-measured";
    }
    return "not-measured";
}

// Geometry is trusted only when ALL polygon inputs are complete, the worldspace
// agrees and the exterior point is finite. Even if geometry is unavailable,
// we preserve a census of RPLI values / vertex counts for read-only inspection.
[[nodiscard]] inline RegionPolygonAudit AssessEveryRecordedPolygon(
    const std::vector<RegionPolygonWithRPLI>& input,
    const std::optional<RegionPoint2>& position,
    std::optional<bool> worldspaceMatches,
    bool incompleteInput = false) {
    RegionPolygonAudit result;
    result.polygons.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        PolygonAuditEntry entry;
        entry.polygonIndex = i;
        entry.vertexCount = input[i].points.size();
        entry.recordedRPLI = input[i].distanceInsideAtMax;
        if (entry.recordedRPLI != 0) ++result.nonZeroRPLICount;
        result.polygons.push_back(entry);
    }
    if (input.empty()) {
        result.status = incompleteInput ? EdgeEvidenceStatus::IncompleteGeometry
                                        : EdgeEvidenceStatus::MissingGeometry;
        return result;
    }
    if (incompleteInput) {
        result.status = EdgeEvidenceStatus::IncompleteGeometry;
        return result;
    }
    for (const auto& polygon : input) {
        if (polygon.points.size() < 3) {
            result.status = EdgeEvidenceStatus::IncompleteGeometry;
            return result;
        }
        for (const auto& vertex : polygon.points) {
            if (!IsFinitePoint(vertex)) {
                result.status = EdgeEvidenceStatus::IncompleteGeometry;
                return result;
            }
        }
    }
    if (!worldspaceMatches.has_value()) {
        result.status = EdgeEvidenceStatus::MissingWorldspace;
        return result;
    }
    if (!*worldspaceMatches) {
        result.status = EdgeEvidenceStatus::WorldspaceMismatch;
        return result;
    }
    if (!position || !IsFinitePoint(*position)) {
        result.status = EdgeEvidenceStatus::MissingPosition;
        return result;
    }

    // Validate all distances before publishing *any* measured status.
    // Prevents partly trustworthy diagnostics on malformed modded geometry.
    std::vector<double> distances;
    std::vector<PolygonRelation> relations;
    distances.reserve(input.size());
    relations.reserve(input.size());
    for (const auto& polygon : input) {
        const auto relation = TestPolygon(polygon.points, *position);
        if (relation == PolygonRelation::Invalid) {
            result.status = EdgeEvidenceStatus::IncompleteGeometry;
            return result;
        }
        double nearest = std::numeric_limits<double>::infinity();
        const auto& points = polygon.points;
        for (std::size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
            const double distance = DistanceToRecordedSegment(*position, points[j], points[i]);
            if (!std::isfinite(distance)) {
                result.status = EdgeEvidenceStatus::IncompleteGeometry;
                return result;
            }
            if (distance < nearest) nearest = distance;
        }
        distances.push_back(nearest);
        relations.push_back(relation);
    }
    result.status = EdgeEvidenceStatus::Measured;
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto& entry = result.polygons[i];
        entry.relation = relations[i];
        entry.nearestRecordedSegment = distances[i];
        entry.measured = true;
        if (entry.relation == PolygonRelation::Inside) ++result.insideCount;
        else if (entry.relation == PolygonRelation::Outside) ++result.outsideCount;
        else if (entry.relation == PolygonRelation::Boundary) ++result.boundaryCount;

        if (entry.relation == PolygonRelation::Outside) {
            entry.proximity = RPLIProximity::OutsideNearestPolygon;
        } else if (entry.recordedRPLI == 0) {
            entry.proximity = RPLIProximity::ZeroRecordedDistance;
        } else {
            entry.proximity = entry.nearestRecordedSegment <= static_cast<double>(entry.recordedRPLI)
                ? RPLIProximity::WithinRecordedDistance
                : RPLIProximity::BeyondRecordedDistance;
        }
        if (entry.relation == PolygonRelation::Boundary) {
            result.status = EdgeEvidenceStatus::OnRecordedBoundary;
        }
    }
    return result;
}

}  // namespace aetherclime
