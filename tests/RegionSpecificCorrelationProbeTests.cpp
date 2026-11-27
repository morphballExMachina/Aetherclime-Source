#include "Aetherclime/RegionSpecificCorrelationProbe.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace aetherclime;
namespace {
constexpr auto Inside = RegionFootprintStatus::InsideRecordedPolygon;
constexpr auto Outside = RegionFootprintStatus::OutsideRecordedPolygons;
constexpr std::uint32_t Winterhold = 0x0010FDC6;
constexpr std::uint32_t Coast = 0x000C5852;
constexpr std::uint32_t Snow = 0x000C5853;

RegionSpecificCorrelationInput Sample(double seconds, std::uint32_t ptr = Winterhold,
                                     std::uint32_t cell = 100, std::uint32_t weather = 11) {
    RegionSpecificCorrelationInput in;
    in.sessionSeconds = seconds;
    in.cellFormId = cell;
    in.sky.exterior = true;
    in.sky.skyAvailable = true;
    in.sky.worldspaceFormId = 60;
    in.sky.currentWeatherFormId = weather;
    in.sky.skyRegionPointerFormId = ptr;
    return in;
}
void Near(double lhs, double rhs) { assert(std::abs(lhs-rhs) < 0.00001); }
}

int main() {
    RegionSpecificCorrelationProbe probe;
    auto out = probe.Observe(Sample(0));
    assert(out.report && out.status == RegionSpecificStatus::Baseline);
    out = probe.Observe(Sample(2));
    assert(!out.report);

    // College bridge: Coast entry, unrelated Snow exit, then pointer changes
    // to Coast. The answer must be Coast, NOT the most recent Snow crossing.
    std::array<BoundaryTransition, 1> coastEntry{{{Coast, Outside, Inside}}};
    auto s = Sample(10); s.sampledCrossings = coastEntry;
    out = probe.Observe(s);
    assert(!out.report && out.allCrossingCount == 1);
    std::array<BoundaryTransition, 1> snowExit{{{Snow, Inside, Outside}}};
    s = Sample(12); s.sampledCrossings = snowExit;
    out = probe.Observe(s);
    assert(!out.report);

    out = probe.Observe(Sample(14, Coast, 101));
    assert(out.report && out.status == RegionSpecificStatus::PointerChanged);
    assert(out.pointerChanged && out.cellChanged && !out.weatherPairChanged);
    assert(out.previousPointer == Winterhold && out.currentPointer == Coast);
    assert(!out.previousPointerCrossing.available);
    assert(out.currentPointerCrossing.available);
    assert(out.currentPointerCrossing.from == Outside && out.currentPointerCrossing.to == Inside);
    Near(out.currentPointerCrossing.secondsBeforeCurrentSample, 4.0);
    assert(out.currentPointerCrossing.nearInTime);
    assert(!out.sameSampleCrossingAndPointer);

    // Region exit occurs AFTER the Sky pointer switched away from it.
    std::array<BoundaryTransition, 1> winterholdExit{{{Winterhold, Inside, Outside}}};
    s = Sample(16, Coast, 101); s.sampledCrossings = winterholdExit;
    out = probe.Observe(s);
    assert(out.report && out.status == RegionSpecificStatus::RelatedCrossing);
    assert(out.relatedCrossingCount == 1);
    assert(out.relatedCrossings[0].regionFormId == Winterhold);
    assert(out.relatedCrossings[0].matchesLatestSwitchFrom);
    assert(!out.relatedCrossings[0].matchesCurrentPointer);
    Near(out.relatedCrossings[0].secondsAfterPointerSwitch, 2.0);

    // Return trip: Coast -> Winterhold, and then a separate Winterhold entry.
    out = probe.Observe(Sample(20, Winterhold, 102));
    assert(out.report && out.pointerChanged);
    assert(out.previousPointerCrossing.available); // Coast, entered at t10
    Near(out.previousPointerCrossing.secondsBeforeCurrentSample, 10.0);
    assert(out.currentPointerCrossing.available); // Winterhold, exited t16
    Near(out.currentPointerCrossing.secondsBeforeCurrentSample, 4.0);
    std::array<BoundaryTransition, 1> winterholdEntry{{{Winterhold, Outside, Inside}}};
    s = Sample(22, Winterhold, 102); s.sampledCrossings = winterholdEntry;
    out = probe.Observe(s);
    assert(out.report && out.relatedCrossings[0].matchesCurrentPointer);
    assert(out.relatedCrossings[0].matchesLatestSwitchTo);
    Near(out.relatedCrossings[0].secondsAfterPointerSwitch, 2.0);

    // Simultaneous pointer and crossing: observed together, ordering UNKNOWN.
    RegionSpecificCorrelationProbe simultaneous;
    out = simultaneous.Observe(Sample(0));
    assert(out.status == RegionSpecificStatus::Baseline);
    s = Sample(2, Coast, 101); s.sampledCrossings = coastEntry;
    out = simultaneous.Observe(s);
    assert(out.report && out.status == RegionSpecificStatus::PointerAndCrossing);
    assert(out.sameSampleCrossingAndPointer);
    assert(out.currentPointerCrossing.available);
    Near(out.currentPointerCrossing.secondsBeforeCurrentSample, 0.0);
    assert(out.relatedCrossingCount == 1 && out.relatedCrossings[0].matchesLatestSwitchTo);
    Near(out.relatedCrossings[0].secondsAfterPointerSwitch, 0.0);

    // Null Sky region pointer is valid, but never pairs with FormID 0.
    out = simultaneous.Observe(Sample(4, 0, 101));
    assert(out.report && out.pointerChanged && out.currentPointer == 0);
    assert(!out.currentPointerCrossing.available);

    // A weather transition without pointer changes is not a report here.
    out = simultaneous.Observe(Sample(6, 0, 101, 99));
    assert(!out.report && out.weatherPairChanged);

    // An invalid crossing does not contaminate region-specific history.
    RegionSpecificCorrelationProbe invalid;
    out = invalid.Observe(Sample(0));
    assert(out.status == RegionSpecificStatus::Baseline);
    std::array<BoundaryTransition, 1> notDefinite{{{Coast, Inside, RegionFootprintStatus::IncompleteGeometry}}};
    s = Sample(2); s.sampledCrossings = notDefinite;
    out = invalid.Observe(s);
    assert(!out.report && out.allCrossingCount == 0);
    out = invalid.Observe(Sample(4, Coast));
    assert(out.report && !out.currentPointerCrossing.available);

    // A historical crossing older than 10s is not called "near".
    out = probe.Observe(Sample(40, Coast, 103));
    assert(out.report && out.currentPointerCrossing.available);
    Near(out.currentPointerCrossing.secondsBeforeCurrentSample, 30.0);
    assert(!out.currentPointerCrossing.nearInTime);

    // Interior, worldspace, clock, geography discontinuities reset history.
    auto interior = Sample(42); interior.sky.exterior = false;
    out = probe.Observe(interior);
    assert(out.status == RegionSpecificStatus::ResetUnavailable);
    out = probe.Observe(interior); assert(!out.report);
    out = probe.Observe(Sample(44)); assert(out.status == RegionSpecificStatus::Baseline);
    out = probe.Observe(Sample(46, Coast));
    assert(out.report && !out.currentPointerCrossing.available);
    auto jump = Sample(48, Coast); jump.geographyDiscontinuity = true;
    out = probe.Observe(jump); assert(out.status == RegionSpecificStatus::ResetGeography);
    out = probe.Observe(Sample(50, Coast)); assert(!out.report);
    auto world = Sample(52, Coast); world.sky.worldspaceFormId = 61;
    out = probe.Observe(world); assert(out.status == RegionSpecificStatus::ResetWorldspace);
    out = probe.Observe(Sample(54, Coast)); assert(out.status == RegionSpecificStatus::ResetWorldspace);
    out = probe.Observe(Sample(56, Coast)); assert(!out.report);
    out = probe.Observe(Sample(55, Coast)); assert(out.status == RegionSpecificStatus::ResetClock);

    // Exhaustion is bounded and Reset() restores a clean next-game budget.
    RegionSpecificCorrelationProbe bounded(2);
    assert(bounded.Observe(Sample(0)).report);
    out = bounded.Observe(Sample(2, Coast));
    assert(out.report && out.exhausted && out.reports == 2);
    out = bounded.Observe(Sample(4, Winterhold));
    assert(!out.report && out.exhausted);
    bounded.Reset();
    out = bounded.Observe(Sample(0));
    assert(out.report && !out.exhausted && out.reports == 1);

    // The fixed capacity of 32 distinct regional IDs must be safe even with
    // many changing cells/modded candidate regions.
    RegionSpecificCorrelationProbe capacity;
    out = capacity.Observe(Sample(0));
    assert(out.status == RegionSpecificStatus::Baseline);
    for (std::uint32_t i = 1; i <= 36; ++i) {
        std::array<BoundaryTransition, 1> entry{{{1000 + i, Outside, Inside}}};
        auto d = Sample(static_cast<double>(i) * 2);
        d.sampledCrossings = entry;
        out = capacity.Observe(d);
        assert(!out.exhausted);
    }
    out = capacity.Observe(Sample(76, 1036));
    assert(out.report && out.currentPointerCrossing.available);
    Near(out.currentPointerCrossing.secondsBeforeCurrentSample, 4.0);
    std::cout << "RegionSpecificCorrelationProbe tests passed\n";
}
