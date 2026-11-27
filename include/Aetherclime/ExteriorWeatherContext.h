#pragma once

#include "Aetherclime/Climate.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {

// An event-sampled, per-session last *verified exterior* weather observation.
// Never represent a Sky reading made in an interior as an exterior measurement.
// This does not query engine objects or hook game updates.
struct ExteriorWeatherObservation {
    std::string regionKey;
    Weather weather{Weather::Unknown};
    std::uint32_t weatherFormId{0};
    std::uint32_t worldspaceFormId{0};
    Date observedAt{};
};

enum class ExteriorWeatherStatus : std::uint8_t {
    NoExteriorObservation,
    ExteriorObserved,
    ExteriorWeatherUnavailable,
    RegionUntrusted,
    InteriorRegionMismatch,
    InteriorTimeUnknown,
    InteriorObservationExpired,
    InteriorObservationCurrent,
    CellUnknown
};

[[nodiscard]] std::string_view ExteriorWeatherStatusName(ExteriorWeatherStatus status) noexcept;

struct ExteriorWeatherRequest {
    bool hasCell{false};
    bool interior{false};
    bool regionTrusted{false};
    std::string_view regionKey{};
    Date now{};
};

struct ExteriorWeatherAssessment {
    ExteriorWeatherStatus status{ExteriorWeatherStatus::NoExteriorObservation};
    bool usable{false};
    double ageGameHours{-1.0};
    std::optional<ExteriorWeatherObservation> observation{};
};

class ExteriorWeatherContext {
public:
    static constexpr double kMaxAgeGameHours = 6.0;

    void Clear() noexcept;
    // Caller must verify that a CELL is exterior, climate region evidence is
    // trusted, and currentSkyWeather is genuinely available.
    [[nodiscard]] bool CaptureExterior(bool exteriorCell, bool regionTrusted,
                                      std::string_view regionKey, Weather weather,
                                      std::uint32_t weatherFormId,
                                      std::uint32_t worldspaceFormId, Date at);
    [[nodiscard]] ExteriorWeatherAssessment Assess(const ExteriorWeatherRequest& request) const;
    [[nodiscard]] const std::optional<ExteriorWeatherObservation>& Last() const noexcept { return last_; }

private:
    std::optional<ExteriorWeatherObservation> last_;
};

} // namespace aetherclime
