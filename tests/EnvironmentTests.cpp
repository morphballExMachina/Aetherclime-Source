#include "Aetherclime/Environment.h"

#include <iostream>
#include <stdexcept>

using namespace aetherclime;

namespace {
void Check(bool condition, const char* message) {
    // Unlike assert(), this stays active in Release builds with NDEBUG.
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    const auto interior = ClassifyEnvironment(true);
    Check(interior.cell == CellEnvironment::Interior, "interior CELL was misclassified");
    Check(interior.outdoorWeatherExposure == OutdoorWeatherExposure::NotApplicable,
        "interior was treated as exposed to outdoor weather");

    const auto exterior = ClassifyEnvironment(false);
    Check(exterior.cell == CellEnvironment::Exterior, "exterior CELL was misclassified");
    Check(exterior.outdoorWeatherExposure == OutdoorWeatherExposure::Possible,
        "exterior should permit, but not guarantee, weather exposure");

    const auto missing = ClassifyEnvironment(std::nullopt);
    Check(missing.cell == CellEnvironment::Unknown, "missing CELL must remain unknown");
    Check(missing.outdoorWeatherExposure == OutdoorWeatherExposure::Unknown,
        "weather exposure cannot be inferred without a CELL");

    Check(ToString(interior.cell) == "interior", "wrong interior label");
    Check(ToString(exterior.cell) == "exterior", "wrong exterior label");
    Check(ToString(missing.outdoorWeatherExposure) == "unknown", "wrong unknown label");
    Check(ToString(interior.outdoorWeatherExposure) == "not-applicable", "wrong indoor exposure label");
    Check(ToString(exterior.outdoorWeatherExposure) == "possible", "wrong outdoor exposure label");

    std::cout << "Aetherclime environment classification: all tests passed\n";
}
