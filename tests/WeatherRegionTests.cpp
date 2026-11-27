#include "Aetherclime/WeatherRegions.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using namespace aetherclime;
int main() {
    const RegionWeatherCandidate empty{};
    auto r = SummarizeRegionWeather(empty, 123);
    assert(r.choices == 0 && r.rawWeightTotal == 0);
    assert(!r.containsObservedWeather && r.choicesWithGlobal == 0);

    RegionWeatherCandidate nat{};
    nat.regionFormId = 0x0010FDC6;
    nat.priority = 65;
    nat.overrideFlag = true;
    nat.choices = {
        {0x0004D7FB, 15, 0},
        {0x000C8221, 5, 0},
        {0x2800ABCD, 20, 0},
        {0x2800EFAB, 30, 0x0100ABCD}
    };
    r = SummarizeRegionWeather(nat, 0x0004D7FB);
    assert(r.choices == 4 && r.rawWeightTotal == 70);
    assert(r.containsObservedWeather && r.choicesWithGlobal == 1);
    assert(!SummarizeRegionWeather(nat, 0).containsObservedWeather);
    assert(!SummarizeRegionWeather(nat, 0x00000001).containsObservedWeather);

    nat.choices = {{1, std::numeric_limits<std::uint32_t>::max(), 0},
                   {2, std::numeric_limits<std::uint32_t>::max(), 0}};
    r = SummarizeRegionWeather(nat, 2);
    assert(r.rawWeightTotal == static_cast<std::uint64_t>(
        std::numeric_limits<std::uint32_t>::max()) * 2);

    std::vector<RegionWeatherCandidate> regions = {
        {.regionFormId = 9, .priority = 60},
        {.regionFormId = 5, .priority = 65},
        {.regionFormId = 2, .priority = 65},
        {.regionFormId = 6, .priority = 50}
    };
    SortWeatherCandidatesForDisplay(regions);
    assert(regions[0].regionFormId == 2 && regions[1].regionFormId == 5);
    assert(regions[2].priority == 60 && regions[3].priority == 50);
    // Ordering does not claim to be the actual region selected by Skyrim.
    std::cout << "Weather region portable tests passed.\n";
}
