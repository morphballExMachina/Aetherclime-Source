#pragma once

#include "Aetherclime/HudSettings.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aetherclime {

enum class Season : std::uint8_t { Winter, Spring, Summer, Autumn };
enum class Weather : std::uint8_t { Clear, Cloudy, Rain, Storm, Snow, Blizzard, Unknown };

constexpr std::array<std::string_view, 12> kMonths{
    "Morning Star", "Sun's Dawn", "First Seed", "Rain's Hand",
    "Second Seed", "Mid Year", "Sun's Height", "Last Seed",
    "Hearthfire", "Frostfall", "Sun's Dusk", "Evening Star"
};

std::string_view SeasonName(Season season) noexcept;
std::optional<Season> ParseSeason(std::string_view name) noexcept;

struct Date {
    int month{0};       // 0=Morning Star, 11=Evening Star
    int day{1};         // 1-based
    double hour{12.0};  // [0,24)
    int year{201};      // Tamrielic year; used only to seed anomalies
};

struct Region {
    std::string name{"Temperate Skyrim"};
    // Degrees Celsius. Order: Winter, Spring, Summer, Autumn.
    std::array<double, 4> seasonalMeanC{-1.0, 9.0, 20.0, 9.0};
    double diurnalAmplitudeC{4.5};
    double altitudeLapseCPerKm{6.0};
    double permanentSnowBiasC{0.0};
    double waterMeanC{7.0};
    bool saltWater{false};
};

// These are sampled outdoor reference neighborhoods, NOT hold boundaries.
// All coordinates and radii are raw Skyrim XY units, not meters.
struct GeographicClimateAnchor {
    std::string label;
    std::string regionKey;
    std::uint32_t worldspaceFormId{0};
    double xRaw{0.0};
    double yRaw{0.0};
    double radiusRawUnits{0.0};
};

// Alpha-27: second, lower-confidence survey tier. A measured coordinate
// supports a nearby CLIMATE ESTIMATE only. It is not verified hold containment,
// and must never authorize verified weather or trusted local ambient values.
// Kept separate from geographicAnchors to make accidental trust impossible.
using ProvisionalClimateSample = GeographicClimateAnchor;

// Alpha-28: bounded, user-editable geographic patches. The supplied sample
// rectangle is a PROVISIONAL research footprint, NOT a known hold boundary.
// Unlike survey circles, patches are extensible through JSON without C++ changes.
struct ProvisionalAtlasZone {
    std::string label;
    std::string regionKey;
    std::uint32_t worldspaceFormId{0};
    double minXRaw{0.0};
    double maxXRaw{0.0};
    double minYRaw{0.0};
    double maxYRaw{0.0};
};

// Relative elevation calibration. Reference Z is a measured exterior game
// coordinate, NOT an altitude above sea level. A climate region may have
// several local reference points, limited by their XY coverage radii.
struct ElevationReference {
    std::string label;
    std::string regionKey;
    std::uint32_t worldspaceFormId{0};
    double xRaw{0.0};
    double yRaw{0.0};
    double zRaw{0.0};
    double radiusRawUnits{0.0};
};

struct ElevationSettings {
    // Initially disabled for safe in-game preview. Flip to true after checking
    // the proposed correction; toggling off restores the existing model.
    bool enabled{false};
    double rawUnitsPerMeter{70.0}; // CK approximation, NOT an absolute altitude datum
    double maxRiseMeters{2000.0};
    double maxCoolingC{12.0};
    std::vector<ElevationReference> references;
};

// Optional explicit inn overrides. Alpha-49 adds safe generic indoor estimates
// without requiring individual Tamriel or mod-added interiors to be named.
struct InteriorLocationRule {
    std::string name;
    std::string regionKey;
};

struct InteriorSettings {
    bool enabled{true}; // Alpha-49: generic indoor estimates enabled for new installs
    bool estimateUnclassified{true}; // do not require a per-interior registry
    double baselineC{16.0};       // conceptual heated shelter baseline at 0C outdoors
    double outdoorCoupling{0.25}; // indoor temperature difference per 1C outdoor change
    double minAmbientC{-5.0};
    double maxAmbientC{30.0};
    std::vector<InteriorLocationRule> locations;
};

// Alpha-31: opt-in SHADOW continuity preview settings. No public or game
// temperature is changed by this preview; these values govern diagnostics only.
struct TemperatureContinuitySettings {
    bool previewEnabled{false};
    double fadeDistanceRawUnits{4096.0};
    double maxCompensationC{6.0};
    double maxLifetimeGameHours{0.5};
};

struct NativeRegionProfileMapping {
    std::string plugin;
    std::uint32_t localFormId{};
    std::string regionKey;
};

