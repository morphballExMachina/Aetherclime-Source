#include "Aetherclime/Climate.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace aetherclime;

namespace {
bool Nearly(double a, double b, double tolerance = 0.001) { return std::abs(a - b) <= tolerance; }
void Check(bool condition, const char* description) {
    // CTest must keep checking conditions even for Release/NDEBUG builds.
    if (!condition) throw std::runtime_error(description);
}
void TestDefaultCalendar() {
    Climate climate;
    Check(climate.GetSeason({0, 15}).official == Season::Winter, "Morning Star must be winter");
    Check(climate.GetSeason({3, 15}).official == Season::Spring, "Rain's Hand must be spring");
    Check(climate.GetSeason({6, 15}).official == Season::Summer, "Sun's Height must be summer");
    Check(climate.GetSeason({9, 15}).official == Season::Autumn, "Frostfall must be autumn");
    Check(climate.GetSeason({11, 15}).official == Season::Winter, "Evening Star must be winter");
}
void TestYearBoundaryAndProgress() {
    Climate climate;
    const auto dec = climate.GetSeason({11, 15});
    const auto jan = climate.GetSeason({0, 15});
    const auto feb = climate.GetSeason({1, 15});
    Check(dec.progress < jan.progress, "winter progress must increase across December/January");
    Check(jan.progress < feb.progress, "winter progress must increase across January/February");
    Check(dec.progress > 0.0 && feb.progress < 1.0, "winter progress outside bounds");
    Check(Nearly(climate.GetSeason({11, 1, 0.0}).weights[0], 0.5), "December boundary blend mismatch");
}
void TestTransitionsAndCustomCalendar() {
    Config custom = DefaultConfig();
    custom.transitionDays = 10;
    custom.months[2] = Season::Winter; // Extended winter through First Seed.
    Climate climate(custom);
    Check(climate.GetSeason({2, 10}).official == Season::Winter, "custom winter calendar mismatch");
    const auto beforeApril = climate.GetSeason({2, 26});
    Check(beforeApril.weights[0] > 0.0 && beforeApril.weights[1] > 0.0, "pre-April blend mismatch");
    const auto april = climate.GetSeason({3, 1, 0.0});
    Check(april.official == Season::Spring, "April official season mismatch");
    Check(Nearly(april.weights[0], 0.5), "April winter blending mismatch");
    Check(Nearly(april.weights[1], 0.5), "April spring blending mismatch");
    custom.transitionDays = 0;
    Climate sharp(custom);
    Check(Nearly(sharp.GetSeason({3, 1}).weights[1], 1.0), "sharp transition mismatch");
}
void TestTemperature() {
    Climate climate;
    Sample midday{.date = {6, 15, 15.0, 201}, .region = "whiterun"};
    Sample midnight = midday;
    midnight.date.hour = 3.0;
    auto warm = climate.Calculate(midday);
    auto cold = climate.Calculate(midnight);
    Check(warm.airC > cold.airC + 9.0, "day/night temperature curve mismatch");
    Check(warm.region == "Whiterun Hold", "Whiterun region label mismatch");
    Sample mountain = midday;
    mountain.elevationAboveReferenceM = 1000;
    Check(Nearly(climate.Calculate(mountain).airC, warm.airC - 6.0), "altitude lapse rate mismatch");
    Sample windy = midday;
    windy.date.month = 0;
    windy.windKmh = 35.0;
    const auto chill = climate.Calculate(windy);
    Check(chill.airC < 10.0 && chill.feelsLikeC < chill.airC, "wind chill mismatch");
    Check(Nearly(Climate::ToFahrenheit(0.0), 32.0), "Fahrenheit conversion mismatch");
    Check(Nearly(climate.Calculate(midday).airC, climate.Calculate(midday).airC), "non-deterministic result");
}
void TestWaterAndPersistence() {
    Climate climate;
    Sample coastal{.date = {0, 20, 12.0, 201}, .region = "winterhold", .weather = Weather::Blizzard, .underwater = true};
    auto surface = climate.Calculate(coastal);
    coastal.waterDepthM = 50;
    auto deep = climate.Calculate(coastal);
    Check(deep.underwater, "underwater state missing");
    Check(deep.waterC <= surface.waterC, "water should cool with depth");
    Check(deep.waterC >= -1.8, "water temperature below configured minimum");
    Check(Nearly(deep.airC, climate.Calculate(coastal).airC), "underwater air estimate changed unexpectedly");
}
void TestInvalid() {
    bool thrown = false;
    try { Climate{}.GetSeason({12, 1}); } catch (const std::invalid_argument&) { thrown = true; }
    Check(thrown, "invalid calendar month must throw");
}
} // namespace

int main() {
    TestDefaultCalendar();
    TestYearBoundaryAndProgress();
    TestTransitionsAndCustomCalendar();
    TestTemperature();
    TestWaterAndPersistence();
    TestInvalid();
    std::cout << "Aetherclime core: all 6 test groups passed\n";
}
