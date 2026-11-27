#include "Aetherclime/WorldPosition.h"

#include <iostream>
#include <limits>
#include <stdexcept>

using namespace aetherclime;

namespace {
void Check(bool truth, const char* message) {
    if (!truth) throw std::runtime_error(message);
}
}

int main() {
    const RawPosition exterior{1600.0, -800.0, 32000.0};
    const auto valid = CheckWorldPosition(true, false, true, exterior);
    Check(valid.status == PositionStatus::Available && valid.worldUnits.has_value(),
        "worldspace exterior coordinates should be available");
    Check(valid.worldUnits->z == 32000.0, "world Z must remain raw and unconverted");
    Check(PositionStatusName(valid.status) == "exterior-world-coordinates", "status label mismatch");

    const auto interior = CheckWorldPosition(true, true, false, exterior);
    Check(interior.status == PositionStatus::InteriorLocalOnly && !interior.worldUnits,
        "interior local Z must never appear as world altitude");

    const auto noCell = CheckWorldPosition(false, false, true, exterior);
    Check(noCell.status == PositionStatus::UnknownCell && !noCell.worldUnits,
        "missing cell should block coordinates");

    const auto noWorldspace = CheckWorldPosition(true, false, false, exterior);
    Check(noWorldspace.status == PositionStatus::MissingWorldspace && !noWorldspace.worldUnits,
        "worldspace-free exteriors must not be treated as calibrated coordinates");

    const RawPosition bad{0.0, 10.0, std::numeric_limits<double>::infinity()};
    const auto invalid = CheckWorldPosition(true, false, true, bad);
    Check(invalid.status == PositionStatus::NonFinite && !invalid.worldUnits,
        "non-finite coordinate should be rejected");

    std::cout << "Aetherclime world position: all tests passed\n";
}
