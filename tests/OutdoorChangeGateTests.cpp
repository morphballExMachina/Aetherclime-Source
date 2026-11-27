#include "Aetherclime/EventClimateContext.h"
#include <cassert>
#include <iostream>
using namespace aetherclime;
int main() {
    OutdoorChangeGate gate;
    const auto winter = OutdoorChangeSample{true, true, "winterhold", 0x100};
    assert(gate.Observe({false, true, "winterhold", 0x100}) == OutdoorChangeKind::NotExteriorVerified);
    assert(gate.Observe({true, false, "winterhold", 0x100}) == OutdoorChangeKind::NotExteriorVerified);
    assert(gate.Observe({true, true, "", 0x100}) == OutdoorChangeKind::NotExteriorVerified);
    assert(gate.Observe({true, true, "winterhold", 0}) == OutdoorChangeKind::NotExteriorVerified);
    assert(gate.Observe(winter) == OutdoorChangeKind::Baseline);
    assert(gate.Observe(winter) == OutdoorChangeKind::Unchanged);
    // Crossing cells is irrelevant: a verified region/weather pair is the baseline.
    assert(gate.Observe({false, true, "falkreath", 0x400}) == OutdoorChangeKind::NotExteriorVerified);
    assert(gate.Observe(winter) == OutdoorChangeKind::Unchanged);
    assert(gate.Observe({true, true, "winterhold", 0x101}) == OutdoorChangeKind::WeatherChanged);
    assert(gate.Observe({true, true, "winterhold", 0x101}) == OutdoorChangeKind::Unchanged);
    assert(gate.Observe({true, true, "falkreath", 0x101}) == OutdoorChangeKind::RegionChanged);
    assert(gate.Observe({true, true, "whiterun", 0x202}) == OutdoorChangeKind::BothChanged);
    assert(gate.Observe({true, true, "whiterun", 0x202}) == OutdoorChangeKind::Unchanged);
    assert(IsMeaningfulOutdoorChange(OutdoorChangeKind::BothChanged));
    assert(IsMeaningfulOutdoorChange(OutdoorChangeKind::Baseline));
    assert(!IsMeaningfulOutdoorChange(OutdoorChangeKind::Unchanged));
    assert(!IsMeaningfulOutdoorChange(OutdoorChangeKind::NotExteriorVerified));
    assert(OutdoorChangeKindName(OutdoorChangeKind::WeatherChanged) == "weather-changed");
    gate.Reset();
    assert(gate.Observe(winter) == OutdoorChangeKind::Baseline);
    std::cout << "OutdoorChangeGate: all assertions passed\n";
}
