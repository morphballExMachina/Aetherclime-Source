#include "Aetherclime/RegionEdgeAudit.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

using namespace aetherclime;

namespace {
void Near(double actual, double expected, double tolerance = 0.001) {
    assert(std::fabs(actual - expected) < tolerance);
}
}

int main() {
    using S = EdgeEvidenceStatus;
    using P = RPLIProximity;
    const RegionPolygonWithRPLI square{{{0,0},{10,0},{10,10},{0,10}}, 3};
    const auto center = AssessRecordedEdgeProximity({square}, RegionPoint2{5,5}, true);
    assert(center.status == S::Measured && center.measured);
    Near(center.nearestSegmentDistance, 5);
    assert(center.nearestPolygonIndex == 0 && center.nearestPolygonRPLI == 3);
    assert(center.nearestPolygonRelation == PolygonRelation::Inside);
    assert(center.proximity == P::BeyondRecordedDistance);

    const auto closeInside = AssessRecordedEdgeProximity({square}, RegionPoint2{9,5}, true);
    Near(closeInside.nearestSegmentDistance, 1);
    assert(closeInside.proximity == P::WithinRecordedDistance);

    const auto outside = AssessRecordedEdgeProximity({square}, RegionPoint2{12,5}, true);
    Near(outside.nearestSegmentDistance, 2);
    assert(outside.nearestPolygonRelation == PolygonRelation::Outside);
    assert(outside.proximity == P::OutsideNearestPolygon); // don't infer exterior blending

    const auto boundary = AssessRecordedEdgeProximity({square}, RegionPoint2{10,5}, true);
    Near(boundary.nearestSegmentDistance, 0);
    assert(boundary.status == S::OnRecordedBoundary);
    assert(boundary.proximity == P::WithinRecordedDistance);

    const RegionPolygonWithRPLI second{{{20,0},{30,0},{30,10},{20,10}}, 0};
    const auto multi = AssessRecordedEdgeProximity({square,second}, RegionPoint2{25,5}, true);
    assert(multi.nearestPolygonIndex == 1 && multi.nearestPolygonRPLI == 0);
    Near(multi.nearestSegmentDistance, 5);
    assert(multi.proximity == P::ZeroRecordedDistance);

    // Multi-polygon data must use the nearest polygon's *own* RPLI.
    const auto between = AssessRecordedEdgeProximity({square, second}, RegionPoint2{15,5}, true);
    assert(between.nearestPolygonIndex == 0); // stable tie order
    assert(between.proximity == P::OutsideNearestPolygon);

    // Degenerate/repeated segments and reversed winding must remain safe.
    const RegionPolygonWithRPLI closed{{{0,0},{4,0},{4,4},{0,4},{0,0}}, 2};
    const auto closedAssessment = AssessRecordedEdgeProximity({closed}, RegionPoint2{2,2}, true);
    assert(closedAssessment.measured);
    Near(closedAssessment.nearestSegmentDistance, 2);
    const RegionPolygonWithRPLI reversed{{{0,10},{10,10},{10,0},{0,0}}, 3};
    Near(AssessRecordedEdgeProximity({reversed}, RegionPoint2{5,5}, true).nearestSegmentDistance, 5);

    assert(AssessRecordedEdgeProximity({}, RegionPoint2{1,1}, true).status == S::MissingGeometry);
    assert(AssessRecordedEdgeProximity({square}, std::nullopt, true).status == S::MissingPosition);
    assert(AssessRecordedEdgeProximity({square}, RegionPoint2{1,1}, false).status == S::WorldspaceMismatch);
    assert(AssessRecordedEdgeProximity({square}, RegionPoint2{1,1}, std::nullopt).status == S::MissingWorldspace);
    assert(AssessRecordedEdgeProximity({square}, RegionPoint2{1,1}, true, true).status == S::IncompleteGeometry);
    assert(AssessRecordedEdgeProximity({{{{0,0},{1,1}}, 10}}, RegionPoint2{1,1}, true).status == S::IncompleteGeometry);
    assert(AssessRecordedEdgeProximity({square}, RegionPoint2{std::numeric_limits<double>::infinity(),0}, true).status == S::MissingPosition);
    assert(AssessRecordedEdgeProximity({{{{0,0},{std::numeric_limits<double>::quiet_NaN(),0},{0,4}}, 10}}, RegionPoint2{1,1}, true).status == S::IncompleteGeometry);

    // Published SSEEdit rectangle + player's previously logged Winterhold position.
    const RegionPolygonWithRPLI winterhold{{
        {101940.0156,93910.7734},{115059.2734,93910.7734},
        {115059.2734,111101.5391},{101940.0156,111101.5391}}, 0};
    const auto winterholdResult = AssessRecordedEdgeProximity(
        {winterhold}, RegionPoint2{109542.4,104511.7}, true);
    assert(winterholdResult.measured && winterholdResult.nearestPolygonRelation == PolygonRelation::Inside);
    Near(winterholdResult.nearestSegmentDistance, 5516.8734, 0.01);

    std::cout << "Region recorded-edge portable tests passed.\n";
}
