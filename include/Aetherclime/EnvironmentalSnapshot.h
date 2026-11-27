#pragma once

#include "Aetherclime/Climate.h"
#include "Aetherclime/ClimateEstimates.h"
#include "Aetherclime/IndoorClimateModel.h"
#include "Aetherclime/BiomeIdentity.h"
#include "Aetherclime/EventClimateContext.h"
#include "Aetherclime/LocationIdentity.h"
#include "Aetherclime/PluginApi.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {

// Describes the *source* of outdoor weather, independently of the source
// of the displayed local name. Unverified interior Sky is never promoted to
// a verified outdoor observation.
enum class OutdoorWeatherProvenance : std::uint8_t {
    Unavailable, DirectExteriorSky, CachedVerifiedExterior, InteriorSkyUnverified
};
[[nodiscard]] std::string_view OutdoorWeatherProvenanceName(OutdoorWeatherProvenance) noexcept;

// Alpha-25: typed, owned snapshot. No pointers/string_views into engine forms,
// no callbacks, no game-thread querying. The adapter collects a one-shot input.
struct EnvironmentSnapshotInput {
    bool hasCell{false};
    bool interior{false};
    bool trustedRegion{false};
    bool provisionalRegion{false}; // recent nearby reference; NOT trusted geography
    std::string regionKey;
    std::string regionName;
    std::string regionSource;
    LocalIdentity local;
    BuildingContext building;
    Date date{};
    Weather weather{Weather::Unknown};
    OutdoorWeatherProvenance weatherProvenance{OutdoorWeatherProvenance::Unavailable};
    std::uint32_t weatherFormId{0};
    double outdoorAirC{0.0};
    double outdoorFeelsLikeC{0.0};
    // The named explicit-inn indoor model is optional and separate.
    std::optional<double> modeledIndoorAmbientC{};
    // Alpha-49 second, non-authoritative tier. Never replace legacy V1 fields.
    std::optional<double> estimatedOutdoorAirC{};
    OutdoorEstimateBasis outdoorEstimateBasis{OutdoorEstimateBasis::Unavailable};
    // Alpha-49.1 log-only provenance, not exposed as a geographic claim.
    std::string outdoorReferenceLabel;
    double outdoorReferenceDistanceRaw{-1.0};
    double outdoorReferenceInfluence{0.0};
    std::uint32_t outdoorReferenceCount{0};
    std::optional<double> estimatedIndoorAmbientC{};
    IndoorEstimateBasis indoorEstimateBasis{IndoorEstimateBasis::Unavailable};
};

struct EnvironmentSnapshot {
    bool available{false};
    bool interior{false};
    bool trustedRegion{false};
    bool provisionalRegion{false}; // recent nearby reference; NOT trusted geography
    std::string regionKey;
    std::string regionName;
    std::string regionSource;
    LocalIdentity local;
    BuildingContext building;
    Date date{};
    Weather weather{Weather::Unknown};
    OutdoorWeatherProvenance weatherProvenance{OutdoorWeatherProvenance::Unavailable};
    std::uint32_t verifiedOutdoorWeatherFormId{0};
    double outdoorAirC{0.0};
    double outdoorFeelsLikeC{0.0};
    // Exterior: same modeled outdoor air, NOT yet measured shelter/exposure.
    // Interior: available ONLY from the explicit configured inn model.
    std::optional<double> localAmbientC{};
    bool localAmbientIsModeledIndoor{false};
    // Alpha-49 estimates. No claim of verified weather, regional identity or fire.
    std::optional<double> estimatedOutdoorAirC{};
    OutdoorEstimateBasis outdoorEstimateBasis{OutdoorEstimateBasis::Unavailable};
    // Alpha-49.1 log-only provenance, not exposed as a geographic claim.
    std::string outdoorReferenceLabel;
    double outdoorReferenceDistanceRaw{-1.0};
    double outdoorReferenceInfluence{0.0};
    std::uint32_t outdoorReferenceCount{0};
    std::optional<double> estimatedIndoorAmbientC{};
    IndoorEstimateBasis indoorEstimateBasis{IndoorEstimateBasis::Unavailable};
    // Alpha-42 owned extensions. V1 serializer deliberately ignores these.
    std::optional<SeasonState> season;
    BiomeIdentity biome;
    std::optional<double> previewAirC; // explicitly experimental, never legacy air

};

[[nodiscard]] EnvironmentSnapshot AssembleEnvironmentSnapshot(const EnvironmentSnapshotInput& input);
// Tested portable serializer for the versioned public DLL data contract.
// No allocations or live Skyrim engine access; writes only the supplied POD.
[[nodiscard]] bool ToPublicSnapshotV1(const EnvironmentSnapshot& input,
                                     AetherclimeEnvironmentSnapshotV1& output) noexcept;

bool ValidSeasonState(const SeasonState&) noexcept;
[[nodiscard]] bool ToPublicSnapshotV3(const EnvironmentSnapshot&,
    AetherclimeEnvironmentSnapshotV3&) noexcept;
// Clears unsuitable biome/preview context in interiors or unavailable snapshots.
void AttachEnvironmentDetails(EnvironmentSnapshot&, const SeasonState&,
    const BiomeIdentity&, std::optional<double> previewAirC = {});
[[nodiscard]] bool ToPublicSnapshotV2(const EnvironmentSnapshot&,
    AetherclimeEnvironmentSnapshotV2&) noexcept;

} // namespace aetherclime
