#include "Aetherclime/Monitor.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace aetherclime;

namespace {
void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
}

int main() {
    Monitor monitor;
    MonitorObservation whiterun{
        .region = "whiterun", .locationFormId = 0x123, .cellFormId = 0x111,
        .worldspaceFormId = 0x3C, .weatherFormId = 0xAAA,
        .cell = CellEnvironment::Exterior, .season = Season::Summer, .outdoorAirC = 19.0
    };
    auto decision = monitor.Observe(whiterun, 0.0);
    Check(decision.report && HasReason(decision.reasons, MonitorReason::Initial), "initial event not reported");
    Check(!monitor.Observe(whiterun, 5.0).report, "identical state should remain quiet");

    auto rainy = whiterun;
    rainy.weatherFormId = 0xBBB;
    Check(!monitor.Observe(rainy, 6.0).report, "debounce interval not honored");
    decision = monitor.Observe(rainy, 10.0);
    Check(decision.report && HasReason(decision.reasons, MonitorReason::Weather), "weather change missing");
    Check(!HasReason(decision.reasons, MonitorReason::Initial), "baseline repeated unexpectedly");

    auto inside = rainy;
    inside.cell = CellEnvironment::Interior;
    inside.cellFormId = 0x222;
    inside.worldspaceFormId = 0;
    decision = monitor.Observe(inside, 20.0);
    Check(decision.report && HasReason(decision.reasons, MonitorReason::CellType), "interior transition missing");
    Check(HasReason(decision.reasons, MonitorReason::Cell), "cell FormID change missing");
    Check(HasReason(decision.reasons, MonitorReason::Worldspace), "worldspace change missing");
    Check(!HasReason(decision.reasons, MonitorReason::Location), "named location should remain unchanged");

    auto differentExterior = inside;
    differentExterior.cell = CellEnvironment::Exterior;
    differentExterior.cellFormId = 0x333;
    differentExterior.worldspaceFormId = 0x9AB;
    decision = monitor.Observe(differentExterior, 30.0);
    Check(decision.report && HasReason(decision.reasons, MonitorReason::Worldspace),
        "new worldspace not observed");
    Check(HasReason(decision.reasons, MonitorReason::Cell), "new cell not observed");

    auto slightlyWarmer = differentExterior;
    slightlyWarmer.outdoorAirC += 0.9;
    Check(!monitor.Observe(slightlyWarmer, 31.0).report, "small temperature change should be ignored");
    slightlyWarmer.outdoorAirC += 0.2;
    decision = monitor.Observe(slightlyWarmer, 41.0);
    Check(decision.report && HasReason(decision.reasons, MonitorReason::Temperature), "temperature threshold missing");

    auto newRegion = slightlyWarmer;
    newRegion.region = "falkreath";
    newRegion.locationFormId = 0x234;
    newRegion.season = Season::Autumn;
    decision = monitor.Observe(newRegion, 52.0);
    Check(decision.report && HasReason(decision.reasons, MonitorReason::Region), "region transition missing");
    Check(HasReason(decision.reasons, MonitorReason::Location), "named location transition missing");
    Check(HasReason(decision.reasons, MonitorReason::Season), "season transition missing");
    Check(!monitor.Observe(newRegion, 53.0).report, "unchanged state logged too often");

    decision = monitor.Observe(newRegion, 235.0);
    Check(decision.report && HasReason(decision.reasons, MonitorReason::Heartbeat), "heartbeat missing");
    Check(ReasonsToString(decision.reasons) == "heartbeat", "heartbeat reason name mismatch");

    Check(!monitor.Observe(newRegion, std::numeric_limits<double>::quiet_NaN()).report,
        "invalid time should be rejected");
    monitor.Reset();
    Check(HasReason(monitor.Observe(newRegion, 0.0).reasons, MonitorReason::Initial),
        "session reset did not restore first observation");
    Check(!monitor.Observe(newRegion, -1.0).report, "negative clock should not emit a change");
    Check(!monitor.Observe(newRegion, 0.0).report, "unchanged state incorrectly marked as a change");

    std::cout << "Aetherclime monitoring: all tests passed\n";
}
