#pragma once
#include "Aetherclime/ClimateRegionResolver.h"
#include "Aetherclime/Climate.h"
#include <optional>
#include <string_view>

namespace aetherclime {
enum class TravelEvent : unsigned char { NoExterior, InitialExterior, Normal, FarJump, WorldspaceChanged, CalendarRolledBack };
std::string_view TravelEventName(TravelEvent) noexcept;
// Tracks only accepted exterior world positions, no interior XY, no RE pointers,
// no per-frame hook, no cross-session persistence.
class TravelDiscontinuity {
public:
    static constexpr double kJumpDistanceRaw=32768.0; // 8 exterior cells; heuristic, not a speed detector
    TravelEvent Observe(const std::optional<ClimateWorldPoint>& position, const Date& date) noexcept;
    void Reset() noexcept {last_.reset(); lastDate_.reset();}
private:
    std::optional<ClimateWorldPoint> last_;
    std::optional<Date> lastDate_;
};
} // namespace aetherclime
