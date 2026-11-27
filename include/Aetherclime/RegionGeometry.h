#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace aetherclime {

// Pure, portable 2D geometry. Coordinates are Skyrim exterior world UNITS,
// not meters. No game records or engine state are modified by these helpers.
struct RegionPoint2 {
    double x{0};
    double y{0};
};

using RegionPolygon = std::vector<RegionPoint2>;

// A polygon test is independent of the Skyrim engine's weather-region
// priority, override behavior, edge fall-off and chosen active weather.
enum class PolygonRelation {
    Outside,
    Inside,
    Boundary,
    Invalid
};

[[nodiscard]] inline bool IsFinitePoint(const RegionPoint2& p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

// Geometric boundary detection prevents treating a point on an edge as
// unambiguously inside or outside. This is only a numerical tolerance in
// raw game units, not Skyrim's configurable region edge fall-off distance.
[[nodiscard]] inline bool OnPolygonEdge(
    const RegionPoint2& p, const RegionPoint2& a, const RegionPoint2& b,
    double tolerance = 0.01) noexcept {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 == 0.0) {
        return std::hypot(p.x - a.x, p.y - a.y) <= tolerance;
    }
    const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy)) <= tolerance;
}

// Standard even-odd point-in-polygon, independent of winding order.
// Does not attempt Skyrim's undocumented handling of overlapping contours,
// interior holes or edge fall-off. Such cases must be validated in-game.
[[nodiscard]] inline PolygonRelation TestPolygon(
    const RegionPolygon& polygon, const RegionPoint2& point) noexcept {
    if (!IsFinitePoint(point) || polygon.size() < 3) return PolygonRelation::Invalid;
    for (const auto& vertex : polygon) {
        if (!IsFinitePoint(vertex)) return PolygonRelation::Invalid;
    }

    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto& a = polygon[j];
        const auto& b = polygon[i];
        if (OnPolygonEdge(point, a, b)) return PolygonRelation::Boundary;
        if ((a.y > point.y) != (b.y > point.y)) {
            const double intersectionX = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (point.x < intersectionX) inside = !inside;
        }
    }
    return inside ? PolygonRelation::Inside : PolygonRelation::Outside;
}

enum class RegionFootprintStatus {
    InsideRecordedPolygon,
    OutsideRecordedPolygons,
    OnRecordedBoundary,
    IncompleteGeometry,
    MissingGeometry,
    MissingPosition,
    WorldspaceMismatch,
    MissingWorldspace
};

[[nodiscard]] inline const char* RegionFootprintName(RegionFootprintStatus status) noexcept {
    switch (status) {
    case RegionFootprintStatus::InsideRecordedPolygon: return "inside-recorded-polygon";
    case RegionFootprintStatus::OutsideRecordedPolygons: return "outside-recorded-polygons";
    case RegionFootprintStatus::OnRecordedBoundary: return "on-recorded-boundary";
    case RegionFootprintStatus::IncompleteGeometry: return "incomplete-geometry";
    case RegionFootprintStatus::MissingGeometry: return "missing-geometry";
    case RegionFootprintStatus::MissingPosition: return "missing-position";
    case RegionFootprintStatus::WorldspaceMismatch: return "worldspace-mismatch";
    case RegionFootprintStatus::MissingWorldspace: return "missing-worldspace";
    }
    return "unknown";
}

struct RegionGeometryAssessment {
    RegionFootprintStatus status{RegionFootprintStatus::MissingGeometry};
    std::size_t polygonCount{0};
    std::size_t vertexCount{0};
    bool incomplete{false};
};

[[nodiscard]] inline RegionGeometryAssessment AssessRegionFootprint(
    const std::vector<RegionPolygon>& polygons,
    const std::optional<RegionPoint2>& position,
    std::optional<bool> worldspaceMatches,
    bool incompleteInput = false) noexcept {
    RegionGeometryAssessment result;
    result.polygonCount = polygons.size();
    result.incomplete = incompleteInput;
    for (const auto& polygon : polygons) result.vertexCount += polygon.size();
    if (!worldspaceMatches.has_value()) {
        result.status = RegionFootprintStatus::MissingWorldspace;
        return result;
    }
    if (!*worldspaceMatches) {
        result.status = RegionFootprintStatus::WorldspaceMismatch;
        return result;
    }
    if (!position || !IsFinitePoint(*position)) {
        result.status = RegionFootprintStatus::MissingPosition;
        return result;
    }
    if (polygons.empty()) {
        result.status = incompleteInput ? RegionFootprintStatus::IncompleteGeometry
                                        : RegionFootprintStatus::MissingGeometry;
        return result;
    }
    bool inside = false;
    bool boundary = false;
    for (const auto& polygon : polygons) {
        const auto relation = TestPolygon(polygon, *position);
        inside |= relation == PolygonRelation::Inside;
        boundary |= relation == PolygonRelation::Boundary;
        result.incomplete |= relation == PolygonRelation::Invalid;
    }
    // A boundary is ambiguous even if another overlapping polygon contains
    // the player. Treat partially read or malformed geometry as uncertain.
    if (result.incomplete) result.status = RegionFootprintStatus::IncompleteGeometry;
    else if (boundary) result.status = RegionFootprintStatus::OnRecordedBoundary;
    else if (inside) result.status = RegionFootprintStatus::InsideRecordedPolygon;
    else result.status = RegionFootprintStatus::OutsideRecordedPolygons;
    return result;
}

} // namespace aetherclime
