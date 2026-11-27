#pragma once

#include "Aetherclime/RegionGeometry.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace aetherclime {

// Recorded RPLI is named "distanceInsideAtMax" in CommonLibSSE-NG.
// These are geometric *observations*, not a weather-selection, weighting,
// interpolation or engine fall-off model. Each polygon owns its own RPLI.
struct RegionPolygonWithRPLI {
    RegionPolygon points;
    std::uint32_t distanceInsideAtMax{0};  // raw exterior game units (RPLI)
};

enum class EdgeEvidenceStatus {
    Measured,
    OnRecordedBoundary,
    MissingPosition,
    MissingGeometry,
    IncompleteGeometry,
    WorldspaceMismatch,
    MissingWorldspace
};

[[nodiscard]] inline const char* EdgeEvidenceStatusName(EdgeEvidenceStatus status) noexcept {
    switch (status) {
    case EdgeEvidenceStatus::Measured: return "measured";
    case EdgeEvidenceStatus::OnRecordedBoundary: return "on-recorded-boundary";
    case EdgeEvidenceStatus::MissingPosition: return "missing-position";
    case EdgeEvidenceStatus::MissingGeometry: return "missing-geometry";
    case EdgeEvidenceStatus::IncompleteGeometry: return "incomplete-geometry";
    case EdgeEvidenceStatus::WorldspaceMismatch: return "worldspace-mismatch";
    case EdgeEvidenceStatus::MissingWorldspace: return "missing-worldspace";
    }
    return "unknown";
}

enum class RPLIProximity {
    WithinRecordedDistance,
    BeyondRecordedDistance,
    ZeroRecordedDistance,
    OutsideNearestPolygon,
    NotComparable
};

[[nodiscard]] inline const char* RPLIProximityName(RPLIProximity status) noexcept {
    switch (status) {
    case RPLIProximity::WithinRecordedDistance: return "within-recorded-distance";
    case RPLIProximity::BeyondRecordedDistance: return "beyond-recorded-distance";
    case RPLIProximity::ZeroRecordedDistance: return "zero-rpli";
    case RPLIProximity::OutsideNearestPolygon: return "outside-nearest-polygon";
    case RPLIProximity::NotComparable: return "not-comparable";
    }
    return "unknown";
}

struct RegionEdgeEvidence {
    EdgeEvidenceStatus status{EdgeEvidenceStatus::MissingGeometry};
    double nearestSegmentDistance{0.0}; // always nonnegative; raw game units
    std::size_t nearestPolygonIndex{0};  // zero-based index when measured
    std::uint32_t nearestPolygonRPLI{0};
    PolygonRelation nearestPolygonRelation{PolygonRelation::Invalid};
    RPLIProximity proximity{RPLIProximity::NotComparable};
    bool measured{false};
};

[[nodiscard]] inline double DistanceToRecordedSegment(
    const RegionPoint2& point, const RegionPoint2& a,
    const RegionPoint2& b) noexcept {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 == 0.0) return std::hypot(point.x - a.x, point.y - a.y);
    if (!std::isfinite(len2)) return std::numeric_limits<double>::quiet_NaN();
    const double projection = std::clamp(
        ((point.x - a.x) * dx + (point.y - a.y) * dy) / len2,
        0.0, 1.0);
    return std::hypot(point.x - (a.x + projection * dx),
                      point.y - (a.y + projection * dy));
}

// Independent, defensive measurement of the closest *recorded line segment*
// among all polygon outlines. It does NOT compute a combined/union polygon
// boundary and does NOT assume linear or any other engine fall-off formula.
[[nodiscard]] inline RegionEdgeEvidence AssessRecordedEdgeProximity(
    const std::vector<RegionPolygonWithRPLI>& polygons,
    const std::optional<RegionPoint2>& position,
    std::optional<bool> worldspaceMatches,
    bool incompleteInput = false) noexcept {
    RegionEdgeEvidence result;
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
    if (polygons.empty()) {
        result.status = incompleteInput ? EdgeEvidenceStatus::IncompleteGeometry
                                        : EdgeEvidenceStatus::MissingGeometry;
        return result;
    }
    if (incompleteInput) {
        result.status = EdgeEvidenceStatus::IncompleteGeometry;
        return result;
    }

    double nearest = std::numeric_limits<double>::infinity();
    for (std::size_t polygonIndex = 0; polygonIndex < polygons.size(); ++polygonIndex) {
        const auto& polygon = polygons[polygonIndex].points;
        if (polygon.size() < 3) {
            result.status = EdgeEvidenceStatus::IncompleteGeometry;
            return result;
        }
        for (const auto& vertex : polygon) {
            if (!IsFinitePoint(vertex)) {
                result.status = EdgeEvidenceStatus::IncompleteGeometry;
                return result;
            }
        }
        for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
            const auto distance = DistanceToRecordedSegment(*position, polygon[j], polygon[i]);
            if (!std::isfinite(distance)) {
                result.status = EdgeEvidenceStatus::IncompleteGeometry;
                return result;
            }
            if (distance < nearest) {
                nearest = distance;
                result.nearestPolygonIndex = polygonIndex;
                result.nearestPolygonRPLI = polygons[polygonIndex].distanceInsideAtMax;
            }
        }
    }
    if (!std::isfinite(nearest)) {
        result.status = EdgeEvidenceStatus::IncompleteGeometry;
        return result;
    }
    result.nearestSegmentDistance = nearest;
    result.nearestPolygonRelation = TestPolygon(
        polygons[result.nearestPolygonIndex].points, *position);
    if (result.nearestPolygonRelation == PolygonRelation::Invalid) {
        result.status = EdgeEvidenceStatus::IncompleteGeometry;
        return result;
    }
    result.measured = true;
    result.status = nearest <= 0.01 ? EdgeEvidenceStatus::OnRecordedBoundary
                                    : EdgeEvidenceStatus::Measured;

    if (result.nearestPolygonRelation == PolygonRelation::Outside) {
        result.proximity = RPLIProximity::OutsideNearestPolygon;
    } else if (result.nearestPolygonRPLI == 0) {
        result.proximity = RPLIProximity::ZeroRecordedDistance;
    } else {
        // A numerical comparison only. Does not infer whether the engine
        // blends weather here, or how the RPLI field is interpreted.
        result.proximity = nearest <= static_cast<double>(result.nearestPolygonRPLI) ?
            RPLIProximity::WithinRecordedDistance : RPLIProximity::BeyondRecordedDistance;
    }
    return result;
}

} // namespace aetherclime
