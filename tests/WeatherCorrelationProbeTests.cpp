#include "Aetherclime/WeatherCorrelationProbe.h"

#include <cassert>
#include <limits>

using namespace aetherclime;

namespace {
WeatherSkyReading Sample(std::uint32_t current = 101, std::uint32_t last = 100) {
    WeatherSkyReading r;
    r.exterior = true;
    r.skyAvailable = true;
    r.worldspaceFormId = 0x3c;
    r.currentWeatherFormId = current;
    r.lastWeatherFormId = last;
    r.skyRegionPointerFormId = 0x10fdc6;
    r.currentWeatherPct = 0.2f;
    return r;
}
}

int main() {
    {
        WeatherCorrelationProbe p;
        auto r = p.Observe(Sample(), 0.0);
        assert(r.report && r.status == WeatherCorrelationStatus::Baseline);
        r = p.Observe(Sample(), 2.0);
        assert(!r.report && r.status == WeatherCorrelationStatus::None);
        auto v = Sample();
        v.currentWeatherPct = 0.8f;
        r = p.Observe(v, 4.0);
        assert(!r.report); // Normal blend advancement MUST NOT spam log.
        v.currentWeatherFormId = 102;
        r = p.Observe(v, 6.0);
        assert(r.report && r.weatherIdsChanged &&
               r.previousCurrentWeatherFormId == 101 &&
               r.status == WeatherCorrelationStatus::WeatherIdsChanged);
        v.skyRegionPointerFormId = 0x5853;
        r = p.Observe(v, 8.0);
        assert(r.report && r.skyRegionPointerChanged && !r.weatherIdsChanged &&
               r.status == WeatherCorrelationStatus::SkyRegionPointerChanged);
    }
    {
        WeatherCorrelationProbe p;
        (void)p.Observe(Sample(), 0.0);
        auto r = p.Observe(Sample(), 4.0, 1);
        assert(r.report && r.status == WeatherCorrelationStatus::BoundarySampled &&
               r.boundaryTransitions == 1 && r.secondsSinceLastCrossing == 0.0);
        r = p.Observe(Sample(), 12.0);
        assert(!r.report);
        r = p.Observe(Sample(), 14.0);
        assert(r.report && r.status == WeatherCorrelationStatus::FollowUp10s &&
               r.secondsSinceLastCrossing == 10.0);
        r = p.Observe(Sample(), 30.0);
        assert(!r.report);
        r = p.Observe(Sample(), 34.0);
        assert(r.report && r.status == WeatherCorrelationStatus::FollowUp30s);
        r = p.Observe(Sample(), 40.0);
        assert(!r.report);
        auto changed = Sample(103, 102);
        r = p.Observe(changed, 42.0, 2);
        assert(r.report && r.weatherIdsChanged && r.boundaryTransitions == 2 &&
               r.status == WeatherCorrelationStatus::BoundaryAndWeatherIdsChanged);
    }
    {
        WeatherCorrelationProbe p;
        (void)p.Observe(Sample(), 0.0);
        auto indoors = Sample();
        indoors.exterior = false;
        indoors.worldspaceFormId = 0;
        auto r = p.Observe(indoors, 2.0);
        assert(r.report && r.status == WeatherCorrelationStatus::ResetUnavailable);
        r = p.Observe(indoors, 4.0);
        assert(!r.report); // Never report indoor weather state.
        r = p.Observe(Sample(), 6.0);
        assert(r.report && r.status == WeatherCorrelationStatus::Baseline);
        auto otherWorld = Sample();
        otherWorld.worldspaceFormId = 0x123;
        r = p.Observe(otherWorld, 8.0);
        assert(r.report && r.status == WeatherCorrelationStatus::ResetWorldspace &&
               !r.weatherIdsChanged);
    }
    {
        WeatherCorrelationProbe p;
        auto noWeather = Sample();
        noWeather.currentWeatherFormId = 0;
        auto r = p.Observe(noWeather, 0.0);
        assert(!r.report);
        auto invalidBlend = Sample();
        invalidBlend.currentWeatherPct = std::numeric_limits<float>::quiet_NaN();
        r = p.Observe(invalidBlend, 2.0);
        assert(r.report && r.status == WeatherCorrelationStatus::Baseline && !r.blendValueValid);
        r = p.Observe(Sample(), std::numeric_limits<double>::quiet_NaN());
        assert(r.report && r.status == WeatherCorrelationStatus::ResetUnavailable);
    }
    {
        WeatherCorrelationProbe p;
        (void)p.Observe(Sample(), 0.0);
        (void)p.Observe(Sample(), 2.0, 1); // pending follow-ups
        auto moved = Sample(200, 199);
        auto r = p.Observe(moved, 6.0, 0, true); // teleport / incomplete geometry
        assert(r.report && r.status == WeatherCorrelationStatus::ResetGeography &&
               !r.weatherIdsChanged);
        r = p.Observe(moved, 40.0);
        assert(!r.report); // follow-up cancelled; no false weather correlation.
    }
    {
        WeatherCorrelationProbe p{2};
        auto r = p.Observe(Sample(), 0.0);
        assert(r.report && !r.exhausted);
        r = p.Observe(Sample(), 2.0, 1);
        assert(r.report && r.exhausted && p.Reports() == 2);
        r = p.Observe(Sample(999), 4.0, 1);
        assert(!r.report && r.exhausted);
        p.Reset();
        assert(p.Reports() == 0);
        r = p.Observe(Sample(), 0.0);
        assert(r.report && !r.exhausted);
    }
    return 0;
}
