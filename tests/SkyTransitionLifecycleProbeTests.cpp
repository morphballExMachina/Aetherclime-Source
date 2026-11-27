#include "Aetherclime/SkyTransitionLifecycleProbe.h"

#include <cassert>
#include <limits>

using namespace aetherclime;

namespace {
WeatherSkyReading R(std::uint32_t current, std::uint32_t last, float pct,
                    std::uint32_t region = 0x10, std::uint32_t ws = 0x3C,
                    std::uint32_t over = 0) {
    WeatherSkyReading r;
    r.exterior = true;
    r.skyAvailable = true;
    r.worldspaceFormId = ws;
    r.currentWeatherFormId = current;
    r.lastWeatherFormId = last;
    r.currentWeatherPct = pct;
    r.skyRegionPointerFormId = region;
    r.overrideWeatherFormId = over;
    return r;
}
}

int main() {
    SkyTransitionLifecycleProbe p(20);
    auto d = p.Observe(R(10, 0, 1.f), 0);
    assert(d.report && d.status == SkyLifecycleStatus::Baseline);
    d = p.Observe(R(10, 0, 1.f), 2);
    assert(!d.report && d.status == SkyLifecycleStatus::None);

    // Transition candidate: pointer pair changes, blend starts at zero.
    d = p.Observe(R(20, 10, 0.f), 4);
    assert(d.report && d.status == SkyLifecycleStatus::WeatherPairChanged && d.pairChanged);
    assert(d.pairState == SkyWeatherPairState::CurrentAndLast && d.blendQuarter == 0);
    assert(d.previousCurrent == 10 && d.previousLast == 0);
    d = p.Observe(R(20, 10, .21f), 6);
    assert(!d.report);
    d = p.Observe(R(20, 10, .31f), 8);
    assert(d.report && d.status == SkyLifecycleStatus::BlendQuarterChanged);
    assert(d.previousBlendQuarter == 0 && d.blendQuarter == 1);
    assert(d.secondsSincePairChange == 4.0);
    d = p.Observe(R(20, 10, .61f), 10);
    assert(d.blendQuarterChanged && d.blendQuarter == 2);
    d = p.Observe(R(20, 10, .8f), 12);
    assert(d.blendQuarterChanged && d.blendQuarter == 3);
    d = p.Observe(R(20, 10, 1.f), 14);
    assert(d.blendQuarterChanged && d.blendQuarter == 4);
    d = p.Observe(R(20, 0, 1.f), 16);
    assert(d.status == SkyLifecycleStatus::LastPointerCleared && d.lastPointerCleared);
    assert(d.pairState == SkyWeatherPairState::CurrentOnly);
    assert(d.secondsSincePairChange == 12.0); // since pair recorded at t=4

    // Pointer-only change, no invented weather-pair transition.
    d = p.Observe(R(20, 0, 1.f, 0x22), 18);
    assert(d.status == SkyLifecycleStatus::SkyRegionPointerChanged && !d.pairChanged);
    d = p.Observe(R(20, 0, 1.f, 0x22, 0x3C, 99), 20);
    assert(d.status == SkyLifecycleStatus::OverridePointerChanged && d.overrideChanged);
    d = p.Observe(R(20, 0, 1.f, 0, 0x3C, 99), 22);
    assert(d.status == SkyLifecycleStatus::SkyRegionPointerChanged);
    // Multiple simultaneous changes recorded by flags without claiming order.
    d = p.Observe(R(30, 20, .05f, 0x44, 0x3C, 0), 24);
    assert(d.status == SkyLifecycleStatus::WeatherPairChanged);
    assert(d.skyRegionChanged && d.overrideChanged && d.pairChanged);
    assert(!d.blendQuarterChanged);

    // Invalid blend values must not generate milestones or unsafe logging.
    d = p.Observe(R(30, 20, std::numeric_limits<float>::quiet_NaN(), 0x44), 26);
    assert(!d.report && !d.blendValid && d.blendQuarter == -1);
    d = p.Observe(R(30, 20, .9f, 0x44), 28);
    assert(!d.report && !d.blendQuarterChanged);
    // Reversed progress is an observation, not a guaranteed monotone rule.
    d = p.Observe(R(30, 20, .2f, 0x44), 30);
    assert(d.report && d.blendQuarterChanged && d.blendQuarter == 0);

    // Geometry discontinuity and interiors reset interpretation windows.
    d = p.Observe(R(30, 20, .2f), 32, true);
    assert(d.status == SkyLifecycleStatus::ResetGeography);
    d = p.Observe(R(31, 30, .2f), 34);
    assert(d.pairChanged && d.secondsSincePairChange == 0);
    auto interior = R(31, 30, .2f);
    interior.exterior = false;
    interior.worldspaceFormId = 0;
    d = p.Observe(interior, 36);
    assert(d.status == SkyLifecycleStatus::ResetUnavailable);
    d = p.Observe(interior, 38);
    assert(!d.report);
    d = p.Observe(R(40, 0, 1.f), 40);
    assert(d.status == SkyLifecycleStatus::Baseline);
    d = p.Observe(R(40, 0, 1.f, 0x10, 0x99), 42);
    assert(d.status == SkyLifecycleStatus::ResetWorldspace);

    // Bounded reporting; reset can re-enable after next load.
    SkyTransitionLifecycleProbe capped(2);
    d = capped.Observe(R(1, 0, 1.f), 0);
    assert(d.report && !d.exhausted);
    d = capped.Observe(R(2, 1, 0.f), 2);
    assert(d.report && d.exhausted && capped.Reports() == 2);
    d = capped.Observe(R(3, 2, 0.f), 4);
    assert(!d.report && d.exhausted);
    capped.Reset();
    d = capped.Observe(R(3, 2, 0.f), 0);
    assert(d.report && !d.exhausted);
}
