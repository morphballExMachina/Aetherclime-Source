#include "Aetherclime/RegionGeometry.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

using namespace aetherclime;

int main() {
    const RegionPolygon winterhold = {
        {101940.0156, 93910.7734}, {115059.2734, 93910.7734},
        {115059.2734, 111101.5391}, {101940.0156, 111101.5391}
    };
    assert(TestPolygon(winterhold, {109542.4, 104511.8}) == PolygonRelation::Inside);
    assert(TestPolygon(winterhold, {90000.0, 104511.8}) == PolygonRelation::Outside);
    assert(TestPolygon(winterhold, {101940.0156, 100000.0}) == PolygonRelation::Boundary);
    assert(TestPolygon(winterhold, {101940.0156, 93910.7734}) == PolygonRelation::Boundary);
    const RegionPolygon reversed{winterhold.rbegin(), winterhold.rend()};
    assert(TestPolygon(reversed, {109542.4, 104511.8}) == PolygonRelation::Inside);

    const RegionPolygon falkreath = {{-10, -10}, {0, -10}, {0, 0}, {-10, 0}};
    assert(TestPolygon(falkreath, {-5, -5}) == PolygonRelation::Inside);
    assert(TestPolygon(falkreath, {5, -5}) == PolygonRelation::Outside);
    const RegionPolygon concave = {{0, 0}, {6, 0}, {6, 2}, {2, 2}, {2, 6}, {0, 6}};
    assert(TestPolygon(concave, {1, 5}) == PolygonRelation::Inside);
    assert(TestPolygon(concave, {5, 5}) == PolygonRelation::Outside);
    const RegionPolygon closed{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
    assert(TestPolygon(closed, {2, 2}) == PolygonRelation::Inside);

    const auto inside = AssessRegionFootprint({winterhold}, RegionPoint2{109542.4,104511.8}, true);
    assert(inside.status == RegionFootprintStatus::InsideRecordedPolygon);
    assert(inside.polygonCount == 1 && inside.vertexCount == 4 && !inside.incomplete);
    assert(AssessRegionFootprint({winterhold}, RegionPoint2{1, 1}, true).status ==
           RegionFootprintStatus::OutsideRecordedPolygons);
    assert(AssessRegionFootprint({winterhold}, RegionPoint2{109542.4,104511.8}, false).status ==
           RegionFootprintStatus::WorldspaceMismatch);
    assert(AssessRegionFootprint({winterhold}, RegionPoint2{109542.4,104511.8}, std::nullopt).status ==
           RegionFootprintStatus::MissingWorldspace);
    assert(AssessRegionFootprint({winterhold}, std::nullopt, true).status ==
           RegionFootprintStatus::MissingPosition);
    assert(AssessRegionFootprint({}, RegionPoint2{0,0}, true).status ==
           RegionFootprintStatus::MissingGeometry);
    assert(AssessRegionFootprint({winterhold, falkreath}, RegionPoint2{-5,-5}, true).status ==
           RegionFootprintStatus::InsideRecordedPolygon);
    assert(AssessRegionFootprint({winterhold}, RegionPoint2{109542.4,104511.8}, true, true).status ==
           RegionFootprintStatus::IncompleteGeometry);
    assert(AssessRegionFootprint({{{1,1},{2,2}}}, RegionPoint2{0,0}, true).status ==
           RegionFootprintStatus::IncompleteGeometry);
    assert(AssessRegionFootprint({winterhold}, RegionPoint2{std::numeric_limits<double>::quiet_NaN(), 0}, true).status ==
           RegionFootprintStatus::MissingPosition);
    std::cout << "Region geometry portable tests passed.\n";
}
