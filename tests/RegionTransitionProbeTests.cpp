#include "Aetherclime/RegionTransitionProbe.h"
#include <cassert>
#if defined(NDEBUG)
#error "Aetherclime test assertions must be enabled; check CMake target flags"
#endif
#include <cmath>
#include <iostream>
#include <limits>

using namespace aetherclime;
namespace {
constexpr auto In = RegionFootprintStatus::InsideRecordedPolygon;
constexpr auto Out = RegionFootprintStatus::OutsideRecordedPolygons;
constexpr auto Border = RegionFootprintStatus::OnRecordedBoundary;
constexpr auto Incomplete = RegionFootprintStatus::IncompleteGeometry;

BoundaryFrame Frame(double x, double y, std::uint32_t cell = 1,
                    std::uint32_t world = 60, RegionFootprintStatus region = In) {
    return {cell, world, RegionPoint2{x, y}, {{0x10FDC6, region}}, true};
}
} // namespace

int main() {
    // The actual Alpha-9 Winterhold walk straddled the known rectangular
    // polygon edge. This is not a weather-engine selection assertion.
    RegionTransitionProbe tracker;
    // Independent rectangle bounds previously read from the loaded REGN
    // record in SSEEdit. Use the real geometry helper to derive the two
    // statuses rather than simply assigning inside/outside in this case.
    const RegionPolygon winterholdRectangle{
        {101940.0156, 93910.7734}, {115059.2734, 93910.7734},
        {115059.2734, 111101.5391}, {101940.0156, 111101.5391}};
    auto winterholdStatus = [&](double x, double y) {
        return AssessRegionFootprint({winterholdRectangle}, RegionPoint2{x, y}, true).status;
    };
    const auto insideStatus = winterholdStatus(114987.8, 110817.8);
    const auto outsideStatus = winterholdStatus(114736.3, 111338.7);
    assert(insideStatus == In);
    assert(outsideStatus == Out);
    auto before = Frame(114987.8, 110817.8, 0x8E62, 60, insideStatus);
    auto after = Frame(114736.3, 111338.7, 0x8E62, 60, outsideStatus);
    assert(tracker.Observe(before).status == BoundaryFrameStatus::Baseline);
    const auto exit = tracker.Observe(after);
    assert(exit.status == BoundaryFrameStatus::Compared);
    assert(exit.transitions.size() == 1);
    assert(exit.transitions[0].regionFormId == 0x10FDC6);
    assert(exit.transitions[0].from == In && exit.transitions[0].to == Out);
    assert(exit.horizontalStepRawUnits > 570.0 && exit.horizontalStepRawUnits < 590.0);
    assert(tracker.Observe(after).transitions.empty());

    // Reverse direction is separately observed, never inferred from a prior exit.
    const auto entry = tracker.Observe(before);
    assert(entry.transitions.size() == 1);
    assert(entry.transitions[0].from == Out && entry.transitions[0].to == In);

    // Boundaries and incomplete geometry do not produce invented transitions.
    tracker.Reset();
    assert(tracker.Observe(Frame(0, 0, 1, 60, Border)).status == BoundaryFrameStatus::Baseline);
    assert(tracker.Observe(Frame(100, 0, 1, 60, Out)).transitions.empty());
    assert(tracker.Observe(Frame(200, 0, 1, 60, Incomplete)).transitions.empty());
    assert(tracker.Observe(Frame(300, 0, 1, 60, In)).transitions.empty());

    // A shared region can be compared across a cell boundary, but a newly
    // discovered region is never taken as proof that the player entered it.
    tracker.Reset();
    assert(tracker.Observe(Frame(0, 0, 10, 60, In)).status == BoundaryFrameStatus::Baseline);
    assert(tracker.Observe(Frame(100, 0, 11, 60, Out)).transitions.size() == 1);
    auto unknown = Frame(200, 0, 12, 60, Out);
    unknown.regions = {{0xABCDEF, In}};
    assert(tracker.Observe(unknown).status == BoundaryFrameStatus::NoSharedCandidates);

    // Fast travel / coc must reset the baseline instead of claiming crossings.
    tracker.Reset();
    (void)tracker.Observe(Frame(0, 0, 1, 60, In));
    assert(tracker.Observe(Frame(10000, 0, 2, 60, Out)).status == BoundaryFrameStatus::ResetLargeJump);
    assert(tracker.Observe(Frame(10100, 0, 2, 60, Out)).transitions.empty());

    // Interior or absent position clears the baseline; no interior coordinates.
    tracker.Reset();
    (void)tracker.Observe(Frame(0, 0, 1, 60, In));
    BoundaryFrame interior{0x43FAB, 0, std::nullopt, {}, true};
    assert(tracker.Observe(interior).status == BoundaryFrameStatus::ResetUnavailable);
    assert(tracker.Observe(Frame(100, 0, 1, 60, Out)).status == BoundaryFrameStatus::Baseline);

    // Switching worldspaces is a reset even if raw coordinates coincide.
    tracker.Reset();
    (void)tracker.Observe(Frame(0, 0, 1, 60, In));
    assert(tracker.Observe(Frame(100, 0, 1, 61, Out)).status == BoundaryFrameStatus::ResetWorldspace);

    // Truncated candidate set and non-finite coordinates cannot be trusted.
    tracker.Reset();
    (void)tracker.Observe(Frame(0, 0, 1, 60, In));
    auto truncated = Frame(10, 0, 1, 60, Out);
    truncated.candidateSnapshotComplete = false;
    assert(tracker.Observe(truncated).status == BoundaryFrameStatus::ResetUnavailable);
    auto nan = Frame(std::numeric_limits<double>::quiet_NaN(), 0);
    assert(tracker.Observe(nan).status == BoundaryFrameStatus::ResetUnavailable);

    // Never invent a crossing when the same status repeats or evidence is missing.
    tracker.Reset();
    auto a = Frame(0, 0);
    a.regions = {{0x10FDC6, In}, {0xC5853, In}};
    auto b = Frame(100, 0);
    b.regions = {{0x10FDC6, In}, {0xC5853, Out}};
    (void)tracker.Observe(a);
    const auto mixed = tracker.Observe(b);
    assert(mixed.sharedCandidates == 2);
    assert(mixed.transitions.size() == 1);
    assert(mixed.transitions[0].regionFormId == 0xC5853);

    std::cout << "Alpha-10 sampled polygon transition policy passed\n";
}
