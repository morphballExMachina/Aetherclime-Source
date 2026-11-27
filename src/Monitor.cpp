#include "Aetherclime/Monitor.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace aetherclime {

Monitor::Monitor(MonitorOptions options) : options_(options) {
    options_.minimumReportIntervalSeconds = std::clamp(options_.minimumReportIntervalSeconds, 1.0, 300.0);
    options_.temperatureReportDeltaC = std::clamp(options_.temperatureReportDeltaC, 0.1, 25.0);
    options_.heartbeatSeconds = std::max(options_.heartbeatSeconds, options_.minimumReportIntervalSeconds);
}

void Monitor::Reset() noexcept {
    lastReported_.reset();
    lastReportSeconds_ = 0.0;
}

MonitorDecision Monitor::Observe(const MonitorObservation& current, double nowSeconds) {
    if (!std::isfinite(nowSeconds) || nowSeconds < 0.0 || !std::isfinite(current.outdoorAirC)) {
        return {};
    }
    // A game-session restart or test clock reset should establish a fresh baseline.
    if (lastReported_ && nowSeconds < lastReportSeconds_) Reset();
    if (!lastReported_) {
        lastReported_ = current;
        lastReportSeconds_ = nowSeconds;
        return {true, MonitorReason::Initial};
    }

    const auto& previous = *lastReported_;
    MonitorReason changes = MonitorReason::None;
    if (current.region != previous.region) changes = changes | MonitorReason::Region;
    if (current.locationFormId != previous.locationFormId) changes = changes | MonitorReason::Location;
    if (current.cellFormId != previous.cellFormId) changes = changes | MonitorReason::Cell;
    if (current.worldspaceFormId != previous.worldspaceFormId) changes = changes | MonitorReason::Worldspace;
    if (current.weatherFormId != previous.weatherFormId) changes = changes | MonitorReason::Weather;
    if (current.cell != previous.cell) changes = changes | MonitorReason::CellType;
    if (current.season != previous.season) changes = changes | MonitorReason::Season;
    if (std::abs(current.outdoorAirC - previous.outdoorAirC) >= options_.temperatureReportDeltaC) {
        changes = changes | MonitorReason::Temperature;
    }

    const double elapsed = nowSeconds - lastReportSeconds_;
    if (elapsed >= options_.heartbeatSeconds) changes = changes | MonitorReason::Heartbeat;
    if (changes == MonitorReason::None || elapsed < options_.minimumReportIntervalSeconds) return {};

    lastReported_ = current;
    lastReportSeconds_ = nowSeconds;
    return {true, changes};
}

std::string ReasonsToString(MonitorReason reasons) {
    std::string answer;
    for (const auto& [flag, label] : {
        std::pair{MonitorReason::Initial, std::string_view("initial")},
        std::pair{MonitorReason::Region, std::string_view("region")},
        std::pair{MonitorReason::Location, std::string_view("location")},
        std::pair{MonitorReason::Cell, std::string_view("cell")},
        std::pair{MonitorReason::Worldspace, std::string_view("worldspace")},
        std::pair{MonitorReason::Weather, std::string_view("weather")},
        std::pair{MonitorReason::CellType, std::string_view("cell-type")},
        std::pair{MonitorReason::Season, std::string_view("season")},
        std::pair{MonitorReason::Temperature, std::string_view("temperature")},
        std::pair{MonitorReason::Heartbeat, std::string_view("heartbeat")}
    }) {
        if (HasReason(reasons, flag)) {
            if (!answer.empty()) answer += ',';
            answer += label;
        }
    }
    return answer.empty() ? "none" : answer;
}

} // namespace aetherclime
