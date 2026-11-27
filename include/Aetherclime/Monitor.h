#pragma once

#include "Aetherclime/Climate.h"
#include "Aetherclime/Environment.h"

#include <cstdint>
#include <optional>
#include <string>

namespace aetherclime {

// Owned, engine-independent values: never retain pointers to Skyrim forms.
struct MonitorObservation {
    std::string region;
    std::uint32_t locationFormId = 0;
    std::uint32_t cellFormId = 0;
    std::uint32_t worldspaceFormId = 0;
    std::uint32_t weatherFormId = 0;
    CellEnvironment cell = CellEnvironment::Unknown;
    Season season = Season::Winter;
    double outdoorAirC = 0.0;
};

struct MonitorOptions {
    // Debounce multiple updates that happen close together (real seconds).
    double minimumReportIntervalSeconds = 10.0;
    double temperatureReportDeltaC = 1.0;
    double heartbeatSeconds = 180.0;
};

enum class MonitorReason : std::uint32_t {
    None = 0,
    Initial = 1U << 0,
    Region = 1U << 1,
    Location = 1U << 2,
    Cell = 1U << 3,
    Worldspace = 1U << 4,
    Weather = 1U << 5,
    CellType = 1U << 6,
    Season = 1U << 7,
    Temperature = 1U << 8,
    Heartbeat = 1U << 9
};

[[nodiscard]] constexpr MonitorReason operator|(MonitorReason a, MonitorReason b) noexcept {
    return static_cast<MonitorReason>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr bool HasReason(MonitorReason value, MonitorReason reason) noexcept {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(reason)) != 0;
}

[[nodiscard]] std::string ReasonsToString(MonitorReason reason);

struct MonitorDecision {
    bool report = false;
    MonitorReason reasons = MonitorReason::None;
};

class Monitor {
public:
    explicit Monitor(MonitorOptions options = {});
    void Reset() noexcept;
    [[nodiscard]] MonitorDecision Observe(const MonitorObservation& current, double nowSeconds);

private:
    MonitorOptions options_;
    std::optional<MonitorObservation> lastReported_;
    double lastReportSeconds_ = 0.0;
};

} // namespace aetherclime
