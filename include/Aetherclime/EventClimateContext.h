#pragma once

#include "Aetherclime/Climate.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {

// Alpha-23: main-game-thread EVENT gate. No polling, hooks, or weather writes.
// Pure logic; Skyrim event timing is intentionally verified at the adapter.
enum class ClimateEventGateStatus : std::uint8_t {
    Disabled, MissingCell, UntrustedRegion, Duplicate, Accept
};
std::string_view ClimateEventGateStatusName(ClimateEventGateStatus status) noexcept;

struct ClimateEventFrame {
    bool enabled{false};
    bool hasCell{false};
    bool interior{false};
    std::uint32_t cellFormId{0};
    std::string regionKey;
    bool regionTrusted{false};
    std::uint32_t skyWeatherFormId{0}; // informational; inside never cached as outdoor weather
    Date observedAt{};
};

// Collapses duplicate cell/location events in the same cell and weather.
// A change in cell, region, interior status, weather ID, date, or elapsed
// in-game time permits a new sample. Each session/reset starts fresh.
class ClimateEventGate {
public:
    static constexpr double kSameCellMinRefreshGameHours = 0.25;
    void Reset() noexcept;
    [[nodiscard]] ClimateEventGateStatus Evaluate(const ClimateEventFrame& frame);
private:
    std::optional<ClimateEventFrame> last_;
};

// Alpha-23 Rev2: pure, bounded policy for the SKSE main-thread task handoff.
// A native cell/location event may arrive before PlayerCharacter has adopted
// the new cell. The adapter schedules checks; this model never polls frames.
enum class DeferredClimateTarget : std::uint8_t { Cell, Location };
enum class DeferredClimateStatus : std::uint8_t { Ready, Retry, Exhausted, Inactive };
constexpr unsigned kMaxDeferredClimateChecks = 6;
struct DeferredClimateFrame {
    bool sessionActive{false};
    bool hasPlayer{false};
    std::uint32_t currentCellId{0};
    std::uint32_t currentLocationId{0};
    DeferredClimateTarget target{DeferredClimateTarget::Cell};
    std::uint32_t expectedId{0};
    unsigned checkNumber{1}; // 1-based; one queued task = one check
};
[[nodiscard]] DeferredClimateStatus EvaluateDeferredClimate(const DeferredClimateFrame& frame) noexcept;
[[nodiscard]] std::string_view DeferredClimateStatusName(DeferredClimateStatus status) noexcept;

// Alpha-24: extend the taxonomy without expanding the heating model.
enum class BuildingKind : std::uint8_t {
    Exterior, UnknownInterior, Inn, PalaceOrCourt, Home, Shop, Hall,
    CaveOrMine, RuinOrCrypt, TempleOrShrine, Funerary, Ambiguous
};
std::string_view BuildingKindName(BuildingKind kind) noexcept;
enum class BuildingEvidence : std::uint8_t { Exterior, None, ConfiguredInn, KnownName, NameHint, Conflict };
std::string_view BuildingEvidenceName(BuildingEvidence evidence) noexcept;
// Evidence confidence is descriptive only: never a permit to infer heat.
[[nodiscard]] std::string_view BuildingEvidenceConfidence(BuildingEvidence evidence) noexcept;

struct BuildingContext {
    BuildingKind kind{BuildingKind::UnknownInterior};
    BuildingEvidence evidence{BuildingEvidence::None};
    // NO heating inference. Only a separate explicit inn rule can activate
    // Alpha-21's ambient model. This is a classification hint for expansion.
    bool heatingAssumed{false};
    bool knownHeatedInnRule{false};
};

// Conservative name-based classification; no arbitrary interior heating.
// A renamed/localized/unknown building remains unclassified unless its
// explicit inn rule matched. Cell and location name disagreement is reported.
BuildingContext ClassifyBuilding(bool hasCell, bool interior,
    std::string_view cellDisplayName, std::string_view locationDisplayName,
    bool explicitConfiguredHeatedInn);

// Alpha-24: semantic changes to verified EXTERIOR observations only.
// Does not replace the event gate or the weather cache; crossing multiple
// exterior CELLs with unchanged weather/region is NOT a consumer update.
// No stationary weather timer, public notification, or engine mutation yet.
enum class OutdoorChangeKind : std::uint8_t {
    NotExteriorVerified, Baseline, Unchanged, WeatherChanged, RegionChanged, BothChanged
};
[[nodiscard]] std::string_view OutdoorChangeKindName(OutdoorChangeKind kind) noexcept;
[[nodiscard]] bool IsMeaningfulOutdoorChange(OutdoorChangeKind kind) noexcept;
struct OutdoorChangeSample {
    bool exterior{false};
    bool trustedRegion{false};
    std::string regionKey;
    std::uint32_t weatherFormId{0};
};
class OutdoorChangeGate {
public:
    void Reset() noexcept;
    [[nodiscard]] OutdoorChangeKind Observe(const OutdoorChangeSample& sample);
private:
    std::optional<OutdoorChangeSample> last_;
};

} // namespace aetherclime
