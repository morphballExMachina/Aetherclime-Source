#include "Aetherclime/ExteriorWeatherContext.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace aetherclime;
namespace {
Date Time(double hour, int day = 17) { return Date{.month = 7, .day = day, .hour = hour, .year = 201}; }
ExteriorWeatherRequest Interior(std::string_view region, double hour, int day = 17) {
    return {true, true, true, region, Time(hour, day)};
}
} // namespace

int main() {
    ExteriorWeatherContext cache;
    assert(cache.Assess(Interior("winterhold", 8.0)).status == ExteriorWeatherStatus::NoExteriorObservation);
    assert(!cache.CaptureExterior(false, true, "winterhold", Weather::Snow, 0x123, 60, Time(8.0)));
    assert(!cache.CaptureExterior(true, false, "winterhold", Weather::Snow, 0x123, 60, Time(8.0)));
    assert(!cache.CaptureExterior(true, true, "winterhold", Weather::Snow, 0, 60, Time(8.0)));
    assert(!cache.CaptureExterior(true, true, "winterhold", Weather::Snow, 0x123, 0, Time(8.0)));
    assert(!cache.Last());

    assert(cache.CaptureExterior(true, true, "winterhold", Weather::Snow, 0x123, 60, Time(8.0)));
    auto inside = cache.Assess(Interior("winterhold", 8.5));
    assert(inside.usable);
    assert(inside.status == ExteriorWeatherStatus::InteriorObservationCurrent);
    assert(std::abs(inside.ageGameHours - 0.5) < 1e-8);
    assert(inside.observation->weather == Weather::Snow);
    assert(inside.observation->weatherFormId == 0x123);

    // An interior Sky reading is NEVER allowed to replace the exterior cache.
    assert(!cache.CaptureExterior(false, true, "winterhold", Weather::Clear, 0x456, 60, Time(8.6)));
    assert(cache.Assess(Interior("winterhold", 9.0)).observation->weather == Weather::Snow);

    // A trip elsewhere cannot silently reuse Winterhold's weather.
    auto wrongRegion = cache.Assess(Interior("falkreath", 9.0));
    assert(!wrongRegion.usable);
    assert(wrongRegion.status == ExteriorWeatherStatus::InteriorRegionMismatch);
    assert(!cache.Assess({true, true, false, "winterhold", Time(9.0)}).usable);
    assert(cache.Assess({true, true, false, "winterhold", Time(9.0)}).status == ExteriorWeatherStatus::RegionUntrusted);
    assert(cache.Assess({false, true, true, "winterhold", Time(9.0)}).status == ExteriorWeatherStatus::CellUnknown);

    // A 7-hour wait, crossing a date boundary, or time going backwards rejects old data.
    assert(cache.Assess(Interior("winterhold", 14.0)).usable); // exact age limit
    assert(cache.Assess(Interior("winterhold", 15.1)).status == ExteriorWeatherStatus::InteriorObservationExpired);
    assert(cache.Assess(Interior("winterhold", 8.1, 18)).status == ExteriorWeatherStatus::InteriorTimeUnknown);
    assert(cache.Assess(Interior("winterhold", 7.9)).status == ExteriorWeatherStatus::InteriorTimeUnknown);

    // A verified exterior sample at a different site updates the observation.
    assert(cache.CaptureExterior(true, true, "falkreath", Weather::Clear, 0x888, 60, Time(10.0)));
    assert(cache.Assess(Interior("falkreath", 10.1)).observation->weather == Weather::Clear);
    assert(cache.Assess(Interior("winterhold", 10.1)).status == ExteriorWeatherStatus::InteriorRegionMismatch);
    cache.Clear(); // load/new-game boundary
    assert(cache.Assess(Interior("falkreath", 10.2)).status == ExteriorWeatherStatus::NoExteriorObservation);
    std::cout << "ExteriorWeatherContext: all assertions passed\n";
}