struct BiomeClimateProfile {
    std::array<double,4> seasonalMeanC{}; // Winter, Spring, Summer, Autumn; includes any snow bias
    double diurnalAmplitudeC{};
};
// Optional Tamriel-only normalized north/south gradient. Offsets use the same
// Winter/Spring/Summer/Autumn order as a biome's base profile.
struct BiomeNorthSouthGradient {
    std::array<double,4> southOffsetC{}, northOffsetC{};
};
struct BiomeClimateSettings {
    bool enabled{false};
    double maxGapRawUnits{4096.0}; // 0 disables extrapolation
    double gapSeparationRawUnits{1024.0};
    std::unordered_map<std::string,BiomeClimateProfile> profiles;
    bool spatialGradientEnabled{false};
    double gradientSouthYRaw{52000.0}, gradientNorthYRaw{175000.0};
    std::unordered_map<std::string,BiomeNorthSouthGradient> gradients;
};

struct Config {
    std::array<Season, 12> months{
        Season::Winter, Season::Winter, Season::Spring, Season::Spring,
        Season::Spring, Season::Summer, Season::Summer, Season::Summer,
        Season::Autumn, Season::Autumn, Season::Autumn, Season::Winter
    };
    std::array<int, 12> daysInMonth{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int transitionDays{9}; // Blend each side of a seasonal boundary; 0 = abrupt.
    double anomalyAmplitudeC{3.0}; // Smooth, deterministic multi-day heat/cold swings.
    std::uint32_t weatherSeed{1337};
    std::string fallbackRegion{"temperate"};
    // Experimental adapter setting; off by default because enabling it
    // installs a PlayerCharacter::Update vtable hook on supported runtimes.
    bool experimentalLiveMonitor{false};
    double liveMonitorIntervalSeconds{10.0};
    // Explicitly opt-in and bounded Alpha-9 movement telemetry. With both
    // experimental flags false, no player-update hook is installed.
    bool experimentalMovementProbe{false};
    // Alpha-10: extra opt-in; requires movement probe. Only recorded polygon
    // snapshots; never writes weather, touches the region selector or runs
    // geometry on every player update.
    bool experimentalBoundaryProbe{false};
    // Alpha-11: optional low-rate RE::Sky correlation observations.
    // Requires BOTH movement and boundary probes; off by default.
    bool experimentalWeatherCorrelationProbe{false};
    double movementProbeIntervalSeconds{2.0};
    double movementProbeMinDistanceRawUnits{256.0};
    std::uint32_t movementProbeMaxSamples{600};
    std::uint32_t movementProbeMaxReports{80};
    // Alpha-17: geographic fallback only when no location-name evidence
    // resolves. Does not read Sky::region or weather REGN priority.
    bool geographicFallbackEnabled{true};
    // Alpha-26 opt-in: recent verified exterior reference, short distance only.
    // Provisional, never authoritatively trusted; no new weather capture rights.
    bool geographicContinuityEnabled{false};
    // Alpha-27 opt-in: static, bounded low-confidence survey samples.
    bool geographicSurveyEnabled{false};
    // Alpha-28: opt-in, strictly provisional rectangular survey atlas.
    bool geographicAtlasEnabled{false};
    bool apiConsumerProbeEnabled{false}; // Alpha-43: once-per-session save-time external DLL diagnostics
    bool nativeRegionShadowEnabled{false};
    bool nativeRegionGeometryExportEnabled{false};
    std::vector<NativeRegionProfileMapping> nativeRegionProfiles;
    BiomeClimateSettings biomeClimate;
    // Alpha-23: opt-in event sinks for cell/location transitions; NO update hook.
    bool eventClimateEnabled{false};
    TemperatureContinuitySettings temperatureContinuity{};
    std::vector<GeographicClimateAnchor> geographicAnchors;
    std::vector<ProvisionalClimateSample> provisionalSurveySamples;
    std::vector<ProvisionalAtlasZone> provisionalAtlasZones;
    ElevationSettings elevation;
    InteriorSettings interior;
    HudSettings hud;
    std::unordered_map<std::string, Region> regions;
};

Config DefaultConfig();

struct SeasonState {
    Season official{Season::Winter};
    std::array<double, 4> weights{1.0, 0.0, 0.0, 0.0};
    double progress{0.0}; // Fraction through the current contiguous season, 0..1.
};

struct Sample {
    Date date;
    std::string region{"temperate"};
    Weather weather{Weather::Unknown};
    double elevationAboveReferenceM{0.0};
    double windKmh{0.0};
    bool underwater{false};
    double waterDepthM{0.0};
};

struct Reading {
    SeasonState season;
    std::string region;
    double seasonalBaselineC{0.0};
    double diurnalOffsetC{0.0};
    double weatherOffsetC{0.0};
    double anomalyOffsetC{0.0};
    double altitudeOffsetC{0.0};
    double airC{0.0};
    double feelsLikeC{0.0};
    double waterC{0.0};
    bool underwater{false};
};

class Climate {
public:
    explicit Climate(Config config = DefaultConfig());
    const Config& Settings() const noexcept { return config_; }
    SeasonState GetSeason(const Date& date) const;
    Reading Calculate(const Sample& sample) const;
    static double ToFahrenheit(double celsius) noexcept { return celsius * 1.8 + 32.0; }
private:
    Config config_;
};

} // namespace aetherclime
