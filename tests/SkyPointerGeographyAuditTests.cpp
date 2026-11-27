#include "Aetherclime/SkyPointerGeographyAudit.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

using namespace aetherclime;

namespace {
RegionPolygon Rectangle(double l, double b, double r, double t) {
    return {{l,b},{r,b},{r,t},{l,t}};
}
SkyPointerCandidateGeometry Region(std::uint32_t id, std::uint8_t priority,
                                   RegionPolygon polygon, bool overrideFlag = true) {
    SkyPointerCandidateGeometry region;
    region.regionFormId = id;
    region.worldspaceFormId = 0x3C;
    region.priority = priority;
    region.overrideFlag = overrideFlag;
    region.polygons.push_back(std::move(polygon));
    return region;
}
}

int main() {
    // Winterhold geometry matches the independently checked SSEEdit rectangle.
    constexpr std::uint32_t winterhold = 0x0010FDC6, snow = 0x000C5853, coast = 0x000C5852;
    const std::vector<SkyPointerCandidateGeometry> candidates{
        Region(winterhold, 65, Rectangle(101940.0156,93910.7734,115059.2734,111101.5391)),
        Region(snow, 60, Rectangle(100000,99000,116000,112000)),
        Region(coast, 50, Rectangle(110000,100000,118000,112000))
    };
    {
        // Near north edge: pointer is geometrically inside but lower priority.
        const auto r = AuditSkyPointerGeography(candidates, coast,
            RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.cacheComplete && r.pointerListed);
        assert(r.pointerRelation == SkyPointerRelation::InsideRecordedPolygon);
        assert(r.insideCount == 3 && r.uncertainCount == 0);
        assert(r.priorityLeaderFormId == winterhold && r.priorityLeaderPriority == 65);
        assert(r.pointerPriority == 50 && !r.pointerMatchesPriorityLeader);
        assert(r.pointerEdgeMeasured && r.pointerNearestRecordedEdgeRawUnits > 0.0);
    }
    {
        // After the north crossing, Winterhold outside; lower-priority region
        // can be the priority leader without claiming an engine rule.
        const auto r = AuditSkyPointerGeography(candidates, coast,
            RegionPoint2{114500.5,111658.6}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::InsideRecordedPolygon);
        assert(r.priorityLeaderFormId == snow);
        assert(!r.pointerMatchesPriorityLeader);
    }
    {
        const auto r = AuditSkyPointerGeography(candidates, winterhold,
            RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::InsideRecordedPolygon);
        assert(r.pointerMatchesPriorityLeader);
        assert(r.pointerNearestRecordedEdgeRawUnits > 0);
    }
    {
        const auto r = AuditSkyPointerGeography(candidates, winterhold,
            RegionPoint2{114500.5,111658.6}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::OutsideRecordedPolygons);
        assert(!r.pointerMatchesPriorityLeader);
    }
    {
        auto r = AuditSkyPointerGeography(candidates, 0, RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::PointerNull && !r.pointerListed);
        r = AuditSkyPointerGeography(candidates, 0xABCD, RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::PointerNotListed && !r.pointerListed);
        assert(r.priorityLeaderFormId == winterhold);
        r = AuditSkyPointerGeography(candidates, coast, RegionPoint2{114993.3,110840.5}, 0x3C, false);
        assert(r.pointerRelation == SkyPointerRelation::CacheUnavailable && !r.cacheComplete);
        r = AuditSkyPointerGeography(candidates, coast, std::nullopt, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::CacheUnavailable);
        r = AuditSkyPointerGeography(candidates, coast, RegionPoint2{114993.3,110840.5}, 0, true);
        assert(r.pointerRelation == SkyPointerRelation::CacheUnavailable);
        r = AuditSkyPointerGeography(candidates, coast,
            RegionPoint2{std::numeric_limits<double>::quiet_NaN(),110840.5}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::CacheUnavailable);
    }
    {
        auto ambiguous = candidates;
        ambiguous[1].priority = 65;
        auto r = AuditSkyPointerGeography(ambiguous, winterhold,
            RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.topPriorityCandidateCount == 2 && r.priorityLeaderFormId == 0);
        ambiguous[1].incomplete = true;
        r = AuditSkyPointerGeography(ambiguous, coast,
            RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.uncertainCount == 1 && r.priorityLeaderFormId == 0);
    }
    {
        auto malformed = candidates;
        malformed[2].polygons[0].clear();
        auto r = AuditSkyPointerGeography(malformed, coast,
            RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::UncertainGeometry);
        assert(!r.pointerEdgeMeasured && r.priorityLeaderFormId == 0);
        malformed = candidates;
        malformed[2].worldspaceFormId = 0x123;
        r = AuditSkyPointerGeography(malformed, coast,
            RegionPoint2{114993.3,110840.5}, 0x3C, true);
        assert(r.pointerRelation == SkyPointerRelation::UncertainGeometry);
        assert(r.outsideCount == 1 && r.pointerEdgeMeasured == false);
    }
    return 0;
}
