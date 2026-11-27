#include "Aetherclime/RegionPointerTimingProbe.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace aetherclime;

namespace {
RegionPointerTimingInput Sample(double t, std::uint32_t cell = 10,
                                std::uint32_t ptr = 65, std::uint32_t weather = 101) {
    RegionPointerTimingInput in;
    in.sessionSeconds = t;
    in.cellFormId = cell;
    in.sky.exterior = true;
    in.sky.skyAvailable = true;
    in.sky.worldspaceFormId = 60;
    in.sky.currentWeatherFormId = weather;
    in.sky.skyRegionPointerFormId = ptr;
    return in;
}
bool Close(double a, double b) { return std::abs(a-b) < 0.00001; }
}
int main() {
    RegionPointerTimingProbe p;
    auto out = p.Observe(Sample(0));
    assert(out.report && out.status == RegionPointerTimingStatus::Baseline);
    out = p.Observe(Sample(2));
    assert(!out.report && !out.exhausted);
    auto crossing = Sample(10, 11);
    crossing.sampledPolygonTransitions = 1;
    out = p.Observe(crossing);
    assert(out.report && out.cellChanged && out.sampledPolygonTransitions == 1);
    assert(out.simultaneousSample && out.pointerSecondsAfterBoundary < 0);
    out = p.Observe(Sample(14, 11, 50));
    assert(out.report && out.skyPointerChanged && !out.weatherPairChanged);
    assert(Close(out.pointerSecondsAfterBoundary, 4.0));
    assert(Close(out.pointerSecondsAfterCell, 4.0));
    assert(Close(out.sampleGapSeconds, 4.0));
    out = p.Observe(Sample(20, 11, 50, 200));
    assert(out.report && out.weatherPairChanged && !out.skyPointerChanged);
    assert(Close(out.weatherSecondsAfterPointer, 6.0));
    assert(Close(out.weatherSecondsAfterBoundary, 10.0));

    // A simultaneous boundary, cell, pointer and weather change is recorded
    // as same-sample *without inventing their within-sample order*.
    auto sameSample = Sample(22, 12, 65, 300);
    sameSample.sampledPolygonTransitions = 2;
    out = p.Observe(sameSample);
    assert(out.report && out.simultaneousSample && out.sampledPolygonTransitions == 2);
    assert(Close(out.pointerSecondsAfterBoundary, 0));
    assert(Close(out.weatherSecondsAfterPointer, 0));
    assert(Close(out.weatherSecondsAfterBoundary, 0));
    assert(Close(out.boundarySecondsAfterPointer, 0));

    // Null Sky region pointer can be a legitimate transient observation.
    out = p.Observe(Sample(24, 12, 0, 300));
    assert(out.report && out.skyPointerChanged);
    // Interior discards all markers; first exterior return is baseline.
    auto interior = Sample(26); interior.sky.exterior = false;
    out = p.Observe(interior);
    assert(out.report && out.status == RegionPointerTimingStatus::ResetUnavailable);
    out = p.Observe(interior);
    assert(!out.report);
    out = p.Observe(Sample(28, 12, 50, 300));
    assert(out.status == RegionPointerTimingStatus::Baseline);
    out = p.Observe(Sample(30, 12, 65));
    assert(out.pointerSecondsAfterBoundary < 0);
    assert(out.weatherSecondsAfterBoundary < 0);

    // An invalid/discontinuous geographic comparison cannot carry history.
    auto jump = Sample(32, 13); jump.geographyDiscontinuity = true;
    out = p.Observe(jump);
    assert(out.report && out.status == RegionPointerTimingStatus::ResetGeography);
    out = p.Observe(Sample(34, 13));
    assert(!out.report); // establishes clean baseline during reset

    // Worldspace and clock regressions each reset instead of fabricating lags.
    auto differentWorld = Sample(36, 13); differentWorld.sky.worldspaceFormId = 61;
    out = p.Observe(differentWorld);
    assert(out.status == RegionPointerTimingStatus::ResetWorldspace);
    out = p.Observe(Sample(37, 13));
    assert(out.status == RegionPointerTimingStatus::ResetWorldspace);
    out = p.Observe(Sample(38, 13));
    assert(!out.report);
    out = p.Observe(Sample(37, 13));
    assert(out.status == RegionPointerTimingStatus::ResetClock);

    // Budgets close after exactly the specified number of reports, reset
    // fully on next load, and do not generate more lines while exhausted.
    RegionPointerTimingProbe bounded(2);
    assert(bounded.Observe(Sample(0)).report);
    out = bounded.Observe(Sample(2, 11));
    assert(out.report && out.exhausted && out.totalReports == 2);
    out = bounded.Observe(Sample(4, 11, 50));
    assert(!out.report && out.exhausted);
    bounded.Reset();
    out = bounded.Observe(Sample(0));
    assert(out.report && !out.exhausted && out.totalReports == 1);

    // Observed weather last-pointer changes are pair changes; unchanged
    // samples do not create periodic spam.
    RegionPointerTimingProbe pairs;
    auto x = Sample(0); x.sky.lastWeatherFormId = 75;
    pairs.Observe(x);
    x.sessionSeconds = 2; x.sky.lastWeatherFormId = 0;
    out = pairs.Observe(x);
    assert(out.report && out.weatherPairChanged);
    x.sessionSeconds = 4;
    out = pairs.Observe(x);
    assert(!out.report);
    std::cout << "RegionPointerTimingProbe tests passed\n";
}
