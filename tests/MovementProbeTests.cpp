#include "Aetherclime/MovementProbe.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace aetherclime;

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
MovementPosition Exterior(double x, double y, double z = 0.0,
                          std::uint32_t cell = 0x1234, std::uint32_t ws = 0x3C) {
    return {.cellFormId = cell, .worldspaceFormId = ws,
            .exteriorWorldPosition = RawPosition{x, y, z}};
}
}

int main() {
    MovementProbe probe({.minimumDistanceRawUnits = 256.0, .maxSamples = 20, .maxReports = 8});
    auto decision = probe.Observe(Exterior(0, 0));
    Check(decision.report && decision.reason == MovementReason::Initial, "first position should report");
    Check(!probe.Observe(Exterior(100, 0)).report, "short movement should not report");
    Check(!probe.Observe(Exterior(200, 0)).report, "small changes must accumulate from last report");
    decision = probe.Observe(Exterior(256, 0));
    Check(decision.report && decision.reason == MovementReason::Distance, "256-unit movement missing");
    Check(std::abs(decision.distanceRawUnits - 256.0) < 0.001, "wrong distance");
    Check(!probe.Observe(Exterior(256, 0, 9000.0)).report,
        "vertical-only movement must not be treated as a region crossing");

    decision = probe.Observe(Exterior(256, 0, 0, 0x1235));
    Check(decision.report && decision.reason == MovementReason::CellChanged,
        "cell transition missing");
    decision = probe.Observe(Exterior(256, 0, 0, 0x1235, 0x4C));
    Check(decision.report && decision.reason == MovementReason::WorldspaceChanged,
        "worldspace transition missing");

    // Interior position must never be forwarded to a worldspace XY audit.
    MovementPosition interior{.cellFormId = 0x4455, .worldspaceFormId = 0,
        .exteriorWorldPosition = RawPosition{999999, 999999, 999999}};
    decision = probe.Observe(interior);
    Check(decision.report && decision.reason == MovementReason::WorldspaceChanged,
        "interior transition missing");
    Check(!probe.Observe(interior).report, "identical interior must stay silent");
    decision = probe.Observe(Exterior(0, 0));
    Check(decision.report && decision.reason == MovementReason::WorldspaceChanged,
        "return to exterior missing");
    Check(!probe.Exhausted(), "probe exhausted too early");

    decision = probe.Observe(Exterior(300, 300));
    Check(decision.report && decision.reason == MovementReason::Distance,
          "movement after exterior transition missing");

    // Non-finite coordinates must be discarded, rather than measured or emitted.
    decision = probe.Observe(Exterior(std::numeric_limits<double>::quiet_NaN(), 0));
    Check(decision.report && decision.reason == MovementReason::PositionAvailabilityChanged,
        "invalid positions must be reported as unavailable");
    Check(probe.Exhausted(), "report budget must stop observation");
    Check(probe.Reports() == 8, "report count must be capped exactly");
    Check(!probe.Observe(Exterior(1000, 0)).report, "exhausted probe emitted data");

    probe.Reset();
    Check(!probe.Exhausted() && probe.Samples() == 0 && probe.Reports() == 0,
        "session reset failed");
    Check(probe.Observe(Exterior(0, 0)).reason == MovementReason::Initial,
        "session reset should reestablish baseline");

    // Sample budget must stop silent/stationary sessions as well.
    MovementProbe stationary({.minimumDistanceRawUnits = 256, .maxSamples = 3, .maxReports = 80});
    Check(stationary.Observe(Exterior(0, 0)).report, "stationary baseline missing");
    Check(!stationary.Observe(Exterior(0, 0)).report, "stationary point logged twice");
    decision = stationary.Observe(Exterior(0, 0));
    Check(decision.exhausted && !decision.report, "sample cap should stop a silent session");
    Check(stationary.Samples() == 3 && stationary.Reports() == 1, "sample/report counts incorrect");

    // Cell same, world same: availability transition is worth reporting.
    MovementProbe missing;
    auto noWorldPosition = Exterior(1, 1);
    noWorldPosition.exteriorWorldPosition.reset();
    Check(missing.Observe(noWorldPosition).report, "missing baseline not reported");
    Check(missing.Observe(Exterior(1, 1)).reason == MovementReason::PositionAvailabilityChanged,
          "position availability transition missing");

    std::cout << "Aetherclime bounded movement probe: all tests passed\n";
}
