#include "Aetherclime/RegionPolygonAudit.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
using namespace aetherclime;

void Check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

RegionPolygon Square(double x0, double y0, double x1, double y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

void TestFalkreathTwoPolygonRPLICensus() {
    const std::vector<RegionPolygonWithRPLI> p{
        {Square(0, 0, 10, 10), 1024},
        {Square(20, 0, 30, 10), 0}
    };
    const auto a = AssessEveryRecordedPolygon(p, RegionPoint2{25, 5}, true);
    Check(a.status == EdgeEvidenceStatus::Measured, "two-poly status");
    Check(a.polygons.size() == 2 && a.nonZeroRPLICount == 1, "all RPLI collected");
    Check(a.outsideCount == 1 && a.insideCount == 1 && a.boundaryCount == 0, "per polygon membership");
    Check(a.polygons[0].recordedRPLI == 1024 && a.polygons[0].relation == PolygonRelation::Outside, "first RPLI preserved");
    Check(a.polygons[0].proximity == RPLIProximity::OutsideNearestPolygon, "outside comparison remains conservative");
    Check(a.polygons[1].recordedRPLI == 0 && a.polygons[1].relation == PolygonRelation::Inside, "second RPLI preserved");
    Check(a.polygons[1].proximity == RPLIProximity::ZeroRecordedDistance, "zero is not a blend radius");
    Check(std::abs(a.polygons[1].nearestRecordedSegment - 5.0) < 0.0001, "second edge distance");
}

void TestInsideNonZeroRPLI() {
    const auto p = std::vector<RegionPolygonWithRPLI>{{Square(0,0,10,10), 6}};
    const auto near = AssessEveryRecordedPolygon(p, RegionPoint2{2,5}, true);
    Check(near.polygons[0].proximity == RPLIProximity::WithinRecordedDistance, "near RPLI observation");
    const auto beyond = AssessEveryRecordedPolygon(p, RegionPoint2{7,5}, true);
    // nearest distance = 3; exercise an RPLI smaller than the distance
    auto q = p;
    q[0].distanceInsideAtMax = 2;
    const auto far = AssessEveryRecordedPolygon(q, RegionPoint2{7,5}, true);
    Check(far.polygons[0].proximity == RPLIProximity::BeyondRecordedDistance, "far RPLI observation");
    Check(beyond.polygons[0].measured, "intermediate sample measured");
}

void TestOverlapAndBoundary() {
    const auto p = std::vector<RegionPolygonWithRPLI>{{Square(0,0,10,10), 10}, {Square(5,5,15,15), 0}};
    const auto overlap = AssessEveryRecordedPolygon(p, RegionPoint2{8,8}, true);
    Check(overlap.insideCount == 2, "overlapping polygons remain distinct");
    const auto boundary = AssessEveryRecordedPolygon(p, RegionPoint2{0,5}, true);
    Check(boundary.status == EdgeEvidenceStatus::OnRecordedBoundary, "boundary flagged");
    Check(boundary.boundaryCount == 1, "boundary count");
    Check(boundary.polygons[0].nearestRecordedSegment <= 0.01, "boundary distance");
}

void TestUntrustedInputsNeverProduceGeometry() {
    auto p = std::vector<RegionPolygonWithRPLI>{{Square(0,0,10,10), 1024}};
    const auto unknownWs = AssessEveryRecordedPolygon(p, RegionPoint2{5,5}, std::nullopt);
    Check(unknownWs.status == EdgeEvidenceStatus::MissingWorldspace && !unknownWs.polygons[0].measured, "unknown worldspace");
    Check(unknownWs.polygons[0].recordedRPLI == 1024, "census persists without position evaluation");
    const auto mismatch = AssessEveryRecordedPolygon(p, RegionPoint2{5,5}, false);
    Check(mismatch.status == EdgeEvidenceStatus::WorldspaceMismatch && !mismatch.polygons[0].measured, "mismatched worldspace");
    const auto absent = AssessEveryRecordedPolygon(p, std::nullopt, true);
    Check(absent.status == EdgeEvidenceStatus::MissingPosition && !absent.polygons[0].measured, "missing player position");
    const auto truncated = AssessEveryRecordedPolygon(p, RegionPoint2{5,5}, true, true);
    Check(truncated.status == EdgeEvidenceStatus::IncompleteGeometry && !truncated.polygons[0].measured, "defensive truncation");
    p.push_back({{{0,0},{10,0},{std::numeric_limits<double>::quiet_NaN(),10}},0});
    const auto invalid = AssessEveryRecordedPolygon(p, RegionPoint2{5,5}, true);
    Check(invalid.status == EdgeEvidenceStatus::IncompleteGeometry && !invalid.polygons[0].measured && !invalid.polygons[1].measured, "one malformed polygon invalidates whole sample");
}

void TestEmptyAndDegenerate() {
    const auto empty = AssessEveryRecordedPolygon({}, RegionPoint2{5,5}, true);
    Check(empty.status == EdgeEvidenceStatus::MissingGeometry, "empty geometry");
    const auto incomplete = AssessEveryRecordedPolygon({}, RegionPoint2{5,5}, true, true);
    Check(incomplete.status == EdgeEvidenceStatus::IncompleteGeometry, "empty truncated geometry");
    const auto degenerate = AssessEveryRecordedPolygon({{{{0,0}, {1,1}}, 0}}, RegionPoint2{0,0}, true);
    Check(degenerate.status == EdgeEvidenceStatus::IncompleteGeometry && !degenerate.polygons[0].measured, "two vertex list not measurable");
}

} // namespace

int main() {
    try {
        TestFalkreathTwoPolygonRPLICensus();
        TestInsideNonZeroRPLI();
        TestOverlapAndBoundary();
        TestUntrustedInputsNeverProduceGeometry();
        TestEmptyAndDegenerate();
        std::cout << "Region polygon census tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Region polygon census failure: " << e.what() << '\n';
        return 1;
    }
}
