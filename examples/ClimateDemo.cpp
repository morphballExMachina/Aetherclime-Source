#include "Aetherclime/Climate.h"

#include <iomanip>
#include <iostream>

int main() {
    using namespace aetherclime;
    Climate climate;
    std::cout << "Aetherclime 0.1 - Climate Core demonstration\n";
    for (int month : {0, 2, 4, 6, 8, 10, 11}) {
        std::cout << "\n" << kMonths[month] << " (" << SeasonName(climate.GetSeason({month, 15}).official) << ")\n";
        for (const auto& region : {"whiterun", "falkreath", "winterhold", "eastmarch"}) {
            const Sample sample{.date = {month, 15, 14.0, 201}, .region = region, .weather = Weather::Cloudy, .windKmh = 12};
            const auto reading = climate.Calculate(sample);
            std::cout << "  " << std::setw(11) << std::left << reading.region.substr(0, 11)
                      << "  " << std::fixed << std::setprecision(1) << std::setw(6)
                      << reading.airC << " C  (" << Climate::ToFahrenheit(reading.airC) << " F)\n";
        }
    }
}
