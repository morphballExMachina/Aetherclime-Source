#pragma once
#include "Aetherclime/EnvironmentalSnapshot.h"
#include "Aetherclime/HudSettings.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {
enum class HudReason { Initial, Season, Region, Context, Weather, Temperature, Location, Evidence, Heartbeat, DebugInitialization };
std::string_view HudReasonName(HudReason) noexcept;
// Static trigger strings only; unknown/irrelevant menus are ignored.
std::string_view HudMenuCloseTrigger(std::string_view menuName) noexcept;
struct HudNotice { HudReason reason; std::string text; };
struct HudStatistics {
    std::uint64_t eligibleSamples{}, sent{}, unavailable{}, unchanged{}, baselineOnly{};
    std::uint64_t blockedUi{}, blockedGrace{}, blockedCooldown{}, blockedSampleInterval{}, invalidClock{};
};
// One pure, value-only presenter per loaded session. BeginSample is cheap and
// reserves a sample; Observe consumes it once, only after a fresh owned snapshot.
// Suppressed events never queue text or move the last-presented baseline.
class HudNotifications {
public:
    void Reset() noexcept;
    bool BeginSample(const HudSettings&, double secondsSinceLoad, bool gameplayReady) noexcept;
    std::optional<HudNotice> Observe(const HudSettings&, const EnvironmentSnapshot&);
    [[nodiscard]] const HudStatistics& Statistics() const noexcept { return stats_; }
private:
    struct State {
        bool interior{}, trusted{}, provisional{}, modeledIndoor{};
        std::string region, name, evidence;
        std::optional<Season> season;
        std::optional<Weather> weather;
        std::optional<double> temperatureC;
        std::string temperatureBasis;
    };
    static State Classify(const HudSettings&, const EnvironmentSnapshot&);
    static std::string Format(const HudSettings&, const EnvironmentSnapshot&, const State&);
    std::optional<State> previous_;
    std::optional<double> lastSample_, lastSent_, lastReference_, reservation_;
    std::string lastText_;
    bool jokeSent_{};
    HudStatistics stats_{};
};
} // namespace aetherclime
