#define AETHERCLIME_BUILDING_DLL 1
#include "Aetherclime/Climate.h"
#include "Aetherclime/ClimateRegionResolver.h"
#include "Aetherclime/ClimateEstimates.h"
#include "Aetherclime/TravelDiscontinuity.h"
#include "Aetherclime/FallbackTransitionAudit.h"
#include "Aetherclime/ClimateGeographyShadowAudit.h"
#include "Aetherclime/ClimateCoverageAudit.h"
#include "Aetherclime/ElevationModel.h"
#include "Aetherclime/IndoorClimateModel.h"
#include "Aetherclime/ExteriorWeatherContext.h"
#include "Aetherclime/EventClimateContext.h"
#include "Aetherclime/LocationIdentity.h"
#include "Aetherclime/RegionalContinuity.h"
#include "Aetherclime/TemperatureHandoffAudit.h"
#include "Aetherclime/TemperatureHandoffCoverage.h"
#include "Aetherclime/GeographicGapReport.h"
#include "Aetherclime/AtlasProposalReview.h"
#include "Aetherclime/GeographicFallbackReason.h"
#include "Aetherclime/TemperatureContinuityPreview.h"
#include "Aetherclime/GeographicEvidence.h"
#include "Aetherclime/RegionAtlas.h"
#include "Aetherclime/NativeRegionAtlas.h"
#include "Aetherclime/BiomeClimatePreview.h"
#include "Aetherclime/ConditionQuery.h"
#include "ApiProbeHost.h"
#include "Aetherclime/EnvironmentalSnapshot.h"
#include "Aetherclime/EnvironmentContextAudit.h"
#include "Aetherclime/HudNotifications.h"
#include <RE/M/Misc.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include "Aetherclime/PluginApi.h"
#include "Aetherclime/Environment.h"
#include "Aetherclime/Monitor.h"
#include "Aetherclime/MovementProbe.h"
#include "Aetherclime/RegionTransitionProbe.h"
#include "Aetherclime/WeatherCorrelationProbe.h"
#include "Aetherclime/SkyTransitionLifecycleProbe.h"
#include "Aetherclime/RegionPointerTimingProbe.h"
#include "Aetherclime/RegionSpecificCorrelationProbe.h"
#include "Aetherclime/SkyPointerGeographyAudit.h"
#include "Aetherclime/WorldPosition.h"
#include "Aetherclime/WeatherRegions.h"
#include "Aetherclime/WeatherPriorityAudit.h"
#include "Aetherclime/RegionPolygonAudit.h"
#include "SettingsJson.h"

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <RE/T/TESRegion.h>
#include <RE/T/TESDataHandler.h>
// TESRegion::pointLists exposes the loaded RPLD/RPLI polygon coordinates.
// We inspect them only, without writing back or invoking the engine selector.
#include <RE/T/TESRegionList.h>
#include <RE/T/TESRegionData.h>
#include <RE/T/TESRegionDataWeather.h>
#include <RE/W/WeatherType.h>
#include <RE/B/BGSActorCellEvent.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/T/TESActorLocationChangeEvent.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <type_traits>
#include <chrono>
#include <exception>
#include <iomanip>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <utility>
#include <unordered_set>
#include <vector>

namespace {
std::unique_ptr<aetherclime::Climate> g_climate;
std::unique_ptr<aetherclime::Monitor> g_monitor;
// Alpha-22: copied, event-sampled exterior weather; never stores engine pointers.
aetherclime::ExteriorWeatherContext g_exteriorWeather;
// Alpha-23: no Update hook; the gate only runs on explicit event notifications.
aetherclime::ClimateEventGate g_climateEventGate;
// Alpha-24: semantic context gate; distinct from the per-cell observation cache.
aetherclime::OutdoorChangeGate g_outdoorChangeGate;
// Alpha-26: fixed-origin, short-range and always provisional. Never a weather trust source.
aetherclime::RegionalContinuity g_regionalContinuity;
aetherclime::TravelDiscontinuity g_travelDiscontinuity;
aetherclime::FallbackTransitionAudit g_fallbackTransitionAudit;
// Alpha-30: read-only, bounded observation of source/profile temperature steps.
aetherclime::TemperatureHandoffAudit g_temperatureHandoffAudit;
// Alpha-31: bounded shadow proposal; never changes exported/model air temperature.
aetherclime::TemperatureContinuityPreview g_temperatureContinuityPreview;
// Alpha-33: count ALL sampled handoffs, even after a routine log budget fills.
aetherclime::TemperatureHandoffCoverage g_temperatureHandoffCoverage;
// Alpha-34/35: ranked geographic candidates, corridors and fallback gates; no world edits.
aetherclime::GeographicGapReport g_geographicGapReport;
aetherclime::AtlasProposalReview g_atlasProposalReview;
aetherclime::NativeRegionAtlas g_nativeAtlas;
aetherclime::NativeShadowCoverage g_nativeShadowCoverage;
unsigned g_nativeShadowLines=0, g_nativeShadowSummaries=0;
unsigned g_biomePreviewLines=0, g_biomePreviewSummaries=0;
std::size_t g_biomeSamples=0,g_biomeAvailable=0,g_biomeOverlap=0,g_biomeGap=0;
bool g_apiConsumerProbeAttempted=false;
constexpr auto kMilestoneMarker="AETHERCLIME_BUILD_ALPHA50_RELEASE_READINESS";
aetherclime::HudNotifications g_hud;
bool g_hudFailed=false;
bool g_hudMenuSinkRegistered=false;
std::atomic<bool> g_hudMenuTaskPending{false};
aetherclime::TemperatureHandoffReportBudget g_temperatureHandoffReportBudget;
std::uint32_t g_temperatureHandoffSummaryReports = 0;
constexpr std::uint32_t kTemperatureHandoffSummaryLimit = 8;
std::uint32_t g_untrustedTransitionReports = 0;
constexpr std::uint32_t kUntrustedTransitionReportLimit = 24;
std::uint32_t g_outdoorChangeLogCount = 0;
constexpr std::uint32_t kOutdoorChangeLogLimit = 48;
bool g_cellEventSinkRegistered = false;
bool g_locationEventSinkRegistered = false;
std::uint32_t g_eventReportCount = 0;
std::uint32_t g_earlyEventReportCount = 0;
constexpr std::uint32_t kEventReportLimit = 64;
// A finite number of queued main-thread tasks is permitted, not an Update hook.
// Epoch invalidation prevents deferred tasks from an older save/new game from
// capturing weather in the newly loaded session.
std::atomic<std::uint64_t> g_deferredSessionEpoch{1};
std::atomic<unsigned> g_deferredPendingTasks{0};
constexpr unsigned kMaxConcurrentDeferredTasks = 12;
std::unique_ptr<aetherclime::MovementProbe> g_movementProbe;
std::unique_ptr<aetherclime::RegionTransitionProbe> g_boundaryTransitionProbe;
std::unique_ptr<aetherclime::WeatherCorrelationProbe> g_weatherCorrelationProbe;
std::unique_ptr<aetherclime::SkyTransitionLifecycleProbe> g_skyLifecycleProbe;
std::unique_ptr<aetherclime::RegionPointerTimingProbe> g_regionTimingProbe;
std::unique_ptr<aetherclime::RegionSpecificCorrelationProbe> g_regionSpecificProbe;
bool g_weatherCorrelationSessionActive = false;
bool g_skyLifecycleLogBudgetAnnounced = false;
bool g_regionTimingBudgetAnnounced = false;
bool g_regionSpecificBudgetAnnounced = false;
// Cached by exterior CELL. Contains copied coordinates, NEVER live form pointers.
using BoundaryCachedRegion = aetherclime::SkyPointerCandidateGeometry;
struct BoundaryCellCache {
    std::uint32_t cellFormId{0};
    std::uint32_t worldspaceFormId{0};
    bool initialized{false};
    bool complete{false};
    std::vector<BoundaryCachedRegion> regions;
};
BoundaryCellCache g_boundaryCellCache;
std::uint32_t g_boundaryTransitionLogCount = 0;
constexpr std::uint32_t kBoundaryTransitionLogBudget = 32;
bool g_boundaryProbeSessionActive = false;
std::chrono::steady_clock::time_point g_monitorEpoch = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point g_nextLiveObservation = std::chrono::steady_clock::now();
std::chrono::steady_clock::time_point g_nextMovementObservation = std::chrono::steady_clock::now();
bool g_liveHookInstalled = false;
bool g_liveSessionActive = false;
bool g_movementProbeSessionActive = false;

// Alpha-18: Collect game evidence once on the stack. The effective resolver
// preserves the full name chain; only a one-shot *shadow* on load/save excludes
// names to exercise Alpha-17's geographic fallback against real exterior XY.
// No new Update hook or continual scanning is required.
const aetherclime::Climate& GetClimate();
// Forward declaration: Alpha-20 elevation audit uses Snapshot() before its definition.
aetherclime::Sample Snapshot();
// Alpha-30 is also invoked by the event sampler defined earlier in this file.
void LogTemperatureHandoff(std::string_view trigger);

struct PlayerClimateEvidence {
    std::array<aetherclime::ClimateLocationEvidence, 16> locations{};
    std::size_t count{0};
    bool hasCell{false};
    bool interior{false};
    bool usedCellLocation{false};
    std::optional<aetherclime::ClimateWorldPoint> exteriorPosition{};

    aetherclime::ClimateRegionContext Context() const {
        return {
            .locationChain = std::span<const aetherclime::ClimateLocationEvidence>(locations.data(), count),
            .hasCell = hasCell,
            .interior = interior,
            .usedCellLocation = usedCellLocation,
            .exteriorPosition = exteriorPosition
        };
    }
};

PlayerClimateEvidence CollectPlayerClimateEvidence(const RE::PlayerCharacter* player) {
    PlayerClimateEvidence evidence;
    const auto* cell = player ? player->GetParentCell() : nullptr;
    const RE::BGSLocation* location = player ? player->GetCurrentLocation() : nullptr;
    evidence.hasCell = cell != nullptr;
    evidence.interior = cell && cell->IsInteriorCell();
    evidence.usedCellLocation = !location && cell;
    if (evidence.usedCellLocation) location = cell->GetLocation();
    for (; location && evidence.count < evidence.locations.size(); location = location->parentLoc) {
        const char* name = location->GetName();
        const char* editorId = location->GetFormEditorID();
        evidence.locations[evidence.count++] = {
            name ? std::string_view(name) : std::string_view{},
            editorId ? std::string_view(editorId) : std::string_view{},
            location->GetFormID()
        };
    }
    // These are player *world* XY coordinates only when the CELL is exterior.
    // Interior local positions must not be treated as Tamriel positions.
    if (player && cell && !cell->IsInteriorCell()) {
        if (const auto* worldspace = player->GetWorldspace()) {
            const auto& pos = player->GetPosition();
            const auto checked = aetherclime::CheckWorldPosition(
                true, false, true, {pos.x, pos.y, pos.z});
            if (checked.worldUnits) {
                evidence.exteriorPosition = aetherclime::ClimateWorldPoint{
                    worldspace->GetFormID(), checked.worldUnits->x, checked.worldUnits->y, checked.worldUnits->z
                };
            }
        }
    }
    return evidence;
}

// A proximity-based continuity result is NEVER marked matched/trusted. Its
// purpose is to avoid a sudden warm temperate model across a SHORT gap until
// actual named/anchor geography returns. The verified origin never moves in
// response to a provisional result.
aetherclime::ContinuityAssessment AssessRegionalContinuity(
    const PlayerClimateEvidence& evidence, const aetherclime::Date& date) {
    const auto& config = GetClimate().Settings();
    return g_regionalContinuity.Assess(
        aetherclime::ResolveClimateRegion(config, evidence.Context()),
        evidence.Context(), date, config.geographicContinuityEnabled);
}

void LogRegionalContinuity(std::string_view trigger) {
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto date = Snapshot().date;
    const auto state = AssessRegionalContinuity(evidence, date);
    SKSE::log::info(
        "Regional continuity audit: trigger={} | status={} | effectiveRegion='{}' "
        "| provenance={} | provisional={} | trusted={} | distanceRawUnits={:.1f} "
        "| ageGameHours={:.2f} | maxDistanceRawUnits={:.0f} | maxAgeGameHours={:.1f} "
        "| surveyLabel='{}' | surveyRadiusRawUnits={:.0f} "
        "| atlasLabel='{}' | surveyEnabled={} | atlasEnabled={} "
        "| noWeatherTrustFromContinuity=true "
        "| noChainedAnchor=true | perFramePolling=false",
        trigger, aetherclime::ContinuityStatusName(state.status), state.resolution.regionKey,
        aetherclime::ClimateRegionSourceName(state.resolution.source),
        state.provisional(), state.resolution.matched(), state.distanceRawUnits,
        state.ageGameHours, aetherclime::RegionalContinuity::kMaxDistanceRawUnits,
        aetherclime::RegionalContinuity::kMaxAgeGameHours,
        state.resolution.source == aetherclime::ClimateRegionSource::ProvisionalSurveySample
            ? state.resolution.anchorLabel : "(none)",
        state.resolution.source == aetherclime::ClimateRegionSource::ProvisionalSurveySample
            ? state.resolution.anchorRadiusRawUnits : -1.0,
        state.resolution.source == aetherclime::ClimateRegionSource::ProvisionalAtlasZone
            ? state.resolution.anchorLabel : "(none)",
        GetClimate().Settings().geographicSurveyEnabled,
        GetClimate().Settings().geographicAtlasEnabled);
}

aetherclime::ClimateRegionResolution ResolveRegionForPlayer(const RE::PlayerCharacter* player) {
    const auto evidence = CollectPlayerClimateEvidence(player);
    return aetherclime::ResolveClimateRegion(GetClimate().Settings(), evidence.Context());
}

std::string RegionForPlayer(const RE::PlayerCharacter* player) {
    return ResolveRegionForPlayer(player).regionKey;
}

std::string LocationChain(const RE::BGSLocation* loc) {
    if (!loc) return "(no location record)";
    std::string chain;
    for (unsigned depth = 0; loc && depth < 16; ++depth, loc = loc->parentLoc) {
        if (depth) chain += " -> ";
        const char* name = loc->GetName();
        const char* editorId = loc->GetFormEditorID();
        if (name && *name) chain += name;
        else if (editorId && *editorId) chain += editorId;
        else chain += "(unnamed)";
    }
    if (loc) chain += " -> ...";
    return chain;
}

void LogClimateRegionResolution(std::string_view trigger) {
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto& config = GetClimate().Settings();
    const auto audit = aetherclime::AuditClimateGeography(config, evidence.Context());
    const auto& resolution = audit.effective;
    const auto profile = config.regions.find(resolution.regionKey);
    const auto elevationAssessment = aetherclime::AssessElevation(config, evidence.Context(), resolution.regionKey);
    SKSE::log::info(
        "Climate region resolution: trigger={} | profileKey='{}' | profileName='{}' "
        "| source={} | matched={} | matchedLocation=0x{:08X} | chainDepth={} "
        "| matchedAlias='{}' | usingCellLocation={} | cellType={} "
        "| anchor='{}' | anchorDistanceRawUnits={:.1f} | anchorRadiusRawUnits={:.1f} "
        "| anchorWorldspace=0x{:08X} | geographicEvidence=sample-neighborhood-not-hold-boundary "
        "| weatherRegionBasis=not-used | elevationApplied={} | "
        "temperatureBasis=outdoor-baseline-optional-relative-elevation",
        trigger, resolution.regionKey,
        profile != config.regions.end() ? profile->second.name : "(unconfigured)",
        aetherclime::ClimateRegionSourceName(resolution.source),
        resolution.matched(), resolution.matchedLocationFormId,
        resolution.locationMatched() ? std::to_string(resolution.chainDepth) : "n/a",
        resolution.matchedAlias, resolution.usedCellLocation,
        resolution.interior ? "interior" : "exterior-or-unknown",
        resolution.anchorLabel.empty() ? "(none)" : resolution.anchorLabel,
        resolution.anchorDistanceRawUnits, resolution.anchorRadiusRawUnits,
        resolution.anchorWorldspaceFormId, elevationAssessment.applied);
    // A shadow evaluates the SAME active geographic fallback with only name
    // evidence excluded. It never changes Snapshot().region or climate API.
    SKSE::log::info(
        "Climate geographic shadow: trigger={} | status={} | shadowOnly=true | affectsClimate=false "
        "| effectiveProfile='{}' | effectiveSource={} | geographicProfile='{}' | geographicSource={} "
        "| profileComparison={} | shadowEvaluated={} | positionAvailable={} | worldspace=0x{:08X} "
        "| rawXY=({:.1f},{:.1f}) | anchor='{}' | distanceRawUnits={:.1f} "
        "| radiusRawUnits={:.1f} | locationEvidencePreserved=true "
        "| weatherRegionBasis=not-used | elevationApplied=false | perFramePolling=false",
        trigger, aetherclime::ClimateShadowStatusName(audit.status),
        audit.effective.regionKey, aetherclime::ClimateRegionSourceName(audit.effective.source),
        audit.shadowEvaluated ? audit.shadow.regionKey : "(not-evaluated)",
        audit.shadowEvaluated ? aetherclime::ClimateRegionSourceName(audit.shadow.source) : "not-evaluated",
        audit.profileComparisonAvailable ? (audit.sameProfile ? "agree" : "differ") : "not-compared",
        audit.shadowEvaluated, audit.positionAvailable, audit.worldspaceFormId,
        audit.xRaw, audit.yRaw,
        audit.shadow.anchorLabel.empty() ? "(none)" : audit.shadow.anchorLabel,
        audit.shadow.anchorDistanceRawUnits, audit.shadow.anchorRadiusRawUnits);
    // Alpha-20: same effective climate profile as Snapshot(), but evaluate
    // relative elevation independently. Disabled by default; no player hook.
    // The audit logs both proposed and actually applied cooling to avoid
    // confusing regional OUTDOOR air with interior ambient conditions.
    const auto& relative = elevationAssessment;
    auto sample = Snapshot();
    const auto reading = GetClimate().Calculate(sample);
    SKSE::log::info(
        "Elevation audit: trigger={} | status={} | enabled={} | eligible={} | applied={} "
        "| profile='{}' | reference='{}' | rawZ={:.1f} | referenceRawZ={:.1f} "
        "| distanceXYRawUnits={:.1f} | riseRelativeM={:.2f} | effectiveRiseM={:.2f} "
        "| rawUnitsPerMeter={:.2f} | lapseCPerKm={:.2f} | projectedOffsetC={:.3f} "
        "| actualOffsetC={:.3f} | readingOffsetC={:.3f} | outdoorAirC={:.2f} "
        "| capped={} | verticalBasis=reference-relative-not-absolute-altitude "
        "| outdoorOnly=true | weatherRegionBasis=not-used | perFramePolling=false",
        trigger, aetherclime::ElevationStatusName(relative.status), relative.enabled,
        relative.eligible, relative.applied, resolution.regionKey,
        relative.referenceLabel.empty() ? "(none)" : relative.referenceLabel,
        relative.rawZ, relative.referenceZRaw, relative.distanceXYRaw,
        relative.riseMeters, relative.effectiveRiseMeters,
        config.elevation.rawUnitsPerMeter,
        config.regions.contains(resolution.regionKey) ?
            config.regions.at(resolution.regionKey).altitudeLapseCPerKm : 0.0,
        relative.projectedOffsetC, relative.actualOffsetC, reading.altitudeOffsetC,
        reading.airC, relative.clamped);
    // Alpha-19: one-shot read-only coverage classification using exactly the
    // active anchor settings. These are surveyed SAMPLE circles, not borders.
    // Raw player Z is reported only for future datum/scale calibration.
    const auto coverage = aetherclime::AuditClimateCoverage(config, evidence.Context());
    SKSE::log::info(
        "Climate coverage audit: trigger={} | status={} | shadowOnly=true | affectsClimate=false "
        "| worldspace=0x{:08X} | rawXYZ=({:.1f},{:.1f},{:.1f}) "
        "| rawZAvailable={} | verticalBasis=raw-world-units-uncalibrated "
        "| profile='{}' | nearestAnchor='{}' | distanceRawUnits={:.1f} "
        "| nearestRadiusRawUnits={:.1f} | signedMarginRawUnits={:.1f} "
        "| nearestBoundaryDistanceRawUnits={:.1f} | nearSampleEdge={} "
        "| inRangeAnchors={} | distinctInRangeProfiles={} | worldspaceAnchors={} "
        "| geographyEnabled={} | weatherRegionBasis=not-used "
        "| coverageIsReadOnly=true | perFramePolling=false",
        trigger, aetherclime::ClimateCoverageStatusName(coverage.status),
        coverage.worldspaceFormId, coverage.xRaw, coverage.yRaw,
        coverage.rawZAvailable ? coverage.zRaw : 0.0,
        coverage.rawZAvailable, coverage.nearestRegionKey,
        coverage.nearestAnchorLabel.empty() ? "(none)" : coverage.nearestAnchorLabel,
        coverage.nearestDistanceRawUnits, coverage.nearestRadiusRawUnits,
        coverage.nearestSignedMarginRawUnits, coverage.nearestBoundaryDistanceRawUnits,
        coverage.nearSampleEdge, coverage.inRangeAnchors, coverage.distinctInRangeProfiles,
        coverage.worldspaceAnchors, config.geographicFallbackEnabled);
    // Alpha-27: a SECOND, explicitly lower-confidence geography tier. Audit
    // it independently of Skyrim's named-location ancestry so a normal save
    // at a named landmark still tells us whether the nearby survey would
    // be usable in an adjacent UNNAMED exterior cell. Never changes climate.
    auto surveyContext = evidence.Context();
    surveyContext.locationChain = {};
    auto surveyConfig = config;
    surveyConfig.geographicAtlasEnabled = false;
    const auto surveyShadow = aetherclime::ResolveClimateRegion(surveyConfig, surveyContext);
    SKSE::log::info(
        "Geographic survey audit: trigger={} | enabled={} | configuredSamples={} "
        "| shadowOnly=true | affectsClimate=false | source={} | provisional={} "
        "| candidateProfile='{}' | sample='{}' | distanceRawUnits={:.1f} "
        "| radiusRawUnits={:.1f} | noHoldBoundaryProof=true "
        "| noWeatherTrustFromSurvey=true | perFramePolling=false",
        trigger, config.geographicSurveyEnabled, config.provisionalSurveySamples.size(),
        aetherclime::ClimateRegionSourceName(surveyShadow.source), surveyShadow.provisional(),
        surveyShadow.regionKey,
        surveyShadow.anchorLabel.empty() ? "(none)" : surveyShadow.anchorLabel,
        surveyShadow.anchorDistanceRawUnits, surveyShadow.anchorRadiusRawUnits);
    // Alpha-28: separate atlas shadow on load/save, regardless of named
    // evidence or higher-priority trusted survey circles. Never affects climate.
    const auto atlas = aetherclime::EvaluateRegionAtlas(config, evidence.Context());
    SKSE::log::info(
        "Geographic atlas audit: trigger={} | enabled={} | configuredZones={} "
        "| status={} | candidateProfile='{}' | zone='{}' "
        "| matchingZones={} | worldspace=0x{:08X} "
        "| rectangleXY=({:.1f},{:.1f},{:.1f},{:.1f}) "
        "| shadowOnly=true | affectsClimate=false | provisionalOnly=true "
        "| noHoldBoundaryProof=true | noWeatherTrustFromAtlas=true "
        "| noReferenceScan=true | perFramePolling=false",
        trigger, config.geographicAtlasEnabled, config.provisionalAtlasZones.size(),
        aetherclime::AtlasStatusName(atlas.status),
        atlas.regionKey.empty() ? "(none)" : atlas.regionKey,
        atlas.zoneLabel.empty() ? "(none)" : atlas.zoneLabel,
        atlas.matchingZones, atlas.worldspaceFormId,
        atlas.minXRaw, atlas.maxXRaw, atlas.minYRaw, atlas.maxYRaw);
}

aetherclime::Sample Snapshot() {
    aetherclime::Sample sample;
    if (const auto* calendar = RE::Calendar::GetSingleton()) {
        sample.date.month = std::clamp(static_cast<int>(calendar->GetMonth()), 0, 11);
        sample.date.day = std::max(1, static_cast<int>(calendar->GetDay()));
        sample.date.hour = std::clamp(static_cast<double>(calendar->GetHour()), 0.0, 23.999);
        sample.date.year = calendar->gameYear ? static_cast<int>(calendar->gameYear->value) : 201;
    }
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto evidence = CollectPlayerClimateEvidence(player);
    // Alpha-49: a new game / freshly loaded exterior establishes its own
    // baseline. Long carriage/teleport hops invalidate the old nearby-region
    // and cached exterior weather WITHOUT guessing a path between endpoints.
    const auto travel = g_travelDiscontinuity.Observe(evidence.exteriorPosition,sample.date);
    if (travel==aetherclime::TravelEvent::FarJump ||
        travel==aetherclime::TravelEvent::WorldspaceChanged ||
        travel==aetherclime::TravelEvent::CalendarRolledBack) {
        g_regionalContinuity.Reset();
        g_exteriorWeather.Clear();
        SKSE::log::info("Travel context reset: reason={} | cacheCleared=true "
            "| regionContinuityCleared=true | noWeatherTrust=true | noPositionInterpolation=true "
            "| perFramePolling=false",aetherclime::TravelEventName(travel));
    }
    const auto& config = GetClimate().Settings();
    const auto assessedRegion = AssessRegionalContinuity(evidence, sample.date);
    const auto& effective = assessedRegion.resolution;
    sample.region = effective.regionKey;
    // A recognized inn's configured outdoor region may resolve an interior
    // with missing/unrecognized BGSLocation ancestry. Never replace trusted
    // location evidence or use local interior XYZ as world coordinates.
    if (evidence.interior && player) {
        const auto* cell = player->GetParentCell();
        const auto* location = player->GetCurrentLocation();
        const auto indoorContext = aetherclime::ResolveInteriorClimateContext(
            config, evidence.hasCell, evidence.interior,
            location && location->GetName() ? location->GetName() : "",
            cell && cell->GetName() ? cell->GetName() : "",
            effective.regionKey, effective.matched());
        if (indoorContext.eligible && indoorContext.usedRuleRegion) {
            sample.region = indoorContext.regionKey;
        }
    }
    const auto assessment = aetherclime::AssessElevation(GetClimate().Settings(), evidence.Context(), sample.region);
    if (assessment.applied) sample.elevationAboveReferenceM = assessment.effectiveRiseMeters;
    if (const auto* sky = RE::Sky::GetSingleton()) {
        if (sky->IsSnowing()) sample.weather = aetherclime::Weather::Snow;
        else if (sky->IsRaining()) sample.weather = aetherclime::Weather::Rain;
        else if (sky->currentWeather && sky->currentWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kCloudy)) {
            sample.weather = aetherclime::Weather::Cloudy;
        } else sample.weather = aetherclime::Weather::Clear;
    }
    // When inside, prefer a recent verified exterior weather observation from
    // the *same* climate region. We never capture interior Sky as exterior data.
    if (evidence.interior) {
        const auto trustedResolution = aetherclime::ResolveClimateRegion(config, evidence.Context());
        bool trusted = trustedResolution.matched();
        if (!trusted && player) {
            const auto* cell = player->GetParentCell();
            const auto* location = player->GetCurrentLocation();
            const auto context = aetherclime::ResolveInteriorClimateContext(
                config, evidence.hasCell, true,
                location && location->GetName() ? location->GetName() : "",
                cell && cell->GetName() ? cell->GetName() : "",
                trustedResolution.regionKey, false);
            trusted = context.eligible && context.regionKey == sample.region;
        }
        const auto retained = g_exteriorWeather.Assess({
            evidence.hasCell, true, trusted, sample.region, sample.date
        });
        if (retained.usable && retained.observation) {
            sample.weather = retained.observation->weather;
        }
    }
    // Relative Z elevation is applied only against matching validated references.
    // Indoor ambient is intentionally SEPARATE from this outdoor-air sample;
    // fire proximity and water exposure remain future features.
    return sample;
}

const aetherclime::Climate& GetClimate() {
    if (!g_climate) g_climate = std::make_unique<aetherclime::Climate>();
    return *g_climate;
}

std::string_view WeatherLabel(aetherclime::Weather weather) {
    switch (weather) {
    case aetherclime::Weather::Clear: return "clear";
    case aetherclime::Weather::Cloudy: return "cloudy";
    case aetherclime::Weather::Rain: return "rain";
    case aetherclime::Weather::Storm: return "storm";
    case aetherclime::Weather::Snow: return "snow";
    case aetherclime::Weather::Blizzard: return "blizzard";
    default: return "unknown";
    }
}

// Alpha-22: cached exterior weather is a trustworthy input only when both
// the region and game-time window match. On direct interior loads without a
// prior exterior observation, the legacy Sky sample remains explicitly
// unverified as an *outdoor* weather reading.
aetherclime::ExteriorWeatherAssessment ExteriorWeatherForSample(
    const aetherclime::Sample& sample,
    const PlayerClimateEvidence& evidence) {
    const auto& config = GetClimate().Settings();
    const auto resolved = aetherclime::ResolveClimateRegion(config, evidence.Context());
    bool regionTrusted = resolved.matched();
    if (evidence.interior && !regionTrusted) {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        const auto* cell = player ? player->GetParentCell() : nullptr;
        const auto* location = player ? player->GetCurrentLocation() : nullptr;
        const auto context = aetherclime::ResolveInteriorClimateContext(
            config, evidence.hasCell, evidence.interior,
            location && location->GetName() ? location->GetName() : "",
            cell && cell->GetName() ? cell->GetName() : "",
            resolved.regionKey, false);
        regionTrusted = context.eligible && context.regionKey == sample.region;
    }
    return g_exteriorWeather.Assess({
        evidence.hasCell, evidence.interior, regionTrusted, sample.region, sample.date
    });
}

std::string_view OutdoorWeatherSource(const aetherclime::ExteriorWeatherAssessment& assessment,
                                      bool interior) {
    if (!interior) return "current-exterior-Sky";
    return assessment.usable ? "cached-verified-exterior" : "current-interior-Sky-unverified";
}

// Alpha-24: read-only semantic detector. The existing exterior cache may be
// refreshed on every exterior cell crossing; this separate gate distinguishes
// real weather/region changes from unchanged repeats. Logging is bounded;
// consumers are NOT notified yet and no weather/temperature is altered.
void ObserveOutdoorChange(std::string_view trigger, std::string_view region,
                          std::uint32_t weatherId) {
    const auto result = g_outdoorChangeGate.Observe({true, true, std::string(region), weatherId});
    const bool meaningful = aetherclime::IsMeaningfulOutdoorChange(result);
    // Never bury meaningful changes after a long trek: cap only repetitive
    // same-weather/same-region diagnostics, not changes themselves.
    if (meaningful || g_outdoorChangeLogCount < kOutdoorChangeLogLimit) {
        if (!meaningful) ++g_outdoorChangeLogCount;
        SKSE::log::info(
            "Outdoor change audit: trigger={} | decision={} | meaningful={} | "
            "region='{}' | weather=0x{:08X} | exteriorVerified=true | "
            "consumerNotification=false | noWeatherWrites=true | perFramePolling=false",
            trigger, aetherclime::OutdoorChangeKindName(result),
            meaningful, region, weatherId);
    }
}

// Event-driven only: on exterior load/save, take a copied observation from
// the actual outdoor Sky. On interior load/save, log why the last exterior
// observation is or is not applicable. NO weather record modifications.
void LogExteriorWeatherContext(std::string_view trigger) {
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto sample = Snapshot();
    const auto region = aetherclime::ResolveClimateRegion(GetClimate().Settings(), evidence.Context());
    if (GetClimate().Settings().geographicContinuityEnabled) {
        (void)g_regionalContinuity.ObserveVerified(region, evidence.Context(), sample.date);
    }
    const auto* sky = RE::Sky::GetSingleton();
    const auto* weather = sky ? sky->currentWeather : nullptr;
    const auto* worldspace = player ? player->GetWorldspace() : nullptr;
    bool captured = false;
    if (evidence.hasCell && !evidence.interior && weather && worldspace) {
        captured = g_exteriorWeather.CaptureExterior(
            true, region.matched(), sample.region, sample.weather,
            weather->GetFormID(), worldspace->GetFormID(), sample.date);
    }
    const auto assessment = ExteriorWeatherForSample(sample, evidence);
    // Assessment is side-effect free; record the captured weather FormID,
    // not a potentially unrelated interior Sky weather FormID.
    const auto& obs = assessment.observation;
    SKSE::log::info(
        "Exterior weather context: trigger={} | status={} | cellType={} "
        "| region='{}' | captured={} | cacheUsable={} | capturedRegion='{}' "
        "| capturedWeather=0x{:08X} | capturedWeatherType={} | ageGameHours={:.2f} "
        "| maxAgeGameHours={:.1f} | outdoorWeatherSource={} | "
        "sameCalendarDayRequired=true | fromExteriorCellOnly=true | "
        "loadGameCacheReset=true | perFramePolling=false",
        trigger, aetherclime::ExteriorWeatherStatusName(assessment.status),
        evidence.hasCell ? (evidence.interior ? "interior" : "exterior") : "unknown",
        sample.region, captured, assessment.usable,
        obs ? obs->regionKey : "(none)", obs ? obs->weatherFormId : 0,
        obs ? WeatherLabel(obs->weather) : "(none)", assessment.ageGameHours,
        aetherclime::ExteriorWeatherContext::kMaxAgeGameHours,
        OutdoorWeatherSource(assessment, evidence.interior));
    if (captured) ObserveOutdoorChange(trigger, sample.region, weather->GetFormID());
}

// TESRegion records are candidate evidence, not an authoritative choice of
// the player's active climate region. Overlapping weather/land regions can
// exist. Passing false avoids creating a missing cell region list.
// Plain region listing includes NON-weather regions. Region weather discovery
// below separately inspects the actual RDAT data entries.
std::string RegionCandidates(RE::TESObjectCELL* cell) {
    if (!cell || !cell->IsExteriorCell()) return "(interior-or-unknown)";
    auto* list = cell->GetRegionList(false);
    if (!list) return "(none-present)";

    std::ostringstream result;
    std::size_t count = 0;
    constexpr std::size_t kSafetyCap = 256;
    for (auto* region : *list) {
        if (!region) continue;
        if (count == kSafetyCap) {
            result << ",...(safety cap 256 reached)";
            break;
        }
        if (count++) result << ',';
        const char* editorId = region->GetFormEditorID();
        result << (editorId && *editorId ? editorId : "(unnamed)")
               << ":0x" << std::uppercase << std::hex
               << std::setfill('0') << std::setw(8) << region->GetFormID()
               << std::dec;
    }
    return count ? result.str() : "(empty)";
}

// Diagnostic approximation of a REGN's recorded two-dimensional footprint.
// Coordinate and worldspace evidence are separate from engine selection.
// Defensive caps avoid unbounded logging/allocations on malformed mod data.
struct RecordedRegionGeometry {
    aetherclime::RegionGeometryAssessment footprint;
    aetherclime::RegionEdgeEvidence edgeEvidence;
    aetherclime::RegionPolygonAudit polygonAudit;
};

RecordedRegionGeometry InspectRegionFootprint(
    RE::TESRegion* region,
    std::optional<aetherclime::RegionPoint2> position,
    std::uint32_t playerWorldspaceId) {
    constexpr std::size_t kMaxPolygons = 128;
    constexpr std::size_t kMaxVerticesPerPolygon = 2048;
    constexpr std::size_t kMaxTotalVertices = 8192;
    std::vector<aetherclime::RegionPolygonWithRPLI> polygonsWithRPLI;
    bool incomplete = false;
    std::size_t totalVertices = 0;

    if (region && region->pointLists) {
        for (auto* list : *region->pointLists) {
            if (!list) {
                incomplete = true;
                continue;
            }
            if (polygonsWithRPLI.size() >= kMaxPolygons) {
                incomplete = true;
                break;
            }
            aetherclime::RegionPolygon polygon;
            for (auto* point : *list) {
                if (!point) {
                    incomplete = true;
                    continue;
                }
                if (polygon.size() >= kMaxVerticesPerPolygon ||
                    totalVertices >= kMaxTotalVertices) {
                    incomplete = true;
                    break;
                }
                polygon.push_back({point->point.x, point->point.y});
                ++totalVertices;
            }
            polygonsWithRPLI.push_back({std::move(polygon), list->distanceInsideAtMax});
            if (totalVertices >= kMaxTotalVertices) {
                incomplete = true;
                break;
            }
        }
    }
    std::optional<bool> sameWorldspace;
    if (region && region->worldSpace && playerWorldspaceId != 0) {
        sameWorldspace = region->worldSpace->GetFormID() == playerWorldspaceId;
    }
    std::vector<aetherclime::RegionPolygon> polygons;
    polygons.reserve(polygonsWithRPLI.size());
    for (const auto& polygon : polygonsWithRPLI) polygons.push_back(polygon.points);
    RecordedRegionGeometry result;
    result.footprint = aetherclime::AssessRegionFootprint(
        polygons, position, sameWorldspace, incomplete);
    result.edgeEvidence = aetherclime::AssessRecordedEdgeProximity(
        polygonsWithRPLI, position, sameWorldspace, incomplete);
    result.polygonAudit = aetherclime::AssessEveryRecordedPolygon(
        polygonsWithRPLI, position, sameWorldspace, incomplete);
    return result;
}

// Read only: inspect the effective in-memory TESRegion instances, which may
// have been overridden by the active modlist. This does not determine the
// winning plugin's filename or which polygon/rule Skyrim currently uses.
struct WeatherDiscovery {
    std::size_t regionCandidates{0};
    bool truncated{false};
    std::vector<aetherclime::RegionWeatherCandidate> weatherRegions;
};

WeatherDiscovery DiscoverRegionWeather(RE::TESObjectCELL* cell) {
    WeatherDiscovery out;
    if (!cell || !cell->IsExteriorCell()) return out;
    auto* list = cell->GetRegionList(false);
    if (!list) return out;

    std::optional<aetherclime::RegionPoint2> playerPosition;
    std::uint32_t playerWorldspaceId = 0;
    if (auto* player = RE::PlayerCharacter::GetSingleton();
        player && player->GetParentCell() == cell) {
        const auto& pos = player->GetPosition();
        playerPosition = aetherclime::RegionPoint2{pos.x, pos.y};
        if (const auto* ws = player->GetWorldspace()) {
            playerWorldspaceId = ws->GetFormID();
        }
    }

    constexpr std::size_t kRegionCap = 256;
    constexpr std::size_t kDataEntryCap = 256;
    constexpr std::size_t kWeatherChoiceCap = 256;
    for (auto* region : *list) {
        if (!region) continue;
        if (out.regionCandidates == kRegionCap) {
            out.truncated = true;
            break;
        }
        ++out.regionCandidates;
        if (!region->dataList) continue;

        // Non-const iteration works around the existing CommonLibSSE-NG
        // BSSimpleList const-iterator issue; the records aren't modified.
        std::size_t dataCount = 0;
        for (auto* data : region->dataList->regionDataList) {
            if (!data) continue;
            if (dataCount++ == kDataEntryCap) {
                out.truncated = true;
                break;
            }
            if (data->GetType() != RE::TESRegionData::Type::kWeather) continue;
            auto* weatherData = static_cast<RE::TESRegionDataWeather*>(data);
            aetherclime::RegionWeatherCandidate candidate;
            candidate.regionFormId = region->GetFormID();
            candidate.worldspaceFormId = region->worldSpace ? region->worldSpace->GetFormID() : 0;
            candidate.priority = data->dataHeader.priority;
            candidate.overrideFlag = data->dataHeader.flags.any(
                RE::TESRegionData::DataHeader::Flag::kOverride);
            const auto geometry = InspectRegionFootprint(
                region, playerPosition, playerWorldspaceId);
            candidate.footprint = geometry.footprint;
            candidate.edgeEvidence = geometry.edgeEvidence;
            candidate.polygonAudit = geometry.polygonAudit;
            std::size_t choices = 0;
            for (auto* type : weatherData->weatherTypes) {
                if (!type) continue;
                if (choices++ == kWeatherChoiceCap) {
                    out.truncated = true;
                    break;
                }
                candidate.choices.push_back({
                    type->weather ? type->weather->GetFormID() : 0,
                    type->chance,
                    type->global ? type->global->GetFormID() : 0
                });
            }
            out.weatherRegions.push_back(std::move(candidate));
        }
    }
    aetherclime::SortWeatherCandidatesForDisplay(out.weatherRegions);
    return out;
}

void LogWeatherRegionDiagnostics() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* cell = player ? player->GetParentCell() : nullptr;
    if (!cell || !cell->IsExteriorCell()) {
        SKSE::log::info("Weather region diagnostics: status=interior-or-no-cell | selection=unresolved");
        return;
    }
    const auto* sky = RE::Sky::GetSingleton();
    const auto observedId = sky && sky->currentWeather ? sky->currentWeather->GetFormID() : 0;
    const auto discovered = DiscoverRegionWeather(cell);
    SKSE::log::info(
        "Weather region diagnostics: status=exterior | cell=0x{:08X} | regionCandidates={} "
        "| weatherEntries={} | observedWeather=0x{:08X} | selection=unresolved "
        "| truncated={}",
        cell->GetFormID(), discovered.regionCandidates, discovered.weatherRegions.size(),
        observedId, discovered.truncated);
    const auto audit = aetherclime::AuditWeatherRegionPriorities(
        discovered.weatherRegions, observedId, discovered.truncated);
    SKSE::log::info(
        "Weather region diagnostics: priority-audit | status={} | inside={} | outside={} "
        "| uncertain={} | topPriorityTies={} | leader=0x{:08X} | leaderPriority={} "
        "| leaderOverride={} | observedInLeader={} | engineSelection=unresolved",
        aetherclime::WeatherPriorityStatusName(audit.status),
        audit.insideCount, audit.outsideCount, audit.indeterminateCount,
        audit.topPriorityTieCount, audit.leadingRegionFormId,
        static_cast<unsigned>(audit.leadingPriority), audit.leadingOverrideFlag,
        audit.observedWeatherInLeader);
    for (const auto& region : discovered.weatherRegions) {
        const auto summary = aetherclime::SummarizeRegionWeather(region, observedId);
        SKSE::log::info(
            "Weather region: form=0x{:08X} | worldspace=0x{:08X} | priority={} | override={} "
            "| options={} | rawWeightTotal={} | withGlobal={} | containsObserved={} "
            "| footprint={} | polygons={} | vertices={} | geometryIncomplete={} "
            "| selection=unresolved",
            region.regionFormId, region.worldspaceFormId,
            static_cast<unsigned>(region.priority), region.overrideFlag,
            summary.choices, summary.rawWeightTotal, summary.choicesWithGlobal,
            summary.containsObservedWeather,
            aetherclime::RegionFootprintName(region.footprint.status),
            region.footprint.polygonCount, region.footprint.vertexCount,
            region.footprint.incomplete);
        // Alpha-8 census: log every polygon and its OWN RPLI, even when
        // that polygon is not the region's nearest recorded edge. This is
        // a record/geometry audit, NEVER an engine weather-blend claim.
        const auto& polygonAudit = region.polygonAudit;
        SKSE::log::info(
            "Weather region: polygon-census | region=0x{:08X} | status={} "
            "| polygons={} | nonZeroRPLI={} | inside={} | outside={} "
            "| boundary={} | engineFalloff=unresolved",
            region.regionFormId,
            aetherclime::EdgeEvidenceStatusName(polygonAudit.status),
            polygonAudit.polygons.size(), polygonAudit.nonZeroRPLICount,
            polygonAudit.insideCount, polygonAudit.outsideCount,
            polygonAudit.boundaryCount);
        for (const auto& p : polygonAudit.polygons) {
            if (p.measured) {
                SKSE::log::info(
                    "Weather region: polygon-audit | region=0x{:08X} "
                    "| polygon={}/{} | vertices={} | RPLI={} raw-units "
                    "| relation={} | nearestRecordedSegment={:.1f} raw-units "
                    "| proximityVsRPLI={} | engineFalloff=unresolved",
                    region.regionFormId, p.polygonIndex + 1,
                    polygonAudit.polygons.size(), p.vertexCount, p.recordedRPLI,
                    aetherclime::PolygonRelationName(p.relation),
                    p.nearestRecordedSegment,
                    aetherclime::RPLIProximityName(p.proximity));
            } else {
                SKSE::log::info(
                    "Weather region: polygon-audit | region=0x{:08X} "
                    "| polygon={}/{} | vertices={} | RPLI={} raw-units "
                    "| relation=not-measured | status={} | engineFalloff=unresolved",
                    region.regionFormId, p.polygonIndex + 1,
                    polygonAudit.polygons.size(), p.vertexCount, p.recordedRPLI,
                    aetherclime::EdgeEvidenceStatusName(polygonAudit.status));
            }
        }
        // This is purely nearest-segment geometry. RPLI is an engine record
        // value named distanceInsideAtMax, not a verified blend radius.
        const auto& edge = region.edgeEvidence;
        if (edge.measured) {
            SKSE::log::info(
                "Weather region: edge-audit | region=0x{:08X} | status={} "
                "| nearestRecordedSegment={:.1f} raw-units | nearestPolygon={} "
                "| polygonRPLI={} raw-units | proximityVsRPLI={} "
                "| semantics=geometry-only | engineFalloff=unresolved",
                region.regionFormId, aetherclime::EdgeEvidenceStatusName(edge.status),
                edge.nearestSegmentDistance, edge.nearestPolygonIndex + 1,
                edge.nearestPolygonRPLI, aetherclime::RPLIProximityName(edge.proximity));
        } else {
            SKSE::log::info(
                "Weather region: edge-audit | region=0x{:08X} | status={} "
                "| proximityVsRPLI=not-comparable | engineFalloff=unresolved",
                region.regionFormId, aetherclime::EdgeEvidenceStatusName(edge.status));
        }
        // Show the full discovered list (rather than an arbitrary first N).
        // We split into multiple log lines to keep each line inspectable.
        std::ostringstream chunk;
        std::size_t inChunk = 0;
        std::size_t chunkIndex = 0;
        for (const auto& choice : region.choices) {
            if (inChunk == 8) {
                SKSE::log::info("Weather choices: region=0x{:08X} | chunk={} | {}",
                    region.regionFormId, ++chunkIndex, chunk.str());
                chunk.str(std::string{});
                chunk.clear();
                inChunk = 0;
            }
            if (inChunk++) chunk << ',';
            chunk << "0x" << std::uppercase << std::hex << std::setw(8)
                  << std::setfill('0') << choice.weatherFormId << std::dec
                  << ':' << choice.rawWeight;
            if (choice.chanceGlobalFormId != 0) {
                chunk << "@G0x" << std::hex << std::setw(8)
                      << std::setfill('0') << choice.chanceGlobalFormId << std::dec;
            }
        }
        if (inChunk) {
            SKSE::log::info("Weather choices: region=0x{:08X} | chunk={} | {}",
                region.regionFormId, ++chunkIndex, chunk.str());
        }
    }
    if (discovered.truncated) {
        SKSE::log::warn("Weather region diagnostics reached a defensive 256-entry safety cap; output incomplete.");
    }
}

void LogGeographyDiagnostics() {
    const auto* player = RE::PlayerCharacter::GetSingleton();
    auto* cell = player ? player->GetParentCell() : nullptr;
    const auto* worldspace = player ? player->GetWorldspace() : nullptr;
    const auto* cellLocation = cell ? cell->GetLocation() : nullptr;
    const char* cellLocName = cellLocation ? cellLocation->GetName() : nullptr;

    aetherclime::RawPosition raw{};
    if (player) {
        const auto pos = player->GetPosition();
        raw = {pos.x, pos.y, pos.z};
    }
    const auto position = aetherclime::CheckWorldPosition(
        cell != nullptr, cell && cell->IsInteriorCell(), worldspace != nullptr, raw);
    const auto candidates = RegionCandidates(cell);

    // We are gathering ground truth for a future region resolver and altitude
    // model. Raw coordinates are NOT meters and should never be fed directly
    // into Climate::Sample::elevationAboveReferenceM.
    if (position.worldUnits) {
        const auto& xyz = *position.worldUnits;
        SKSE::log::info(
            "Geography diagnostics: positionStatus={} | worldXYZ=({:.1f},{:.1f},{:.1f}) raw-game-units "
            "| cellLocation='{}' (0x{:08X}) | regionCandidates=[{}] | regionBasis=climate-location-hierarchy-inference | elevationBasis=see-elevation-audit",
            aetherclime::PositionStatusName(position.status), xyz.x, xyz.y, xyz.z,
            cellLocName && *cellLocName ? cellLocName : "(unnamed-or-unavailable)",
            cellLocation ? cellLocation->GetFormID() : 0, candidates);
    } else {
        SKSE::log::info(
            "Geography diagnostics: positionStatus={} | worldXYZ=(not-applicable) "
            "| cellLocation='{}' (0x{:08X}) | regionCandidates=[{}] | regionBasis=climate-location-hierarchy-inference | elevationBasis=see-elevation-audit",
            aetherclime::PositionStatusName(position.status),
            cellLocName && *cellLocName ? cellLocName : "(unnamed-or-unavailable)",
            cellLocation ? cellLocation->GetFormID() : 0, candidates);
    }
}

// Alpha-49: One-shot indoor temperature estimates, including generic shelter.
// Deliberately not wired into Aetherclime_GetTemperatureC: that API remains
// regional OUTDOOR air temperature even while the player is in an inn.
// No reference scans, heat-source searches, or player Update hooks.
void LogIndoorClimateContext(std::string_view trigger) {
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto* cell = player ? player->GetParentCell() : nullptr;
    const auto* location = player ? player->GetCurrentLocation() : nullptr;
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto& climate = GetClimate();
    const auto& config = climate.Settings();
    const auto effective = aetherclime::ResolveClimateRegion(config, evidence.Context());
    const auto indoorContext = aetherclime::ResolveInteriorClimateContext(
        config, evidence.hasCell, evidence.interior,
        location && location->GetName() ? location->GetName() : "",
        cell && cell->GetName() ? cell->GetName() : "",
        effective.regionKey, effective.matched());
    auto sample = Snapshot(); // Applies only previously verified exterior Z.
    if (indoorContext.eligible && indoorContext.usedRuleRegion) {
        sample.region = indoorContext.regionKey;
    }
    const auto retainedWeather = ExteriorWeatherForSample(sample, evidence);
    if(evidence.interior && !retainedWeather.usable)
        sample.weather=aetherclime::Weather::Unknown;
    const auto outdoor = climate.Calculate(sample);
    const char* cellName = cell && cell->GetName() ? cell->GetName() : "";
    const char* locationName = location && location->GetName() ? location->GetName() : "";
    const auto building = aetherclime::ClassifyBuilding(evidence.hasCell,evidence.interior,
        cellName,locationName,indoorContext.eligible);
    const auto indoor = aetherclime::AssessIndoorAmbient(config, indoorContext,
        outdoor.airC, building.kind);
    SKSE::log::info(
        "Indoor climate audit: trigger={} | status={} | enabled={} | eligible={} | "
        "ambientAvailable={} | cellType={} | cellName='{}' | locationName='{}' | "
        "outdoorProfile='{}' | profileSource={} | innRule='{}' | usedRuleRegion={} | "
        "outdoorAirC={:.2f} | projectedIndoorC={:.2f} | actualIndoorC={:.2f} | "
        "projectedIndoorMinusOutdoorC={:.2f} | clamped={} | "
        "outdoorWeatherSource={} | outdoorWeatherCacheStatus={} | outdoorMetricIndependent=true | "
        "estimateBasis={} | ambientBasis=inferred-not-measured-fire | "
        "exteriorPositionUsed=false | perFramePolling=false",
        trigger, aetherclime::InteriorClimateStatusName(indoor.context.status),
        indoor.enabled, indoor.context.eligible, indoor.available,
        evidence.hasCell ? (evidence.interior ? "interior" : "exterior") : "unknown",
        cell && cell->GetName() ? cell->GetName() : "(none)",
        location && location->GetName() ? location->GetName() : "(none)",
        sample.region, aetherclime::ClimateRegionSourceName(effective.source),
        indoor.context.matchedRule.empty() ? "(none)" : indoor.context.matchedRule,
        indoor.context.usedRuleRegion, outdoor.airC,
        indoor.projectedAmbientC, indoor.actualAmbientC,
        indoor.projectedDeltaC, indoor.clamped,
        OutdoorWeatherSource(retainedWeather, evidence.interior),
        aetherclime::ExteriorWeatherStatusName(retainedWeather.status),
        aetherclime::IndoorEstimateBasisName(indoor.basis));
}

// Alpha-23: classification evidence for future ambient models. This does
// NOT activate heating for any building beyond explicit Alpha-21 inn rules.
void LogBuildingClassification(std::string_view trigger) {
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto* cell = player ? player->GetParentCell() : nullptr;
    const auto* location = player ? player->GetCurrentLocation() : nullptr;
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto& config = GetClimate().Settings();
    const auto resolved = aetherclime::ResolveClimateRegion(config, evidence.Context());
    const char* cellName = cell && cell->GetName() ? cell->GetName() : "";
    const char* locationName = location && location->GetName() ? location->GetName() : "";
    const auto inn = aetherclime::ResolveInteriorClimateContext(
        config, evidence.hasCell, evidence.interior, locationName, cellName,
        resolved.regionKey, resolved.matched());
    const auto building = aetherclime::ClassifyBuilding(
        evidence.hasCell, evidence.interior, cellName, locationName, inn.eligible);
    SKSE::log::info(
        "Building classification: trigger={} | kind={} | evidence={} | "
        "cell='{}' | location='{}' | confidence={} | knownHeatedInnRule={} | "
        "heatingAssumed=false | temperatureApplied=false | "
        "ambientModel=explicit-inn-or-typed-generic | noReferenceScan=true | perFramePolling=false",
        trigger, aetherclime::BuildingKindName(building.kind),
        aetherclime::BuildingEvidenceName(building.evidence),
        cellName, locationName,
        aetherclime::BuildingEvidenceConfidence(building.evidence), building.knownHeatedInnRule);
}

// Alpha-25: one-shot, ownership-safe data assembled from existing read-only
// climate and location observations. Do not cache RE pointers; generic
// ambient is confined to V3 and cannot masquerade as verified V1 ambient.
// Called only from Skyrim's main/game thread.
std::optional<aetherclime::EnvironmentSnapshot> ReadCurrentEnvironmentSnapshot(bool includeDetails = false) {
    if (!g_climate) return std::nullopt;
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto* cell = player ? player->GetParentCell() : nullptr;
    if (!cell) return std::nullopt;
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto& climate = GetClimate();
    const auto& config = climate.Settings();
    const auto directRegion = aetherclime::ResolveClimateRegion(config, evidence.Context());
    const auto* location = player->GetCurrentLocation();
    const char* locationName = location && location->GetName() ? location->GetName() : "";
    const char* cellName = cell->GetName() ? cell->GetName() : "";
    const auto indoorContext = aetherclime::ResolveInteriorClimateContext(
        config, evidence.hasCell, evidence.interior, locationName, cellName,
        directRegion.regionKey, directRegion.matched());
    auto sample = Snapshot();
    const auto region = AssessRegionalContinuity(evidence, sample.date).resolution;
    const auto outdoor = climate.Calculate(sample);
    const auto building = aetherclime::ClassifyBuilding(
        evidence.hasCell, evidence.interior, cellName, locationName, indoorContext.eligible);
    // Interior Sky is not an exterior observation. For unknown interior
    // weather, use the region's seasonal/daytime baseline rather than Sky.
    const auto retainedWeather = ExteriorWeatherForSample(sample,evidence);
    auto indoorReferenceSample=sample;
    if(evidence.interior && !retainedWeather.usable)
        indoorReferenceSample.weather=aetherclime::Weather::Unknown;
    const auto indoorOutdoor=climate.Calculate(indoorReferenceSample);
    const auto ambient=aetherclime::AssessIndoorAmbient(
        config, indoorContext, indoorOutdoor.airC, building.kind);
    const auto found = config.regions.find(sample.region);
    const auto regionName = found != config.regions.end() ? found->second.name : sample.region;
    const bool trusted = directRegion.matched() ||
        (evidence.interior && indoorContext.eligible && indoorContext.usedRuleRegion);
    // Region 'temperate' is a fallback CLIMATE profile, not a Skyrim place.
    // A player-facing local label must not claim there is a 'Temperate Skyrim'.
    const std::string fallbackDisplay = trusted ? regionName :
        (region.provisional() ? std::string("Near ") + regionName : "Skyrim");
    const auto identity = aetherclime::ResolveLocalIdentity({
        evidence.hasCell, evidence.interior, cell->GetFormID(), cellName,
        evidence.Context().locationChain, fallbackDisplay, sample.region});
    const auto* sky = RE::Sky::GetSingleton();
    const auto* currentWeather = sky ? sky->currentWeather : nullptr;
    const auto& retained = retainedWeather;
    aetherclime::OutdoorWeatherProvenance weatherSource =
        aetherclime::OutdoorWeatherProvenance::Unavailable;
    std::uint32_t weatherId = 0;
    if (!evidence.interior && currentWeather && player->GetWorldspace() && trusted) {
        weatherSource = aetherclime::OutdoorWeatherProvenance::DirectExteriorSky;
        weatherId = currentWeather->GetFormID();
    } else if (evidence.interior && retained.usable && retained.observation) {
        weatherSource = aetherclime::OutdoorWeatherProvenance::CachedVerifiedExterior;
        weatherId = retained.observation->weatherFormId;
    } else if (evidence.interior) {
        weatherSource = aetherclime::OutdoorWeatherProvenance::InteriorSkyUnverified;
    }
    aetherclime::EnvironmentSnapshotInput input;
    input.hasCell = evidence.hasCell;
    input.interior = evidence.interior;
    input.trustedRegion = trusted;
    input.provisionalRegion = region.provisional();
    input.regionKey = sample.region;
    input.regionName = regionName;
    input.regionSource = std::string(aetherclime::ClimateRegionSourceName(region.source));
    input.local = identity;
    input.building = building;
    input.date = sample.date;
    input.weather = sample.weather;
    input.weatherProvenance = weatherSource;
    input.weatherFormId = weatherId;
    input.outdoorAirC = outdoor.airC;
    input.outdoorFeelsLikeC = outdoor.feelsLikeC;
    // Preserve V1/V2 semantics: only the established, trusted, named inn
    // rules may publish the legacy localAmbientC. Other estimates are V3 only.
    if (ambient.available && indoorContext.eligible && trusted)
        input.modeledIndoorAmbientC = ambient.actualAmbientC;
    if (evidence.interior && ambient.available) {
        input.estimatedIndoorAmbientC=ambient.actualAmbientC;
        input.indoorEstimateBasis=ambient.basis;
    }
    // A biome query is only needed when the region lacks direct evidence;
    // Debug and V2/V3 queries retain their independent detail attachments.
    std::optional<double> estimateBiome;
    bool estimateBiomeNearby=false;
    std::optional<aetherclime::NativeAtlasResult> sampledAtlas;
    if (!evidence.interior && !trusted && config.nativeRegionShadowEnabled &&
        config.biomeClimate.enabled && !REL::Module::IsVR() && evidence.exteriorPosition) {
        sampledAtlas=g_nativeAtlas.Query(evidence.exteriorPosition,true);
        const auto preview=aetherclime::CompareBiomeClimate(config,sample,outdoor,*sampledAtlas,evidence.exteriorPosition);
        if(preview.available) {
            estimateBiome=preview.previewAirC;
            estimateBiomeNearby=(aetherclime::ResolveBiomeIdentity(config,*sampledAtlas).evidence==
                aetherclime::BiomeEvidence::Nearby);
        }
    }
    const auto estimate=aetherclime::EstimateOutdoorClimate(config,sample,outdoor,
        trusted,region.provisional(),estimateBiome,estimateBiomeNearby,evidence.exteriorPosition);
    input.estimatedOutdoorAirC=estimate.airC;
    input.outdoorEstimateBasis=estimate.basis;
    input.outdoorReferenceLabel=estimate.referenceLabel;
    input.outdoorReferenceDistanceRaw=estimate.referenceDistanceRawUnits;
    input.outdoorReferenceInfluence=estimate.referenceInfluence;
    input.outdoorReferenceCount=estimate.contributingReferences;
    auto result = aetherclime::AssembleEnvironmentSnapshot(input);
    if (!result.available) return std::nullopt;
    if (includeDetails) {
        aetherclime::BiomeIdentity identity;
        std::optional<double> previewAir;
        if (evidence.interior) identity.status="interior-no-exterior-biome";
        else if (!config.nativeRegionShadowEnabled) identity.status="atlas-disabled";
        else if (REL::Module::IsVR()) identity.status="vr-unverified";
        else if (!evidence.exteriorPosition) identity.status="no-exterior-position";
        else {
            if(!sampledAtlas) sampledAtlas=g_nativeAtlas.Query(evidence.exteriorPosition,true);
            identity=aetherclime::ResolveBiomeIdentity(config,*sampledAtlas);
            if (config.biomeClimate.enabled) {
                const auto preview=aetherclime::CompareBiomeClimate(config,sample,outdoor,*sampledAtlas,evidence.exteriorPosition);
                if (preview.available) previewAir=preview.previewAirC;
            }
        }
        aetherclime::AttachEnvironmentDetails(result,outdoor.season,identity,previewAir);
    }
    return result;
}

// Alpha-48: game-thread presentation only. The HUD takes an owned snapshot
// at eligible settled events, independently of the diagnostic print budgets.
// No engine writes, added update hook, Papyrus scheduler or delayed message queue.
bool HudEnabled() noexcept {
    return g_climate && !REL::Module::IsVR() && g_climate->Settings().hud.mode!=aetherclime::HudMode::Off && !g_hudFailed;
}
bool ClimateEventSamplingEnabled() noexcept {
    return g_climate && (g_climate->Settings().eventClimateEnabled || HudEnabled());
}
void ObserveHud(std::string_view trigger) {
    if(!g_liveSessionActive||!HudEnabled()) return;
    try {
        auto* ui=RE::UI::GetSingleton();
        const bool ready=ui && !ui->GameIsPaused() && !ui->IsMenuOpen("Loading Menu") &&
            !ui->IsMenuOpen("Main Menu") && !ui->IsMenuOpen("Dialogue Menu") && !ui->IsMenuOpen("Console") &&
            !ui->IsItemMenuOpen() && !ui->IsModalMenuOpen() && ui->IsMenuOpen("HUD Menu");
        const auto now=std::chrono::duration<double>(std::chrono::steady_clock::now()-g_monitorEpoch).count();
        const auto& settings=g_climate->Settings().hud;
        if(!g_hud.BeginSample(settings,now,ready))return;
        // Only Debug needs the heavier biome/preview fields. Minimal and
        // Detailed attach the calendar season without an extra atlas query.
        auto owned=ReadCurrentEnvironmentSnapshot(settings.mode==aetherclime::HudMode::Debug);
        if(owned && !owned->season) {
            const auto season=GetClimate().GetSeason(owned->date);
            if(aetherclime::ValidSeasonState(season))owned->season=season;
        }
        const aetherclime::EnvironmentSnapshot empty{};
        if(auto notice=g_hud.Observe(settings,owned?*owned:empty)) {
            RE::DebugNotification(notice->text.c_str(),nullptr,true);
            SKSE::log::info("HUD notification: trigger={} | mode={} | reason={} | elapsedSeconds={:.2f} "
                "| text='{}' | nativeQueueRequested=true | sampledEventsOnly=true | noWeatherWrites=true",
                trigger,aetherclime::HudModeName(settings.mode),aetherclime::HudReasonName(notice->reason),now,notice->text);
        }
    } catch(const std::exception& e) {
        g_hudFailed=true;
        SKSE::log::warn("HUD notification failed safely: {}. HUD muted for this loaded session.",e.what());
    } catch(...) {
        g_hudFailed=true;
        SKSE::log::warn("HUD notification failed safely: unknown exception. HUD muted for this loaded session.");
    }
}
void LogHudSummary() {
    if(!g_climate)return;
    const auto& h=g_hud.Statistics();
    SKSE::log::info("HUD coverage: trigger=save | mode={} | eligibleSamples={} | sent={} | unavailable={} "
        "| unchanged={} | baselineOnly={} | blockedUi={} | blockedGrace={} | blockedCooldown={} "
        "| blockedSampleInterval={} | invalidClock={} | sessionMuted={} | sampledEventsOnly=true",
        aetherclime::HudModeName(g_climate->Settings().hud.mode),h.eligibleSamples,h.sent,h.unavailable,
        h.unchanged,h.baselineOnly,h.blockedUi,h.blockedGrace,h.blockedCooldown,h.blockedSampleInterval,h.invalidClock,g_hudFailed);
}

// Alpha-37: copy a bounded global REGN weather-footprint census ONCE at
// data loaded. It does not consult CELL/XCLR or Sky::region to select climate.
void BuildNativeRegionAtlas(const aetherclime::Config& config) {
    using Atlas=aetherclime::NativeRegionAtlas;
    g_nativeAtlas.Build({}, false);
    if(!config.nativeRegionShadowEnabled || REL::Module::IsVR()) {
        SKSE::log::info("Native atlas census: status={} | shadowOnly=true",config.nativeRegionShadowEnabled?"vr-unverified":"disabled");
        return;
    }
    auto* handler=RE::TESDataHandler::GetSingleton();
    if(!handler) { SKSE::log::warn("Native atlas census: status=no-data-handler"); return; }
    std::unordered_map<std::uint32_t,std::string> mappings;
    std::size_t unresolved=0; bool mappingConflict=false;
    for(const auto& m:config.nativeRegionProfiles) {
        auto* region=handler->LookupForm<RE::TESRegion>(m.localFormId,m.plugin);
        if(!region) {++unresolved; SKSE::log::warn("Native atlas mapping missing: plugin='{}' | localFormId=0x{:06X} | profile='{}'",m.plugin,m.localFormId,m.regionKey); continue;}
        auto [it,inserted]=mappings.emplace(region->GetFormID(),m.regionKey);
        if(!inserted && it->second!=m.regionKey) mappingConflict=true;
    }
    std::unordered_map<std::uint32_t,const aetherclime::NativeBiomeDefinition*> biomeMappings;
    for(const auto& entry:aetherclime::kVanillaBiomeRegistry) {
        if(auto* region=handler->LookupForm<RE::TESRegion>(entry.localFormId,"Skyrim.esm"))
            biomeMappings.emplace(region->GetFormID(),&entry);
        else SKSE::log::warn("Native biome registry missing: localFormId=0x{:08X} | editorId={}",entry.localFormId,entry.editorId);
    }
    std::vector<aetherclime::NativeRegionRecord> copied;
    std::unordered_set<std::uint32_t> visited;
    std::size_t seen=0, polygons=0, vertices=0;
    bool complete=!mappingConflict;
    auto copy=[&](RE::TESRegion* region) {
        if(!complete) return;
        if(++seen>4096) {complete=false; return;}
        if(!region || !visited.insert(region->GetFormID()).second) return;
        if(region->IsDeleted() || region->IsIgnored()) return;
        const auto mapped=mappings.find(region->GetFormID());
        bool weather=false; std::size_t dataCount=0;
        if(region->dataList) for(auto* data:region->dataList->regionDataList) {
            if(++dataCount>128) {complete=false;return;}
            if(data && data->GetType()==RE::TESRegionData::Type::kWeather) weather=true;
        }
        const auto biome=biomeMappings.find(region->GetFormID());
        if(!weather && mapped==mappings.end() && biome==biomeMappings.end()) return;
        if(copied.size()>=Atlas::maxRecords) {complete=false;return;}
        aetherclime::NativeRegionRecord record;
        record.formId=region->GetFormID();
        record.worldspace=region->worldSpace?region->worldSpace->GetFormID():0;
        if(mapped!=mappings.end()) record.profile=mapped->second;
        if(biome!=biomeMappings.end()) {
            record.editorId=biome->second->editorId;
            record.biome=biome->second->biome;
            record.biomeRole=biome->second->role;
        }
        if(region->pointLists) for(auto* list:*region->pointLists) {
            if(++polygons>Atlas::maxPolygons) {complete=false;return;}
            if(!list) {record.incomplete=true;continue;}
            aetherclime::RegionPolygon p;
            std::size_t pointCount=0;
            for(auto* point:*list) {
                if(++pointCount>Atlas::maxPolygonVertices || ++vertices>Atlas::maxVertices) {complete=false;return;}
                if(!point) {record.incomplete=true;continue;}
                p.push_back({point->point.x,point->point.y});
            }
            record.polygons.push_back(std::move(p));
        }
        copied.push_back(std::move(record));
    };
    // Some runtime data handlers may keep REGNs in the dedicated list rather
    // than the generic form array. Visit both, de-duplicating runtime FormIDs.
    const auto& forms=handler->GetFormArray<RE::TESRegion>();
    for(auto* r:forms) {copy(r); if(!complete) break;}
    if(complete && handler->regionList) for(auto* r:*handler->regionList) {copy(r);if(!complete) break;}
    // A configured record that failed to appear in the census is not silently
    // ignored: an unseen competing mapped region could change the answer.
    for(const auto& [id,profile]:mappings) {
        if(std::none_of(copied.begin(),copied.end(),[&](const auto& r){return r.formId==id;})) ++unresolved;
    }
    // Only publish the inventory when the enumeration was not truncated.
    // Runtime IDs are for xEdit investigation, never long-term config keys.
    if(complete) {
        for(std::size_t i=0;i<std::min<std::size_t>(copied.size(),128);++i) {
            const auto& r=copied[i];
            SKSE::log::info("Native atlas record: form=0x{:08X} | worldspace=0x{:08X} | mappedProfile='{}' | polygons={} | inputIncomplete={} | runtimeIdsForReviewOnly=true",r.formId,r.worldspace,r.profile,r.polygons.size(),r.incomplete);
        }
    }
    g_nativeAtlas.Build(std::move(copied),complete && unresolved==0);
    const auto& stats=g_nativeAtlas.Stats();
    SKSE::log::info("Native atlas census: status={} | records={} | mappedRecords={} | polygons={} | vertices={} | incompleteRecords={} | visitedEntries={} | unresolvedMappings={} | mappingConflict={} | shadowOnly=true | noWeatherSelection=true",
        stats.complete?"ready":"unavailable",stats.records,stats.mapped,stats.polygons,stats.vertices,stats.incomplete,seen,unresolved,mappingConflict);
    SKSE::log::info("Native biome census: registryResolved={} | broadOrVariantRecords={} | quarantinedWorldspace={} | shadowOnly=true",biomeMappings.size(),stats.biomeRecords,stats.quarantined);
    if(config.nativeRegionGeometryExportEnabled && stats.complete) {
        try {
            if(auto dir=SKSE::log::log_directory()) {
                const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                const auto path=*dir/("Aetherclime-native-geometry-"+std::to_string(stamp)+".csv");
                std::ofstream file(path,std::ios::out|std::ios::binary);
                if(!file || !g_nativeAtlas.ExportGeometry(file)) SKSE::log::warn("Native geometry export failed: {}",path.string());
                else SKSE::log::info("Native geometry export: path='{}' | schema=1 | validatedCache=true | readOnlyGame=true",path.string());
            } else SKSE::log::warn("Native geometry export skipped: no-log-directory");
        } catch(const std::exception& e) { SKSE::log::warn("Native geometry export failed safely: {}",e.what()); }
    }
}

void ObserveNativeAtlasShadow(std::string_view trigger,const aetherclime::Config& config,
    const PlayerClimateEvidence& evidence,const aetherclime::ClimateRegionResolution& direct,
    const aetherclime::ClimateRegionResolution& effective,const aetherclime::Sample& sample) {
    if(!config.nativeRegionShadowEnabled) return;
    const auto result=g_nativeAtlas.Query(evidence.exteriorPosition,config.biomeClimate.enabled);
    if(config.biomeClimate.enabled) {
        const auto preview=aetherclime::CompareBiomeClimate(config,sample,GetClimate().Calculate(sample),result,evidence.exteriorPosition);
        ++g_biomeSamples;
        if(preview.available) {
            ++g_biomeAvailable;
            if(!preview.biomeB.empty()) ++g_biomeOverlap;
            if(preview.gapDistanceRaw>=0) ++g_biomeGap;
        }
        const bool manual=trigger=="save"||trigger=="load";
        if((manual || (preview.available && (std::abs(preview.deltaC)>=4 || !preview.biomeB.empty() || preview.gapDistanceRaw>=0))) && (manual?g_biomePreviewLines<40:g_biomePreviewLines<32)) {
            ++g_biomePreviewLines;
            SKSE::log::info("Biome climate preview: trigger={} | status={} | available={} | geographicProfile='{}' | geographySource={} | biomeA='{}' | biomeB='{}' | weightA={:.3f} | weightB={:.3f} | gapDistanceRaw={:.1f} | gapRunnerUpRaw={:.1f} | legacyAirC={:.2f} | previewAirC={:.2f} | deltaC={:+.2f} | previewBaselineC={:.2f} | previewDiurnalC={:.2f} | gradientApplied={} | gradientOffsetC={:+.2f} | northFraction={:.3f} | legacyWeatherAnomalyElevationRetained=true | shadowOnly=true | publicSnapshotModified=false | noWeatherTrust=true",
                trigger,preview.status,preview.available,preview.geographicProfile,aetherclime::ClimateRegionSourceName(effective.source),preview.biomeA,preview.biomeB,preview.weightA,preview.weightB,preview.gapDistanceRaw,preview.gapRunnerUpRaw,preview.legacyAirC,preview.previewAirC,preview.deltaC,preview.baselineC,preview.diurnalC,preview.gradientApplied,preview.gradientOffsetC,preview.northFraction);
        }
        if(trigger=="save" && g_biomePreviewSummaries<8) {
            ++g_biomePreviewSummaries;
            SKSE::log::info("Biome climate coverage: samples={} | available={} | overlapPreviews={} | nearbyPreviews={} | summaryIndex={} | sampledEventsOnly=true | shadowOnly=true",g_biomeSamples,g_biomeAvailable,g_biomeOverlap,g_biomeGap,g_biomePreviewSummaries);
        }
    }
    auto decision=aetherclime::CompareNativeShadow(direct,effective,result);
    if(!config.geographicFallbackEnabled || !evidence.exteriorPosition)
        decision={effective.regionKey,"geographic-gate-or-interior-preserved",false};
    g_nativeShadowCoverage.Observe(result,decision,effective);
    const bool manual=trigger=="save"||trigger=="load";
    // Up to 32 event-detail lines plus eight save summaries. Always retain
    // observation counts; routine same-profile events cannot consume slots.
    const bool interesting=decision.wouldReplaceFallback || (!result.profile.empty() && result.profile!=effective.regionKey) ||
        (!effective.matched() && !effective.provisional() && result.unmappedContaining>0) ||
        result.status==aetherclime::NativeAtlasStatus::ConflictingProfiles || result.biomeStatus==aetherclime::NativeBiomeStatus::Conflict;
    if((manual||interesting)&&g_nativeShadowLines<32) {
        ++g_nativeShadowLines;
        std::ostringstream ids;
        for(std::size_t i=0;i<std::min(result.unmappedContaining,result.unmappedIds.size());++i) {
            if(i) ids<<',';
            ids<<"0x"<<std::uppercase<<std::hex<<std::setw(8)<<std::setfill('0')<<result.unmappedIds[i];
        }
        SKSE::log::info("Native biome shadow: trigger={} | status={} | biome='{}' | broadContaining={} | localOverrides={} | worldspace=0x{:08X} | rawXY=({:.1f},{:.1f}) | shadowOnly=true | climateUnchanged=true",
            trigger,aetherclime::NativeBiomeStatusName(result.biomeStatus),result.biome,result.biomeContaining,result.localOverrideContaining,
            evidence.exteriorPosition?evidence.exteriorPosition->worldspaceFormId:0,evidence.exteriorPosition?evidence.exteriorPosition->xRaw:0.0,evidence.exteriorPosition?evidence.exteriorPosition->yRaw:0.0);
        SKSE::log::info("Native atlas shadow: trigger={} | status={} | effectiveProfile='{}' | effectiveSource={} | polygonProfile='{}' | proposedProfile='{}' | decision={} | fallbackWouldReplace={} | mappedContaining={} | unmappedContaining={} | unmappedIds='{}' | incompleteUnmapped={} | boundaryUnmapped={} | worldspace=0x{:08X} | rawXY=({:.1f},{:.1f}) | shadowOnly=true | readOnly=true | noWeatherTrust=true | noChainedAnchor=true | publicSnapshotModified=false",
            trigger,aetherclime::NativeAtlasStatusName(result.status),effective.regionKey,aetherclime::ClimateRegionSourceName(effective.source),result.profile,decision.profile,decision.reason,decision.wouldReplaceFallback,result.mappedContaining,result.unmappedContaining,ids.str(),result.incompleteUnmapped,result.boundaryUnmapped,
            evidence.exteriorPosition?evidence.exteriorPosition->worldspaceFormId:0,evidence.exteriorPosition?evidence.exteriorPosition->xRaw:0.0,evidence.exteriorPosition?evidence.exteriorPosition->yRaw:0.0);
    }
    if(trigger=="save" && g_nativeShadowSummaries<8) {
        ++g_nativeShadowSummaries;
        const auto& c=g_nativeShadowCoverage;
        SKSE::log::info("Native atlas coverage: trigger=save | samples={} | noPosition={} | unavailable={} | unique={} | unmapped={} | conflicts={} | boundary={} | incomplete={} | noCoverage={} | fallbackWouldReplace={} | sameProfile={} | differentProfile={} | directPreserved={} | summaryIndex={} | eventSamplesNotArea=true | shadowOnly=true | readOnly=true | perFramePolling=false",
            c.samples,c.noPosition,c.unavailable,c.unique,c.unmapped,c.conflicts,c.boundary,c.incomplete,c.noCoverage,c.fallbackWouldReplace,c.sameProfile,c.differentProfile,c.directPreserved,g_nativeShadowSummaries);
    }
}

// Alpha-30: compare adjacent *event-sampled* exterior model outputs. We log
// discontinuities without changing the climate calculation, snapshot flags,
// trust levels, or weather. A manual save can also contribute one observation.
void LogTemperatureHandoff(std::string_view trigger) {
    if (!g_climate) return;
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto sample = Snapshot();
    // Keep the direct and effective resolutions separately for Alpha-35
    // fallback-gate diagnostics. This performs the same one resolver call and
    // continuity assessment as AssessRegionalContinuity, with no model writes.
    const auto& config = GetClimate().Settings();
    const auto context = evidence.Context();
    const auto direct = aetherclime::ResolveClimateRegion(config, context);
    const auto continuity = g_regionalContinuity.Assess(
        direct, context, sample.date, config.geographicContinuityEnabled);
    const auto& effective = continuity.resolution;
    try { ObserveNativeAtlasShadow(trigger,config,evidence,direct,effective,sample); }
    catch(const std::exception& e) { SKSE::log::warn("Native atlas shadow probe skipped: {}",e.what()); }
    auto fallbackFinding = aetherclime::ClassifyGeographicFallback(
        effective.source, direct.source, continuity.status, config.geographicFallbackEnabled);
    using ContinuityState = aetherclime::ContinuityStatus;
    if (continuity.status == ContinuityState::TooFar || continuity.status == ContinuityState::Provisional ||
        continuity.status == ContinuityState::Expired) {
        // Survey distance is never a fixed verified-origin distance.
        if (std::isfinite(continuity.distanceRawUnits) && continuity.distanceRawUnits >= 0)
            fallbackFinding.originDistanceRawUnits = continuity.distanceRawUnits;
        if (std::isfinite(continuity.ageGameHours) && continuity.ageGameHours >= 0)
            fallbackFinding.originAgeGameHours = continuity.ageGameHours;
    }
    const auto reading = GetClimate().Calculate(sample);
    aetherclime::TemperatureHandoffSample observation;
    observation.exterior = evidence.hasCell && !evidence.interior;
    observation.position = evidence.exteriorPosition;
    observation.date = sample.date;
    observation.regionKey = sample.region;
    observation.source = effective.source;
    observation.weather = sample.weather;
    observation.outdoorAirC = reading.airC;
    const auto assessment = g_temperatureHandoffAudit.Observe(observation);
    // Track EVERY observation before the 48-line output filter. This is an
    // event-sampled audit, not a spatial coverage fraction or per-frame scan.
    g_temperatureHandoffCoverage.Record(observation, assessment);
    g_geographicGapReport.Observe(observation, assessment,
                                  g_temperatureHandoffCoverage.Snapshot().observations,
                                  fallbackFinding);
    if (const auto& candidate = g_geographicGapReport.LastCandidate(); candidate)
        g_atlasProposalReview.Observe(config, *candidate);
    if (assessment.status != aetherclime::TemperatureHandoffStatus::NoExteriorEvidence)
        g_atlasProposalReview.RecordAtlasUse(effective);
    const auto preview = g_temperatureContinuityPreview.Observe(
        observation, assessment, GetClimate().Settings().temperatureContinuity);
    const bool manual = trigger == "save" || trigger == "load";
    const bool interesting = assessment.status == aetherclime::TemperatureHandoffStatus::Baseline ||
        assessment.sourceChanged || assessment.profileChanged;
    const bool previewInteresting = preview.status == aetherclime::TemperatureContinuityStatus::Started ||
        preview.status == aetherclime::TemperatureContinuityStatus::Expired ||
        preview.status == aetherclime::TemperatureContinuityStatus::Fading ||
        preview.status == aetherclime::TemperatureContinuityStatus::IneligibleHandoff;
    // A routine named-location <-> anchor flicker cannot use all 48 lines;
    // preserve at least 30 slots unless high-priority steps use them first.
    const bool priority = manual || assessment.status == aetherclime::TemperatureHandoffStatus::Baseline ||
        assessment.profileChanged || preview.status == aetherclime::TemperatureContinuityStatus::Started ||
        preview.status == aetherclime::TemperatureContinuityStatus::Expired;
    if (g_temperatureHandoffReportBudget.Allow(manual || interesting || previewInteresting, priority)) {
        SKSE::log::info(
            "Temperature handoff audit: trigger={} | sampleIndex={} | status={} | stepClass={} "
            "| previousRegion='{}' | currentRegion='{}' | previousSource={} | currentSource={} "
            "| sourceChanged={} | profileChanged={} | comparable={} "
            "| distanceRawUnits={:.1f} | ageGameHours={:.3f} | airDeltaC={:+.2f} "
            "| previousReadingAvailable={} | previousOutdoorAirC={:.2f} "
            "| significant={} | thresholdC={:.1f} | currentOutdoorAirC={:.2f} "
            "| shadowStatus={} | shadowOutdoorAirC={:.2f} | shadowOffsetC={:+.2f} "
            "| shadowActive={} | shadowPathRawUnits={:.1f} | shadowAgeGameHours={:.3f} "
            "| previousEvidenceTier={} | currentEvidenceTier={} | evidenceChange={} "
            "| shadowReason={} | fadeDistanceRemaining={:.3f} | fadeTimeRemaining={:.3f} "
            "| compensationCapped={} "
            "| shadowOnly=true | publicSnapshotModified=false "
            "| readOnly=true | modelTemperatureModified=false | confidenceUnchanged=true "
            "| sameWeatherRequired=true | perFramePolling=false",
            trigger, g_temperatureHandoffCoverage.Snapshot().observations,
            aetherclime::TemperatureHandoffStatusName(assessment.status),
            aetherclime::TemperatureHandoffStepClassName(aetherclime::ClassifyTemperatureHandoffStep(
                assessment, effective.source)),
            assessment.previousRegion, sample.region,
            aetherclime::ClimateRegionSourceName(assessment.previousSource),
            aetherclime::ClimateRegionSourceName(effective.source),
            assessment.sourceChanged, assessment.profileChanged, assessment.comparable,
            assessment.distanceRawUnits, assessment.ageGameHours, assessment.airDeltaC,
            assessment.previousReadingAvailable, assessment.previousOutdoorAirC,
            assessment.significant, aetherclime::TemperatureHandoffAudit::kSignificantStepC,
            reading.airC,
            aetherclime::TemperatureContinuityStatusName(preview.status),
            preview.previewOutdoorAirC, preview.previewOffsetC, preview.candidateActive,
            preview.pathDistanceRawUnits, preview.elapsedGameHours,
            assessment.previousRegion.empty() ? "none" :
                aetherclime::GeographicEvidenceTierName(aetherclime::ClassifyGeographicEvidence(assessment.previousSource)),
            aetherclime::GeographicEvidenceTierName(aetherclime::ClassifyGeographicEvidence(effective.source)),
            aetherclime::GeographicEvidenceChangeName(aetherclime::CompareGeographicEvidence(
                assessment.previousSource, effective.source, assessment.comparable)),
            aetherclime::TemperatureContinuityReasonName(preview.reason),
            preview.distanceRemainingFraction, preview.timeRemainingFraction,
            preview.compensationCapped);
    }

    // A summary on up to eight manual saves is independent of the per-event
    // budget. No extra sampling or file writes: the same observations feed it.
    if (trigger == "save" && g_temperatureHandoffSummaryReports < kTemperatureHandoffSummaryLimit) {
        ++g_temperatureHandoffSummaryReports;
        const auto& c = g_temperatureHandoffCoverage.Snapshot();
        SKSE::log::info(
            "Temperature handoff coverage: trigger=save | observations={} | baselines={} "
            "| invalidExteriorSamples={} | rejectedComparisons={} | comparablePairs={} "
            "| fallbackSamples={} | provisionalSamples={} | anchorSamples={} | namedSamples={} "
            "| rejectedWorldspace={} | rejectedDayOrTime={} "
            "| rejectedDistanceOrAge={} | rejectedWeather={} "
            "| sourceSwitchSameProfile={} | profileStepsBelowThreshold={} "
            "| significantProfileSteps={} | significantTowardStronger={} "
            "| significantTowardWeaker={} | significantOther={} "
            "| fallbackToAnchor={} | anchorToFallback={} | uniqueTierRoutes={} "
            "| hasPeakProfileStep={} | peakSignedProfileStepC={:+.2f} "
            "| peakFrom='{}' | peakTo='{}' | peakPreviousSource={} | peakCurrentSource={} "
            "| peakDistanceRawUnits={:.1f} | peakAgeGameHours={:.3f} "
            "| handoffLinesEmitted={} | routineLinesEmitted={} | summaryIndex={} "
            "| sampledEventsOnly=true | readOnly=true | perFramePolling=false",
            c.observations, c.baselines, c.invalidExteriorSamples, c.rejectedComparisons,
            c.comparablePairs,
            c.fallbackSamples, c.provisionalSamples, c.anchorSamples, c.namedSamples,
            c.rejectedWorldspace, c.rejectedDayOrTime,
            c.rejectedDistanceOrAge, c.rejectedWeather,
            c.sourceSwitchSameProfile, c.profileStepsBelowThreshold,
            c.significantProfileSteps, c.significantTowardStronger, c.significantTowardWeaker,
            c.significantOther, c.fallbackToAnchor, c.anchorToFallback, c.uniqueTierRoutes,
            c.hasPeakProfileStep, c.peakSignedProfileStepC,
            c.peakPreviousRegion, c.peakCurrentRegion,
            aetherclime::ClimateRegionSourceName(c.peakPreviousSource),
            aetherclime::ClimateRegionSourceName(c.peakCurrentSource),
            c.peakDistanceRawUnits, c.peakAgeGameHours,
            g_temperatureHandoffReportBudget.TotalEmitted(),
            g_temperatureHandoffReportBudget.RoutineEmitted(),
            g_temperatureHandoffSummaryReports);
        const auto& gap = g_geographicGapReport.Summary();
        SKSE::log::info(
            "Geographic gap review: trigger=save | comparablePairs={} | qualifyingEvents={} "
            "| fallbackEntries={} | fallbackExits={} | provisionalProfileSteps={} "
            "| otherSignificantProfileSteps={} | retainedCandidates={} "
            "| mergedNearbyOccurrences={} | replacedRepresentatives={} | droppedFromTopList={} "
            "| candidateOutputLimit=4 | maxCandidates=6 | summaryIndex={} "
            "| reviewCandidatesNotVerifiedGaps=true | sampledEventsOnly=true "
            "| worldspaceCoordinatesOnly=true | readOnly=true | perFramePolling=false",
            gap.comparablePairs, gap.qualifyingEvents, gap.fallbackEntries, gap.fallbackExits,
            gap.provisionalProfileSteps, gap.otherSignificantProfileSteps, gap.candidateCount,
            gap.mergedNearbyOccurrences, gap.replacedRepresentatives, gap.droppedFromTopList,
            g_temperatureHandoffSummaryReports);
        const auto ranked = g_geographicGapReport.RankedCandidates();
        for (std::size_t i = 0; i < gap.candidateCount && i < 4; ++i) {
            const auto& c = ranked[i];
            SKSE::log::info(
                "Geographic gap candidate: rank={} | priority={} | kind={} | sampleIndex={} "
                "| occurrences={} | worldspace=0x{:08X} "
                "| previousXYRaw=({:.1f},{:.1f}) | currentXYRaw=({:.1f},{:.1f}) "
                "| previousRegion='{}' | currentRegion='{}' "
                "| previousSource={} | currentSource={} | airDeltaC={:+.2f} "
                "| distanceRawUnits={:.1f} | ageGameHours={:.3f} "
                "| profileChanged={} | significant={} "
                "| previousFallbackGate={} | currentFallbackGate={} "
                "| currentDirectSource={} | currentContinuityStatus={} "
                "| notProofOfHoldBoundary=true | noAutomaticAtlasEdits=true | readOnly=true",
                i + 1, aetherclime::GeographicGapPriorityName(c.priority),
                aetherclime::GeographicGapKindName(c.kind), c.sampleIndex, c.occurrences,
                c.worldspaceFormId, c.previousXRaw, c.previousYRaw,
                c.currentXRaw, c.currentYRaw,
                c.previousRegion, c.currentRegion,
                aetherclime::ClimateRegionSourceName(c.previousSource),
                aetherclime::ClimateRegionSourceName(c.currentSource),
                c.airDeltaC, c.distanceRawUnits, c.ageGameHours,
                c.profileChanged, c.significant,
                aetherclime::GeographicFallbackReasonName(c.previousFallback.reason),
                aetherclime::GeographicFallbackReasonName(c.currentFallback.reason),
                aetherclime::ClimateRegionSourceName(c.currentFallback.directSource),
                aetherclime::ContinuityStatusName(c.currentFallback.continuityStatus));
        }
        SKSE::log::info(
            "Geographic corridor review: trigger=save | qualifyingEvents={} "
            "| retainedCorridors={} | mergedOccurrences={} | shortenedRepresentatives={} "
            "| listReplacements={} | droppedFromTopList={} "
            "| endpointMatchRawUnits={:.0f} | outputLimit=4 | maxCorridors={} "
            "| summaryIndex={} | sampledBracketsNotBoundary=true | noAtlasEdits=true "
            "| readOnly=true | perFramePolling=false",
            gap.qualifyingEvents, gap.corridorCount, gap.corridorMergedOccurrences,
            gap.corridorRepresentativesReplaced, gap.corridorListReplacements,
            gap.corridorDroppedFromTopList,
            aetherclime::GeographicGapReport::kCorridorEndpointMatchRawUnits,
            aetherclime::GeographicGapReport::kMaxCorridors,
            g_temperatureHandoffSummaryReports);
        const auto corridors = g_geographicGapReport.RankedCorridors();
        for (std::size_t i = 0; i < gap.corridorCount && i < 4; ++i) {
            const auto& corridor = corridors[i];
            const auto& r = corridor.representative;
            SKSE::log::info(
                "Geographic corridor candidate: rank={} | corridorId={} | priority={} "
                "| worldspace=0x{:08X} | occurrences={} | sameDirection={} "
                "| reverseDirection={} | observedBothDirections={} "
                "| firstSampleIndex={} | lastSampleIndex={} | representativeSampleIndex={} "
                "| kind={} | from='{}' | to='{}' "
                "| previousSource={} | currentSource={} "
                "| previousXYRaw=({:.1f},{:.1f}) | currentXYRaw=({:.1f},{:.1f}) "
                "| shortestObservedSpanRawUnits={:.1f} | representativeAirDeltaC={:+.2f} "
                "| currentFallbackGate={} | currentContinuityStatus={} "
                "| previousFallbackGate={} | previousOriginDistanceRawUnits={:.1f} | previousOriginAgeGameHours={:.3f} "
                "| currentOriginDistanceRawUnits={:.1f} | currentOriginAgeGameHours={:.3f} "
                "| notExactBoundary=true | readOnly=true",
                i + 1, corridor.corridorId,
                aetherclime::GeographicGapPriorityName(corridor.priority), r.worldspaceFormId,
                corridor.occurrences, corridor.sameDirectionOccurrences,
                corridor.reverseDirectionOccurrences, corridor.observedBothDirections,
                corridor.firstSampleIndex, corridor.lastSampleIndex, r.sampleIndex,
                aetherclime::GeographicGapKindName(r.kind), r.previousRegion, r.currentRegion,
                aetherclime::ClimateRegionSourceName(r.previousSource),
                aetherclime::ClimateRegionSourceName(r.currentSource),
                r.previousXRaw, r.previousYRaw, r.currentXRaw, r.currentYRaw,
                corridor.shortestObservedSpanRawUnits, r.airDeltaC,
                aetherclime::GeographicFallbackReasonName(r.currentFallback.reason),
                aetherclime::ContinuityStatusName(r.currentFallback.continuityStatus),
                aetherclime::GeographicFallbackReasonName(r.previousFallback.reason),
                r.previousFallback.originDistanceRawUnits, r.previousFallback.originAgeGameHours,
                r.currentFallback.originDistanceRawUnits, r.currentFallback.originAgeGameHours);
        }
        const auto& proposal = g_atlasProposalReview.Summary();
        SKSE::log::info(
            "Atlas proposal review: trigger=save | fallbackHandoffs={} | eligibleDraftEvents={} "
            "| retainedDrafts={} | mergedDraftEvents={} | droppedDraftEvents={} "
            "| weakSupport={} | unsafeFallback={} | wideBrackets={} | conflictsWithConfig={} "
            "| existingCoverage={} | capacityRejected={} | invalidOrIneligible={} "
            "| retainedRegionalGroups={} | droppedRegionalEvents={} | effectiveAtlasObservations={} "
            "| trackedAtlasZones={} | untrackedAtlasObservations={} | summaryIndex={} "
            "| manualReviewRequired=true | noAtlasEdits=true | readOnly=true",
            proposal.fallbackHandoffs, proposal.eligibleDrafts, proposal.draftCount,
            proposal.mergedDrafts, proposal.droppedDrafts, proposal.weakSupport,
            proposal.unsafeFallback, proposal.wideBrackets, proposal.conflicts,
            proposal.existingCoverage, proposal.capacityRejected, proposal.invalidOrIneligible,
            proposal.regionCount, proposal.droppedRegionalEvents, proposal.effectiveAtlasObservations,
            proposal.atlasUseCount, proposal.untrackedAtlasObservations, g_temperatureHandoffSummaryReports);
        const auto regions = g_atlasProposalReview.RankedRegions();
        for (std::size_t i=0; i<proposal.regionCount && i<4; ++i) {
            const auto& r=regions[i];
            SKSE::log::info(
                "Atlas regional issue: worldspace=0x{:08X} | region='{}' | entries={} | exits={} "
                "| significant={} | originTooFar={} | draftEvents={} | peakAbsDeltaC={:.2f} "
                "| groupingNotSpatialBoundary=true | readOnly=true",
                r.worldspaceFormId, r.region, r.entries, r.exits, r.significant, r.originTooFar, r.drafts, r.peakAbsDeltaC);
        }
        const auto atlasUse = g_atlasProposalReview.RankedAtlasUse();
        for (std::size_t i=0; i<proposal.atlasUseCount && i<4; ++i) {
            const auto& u=atlasUse[i];
            SKSE::log::info(
                "Atlas effective use: worldspace=0x{:08X} | region='{}' | zone='{}' "
                "| observations={} | effectiveSource=provisional-atlas-zone | sampledEventsOnly=true | trusted=false",
                u.worldspaceFormId,u.region,u.label,u.observations);
        }
        const auto drafts = g_atlasProposalReview.RankedDrafts();
        for (std::size_t i=0; i<proposal.draftCount && i<4; ++i) {
            const auto& d=drafts[i]; const auto& z=d.zone; const auto& e=d.observation;
            const auto& gate=d.fallbackWasPrevious?e.previousFallback:e.currentFallback;
            SKSE::log::info(
                "Atlas proposal candidate: rank={} | status={} | sampleIndex={} | occurrences={} "
                "| label='{}' | regionKey='{}' | worldspaceFormId={} "
                "| minXRaw={:.1f} | maxXRaw={:.1f} | minYRaw={:.1f} | maxYRaw={:.1f} "
                "| fallbackXYRaw=({:.1f},{:.1f}) | spanRawUnits={:.1f} | airDeltaC={:+.2f} "
                "| fallbackGate={} | originDistanceRawUnits={:.1f} | originAgeGameHours={:.3f} "
                "| provisionalOnly=true | manualReviewRequired=true | noAtlasEdits=true",
                i+1, aetherclime::AtlasProposalStatusName(d.status), e.sampleIndex, d.occurrences,
                z.label,z.regionKey,z.worldspaceFormId,z.minXRaw,z.maxXRaw,z.minYRaw,z.maxYRaw,
                d.fallbackXRaw,d.fallbackYRaw,e.distanceRawUnits,e.airDeltaC,
                aetherclime::GeographicFallbackReasonName(gate.reason),gate.originDistanceRawUnits,gate.originAgeGameHours);
        }
    }
}

// Alpha-41: bounded diagnostics share the already-collected snapshot. Only
// load/save, no new hooks or polling; the strings are test recipes, not policy.
void LogConditionQueries(std::string_view trigger, const aetherclime::EnvironmentSnapshot& state) {
    if (trigger != "load" && trigger != "save") return;
    const auto season = state.available && g_climate ?
        std::optional<aetherclime::Season>{GetClimate().GetSeason(state.date).official} : std::nullopt;
    constexpr std::array<std::string_view, 8> rules{
        "month>=0 & month<=11 & day>=1 & day<=40 & time>=00:00:00 & time<=23:59:59",
        "time>=18:30 | time<06:00",
        "outdoorAirC<0",
        "false & unsupportedField=1",
        "biome=coastal & biomeWeight(coastal)>=0.5",
        "seasonWeight(summer)>=0 & seasonWeight(summer)<=1",
        "biomeEvidence=unique | biomeEvidence=overlap | biomeEvidence=overlap-edge",
        "previewAirC>0"
    };
    for (std::size_t i=0; i<rules.size(); ++i) {
        AetherclimeConditionResultV1 result{};
        aetherclime::WriteConditionResultV1(rules[i].data(), static_cast<std::uint32_t>(rules[i].size()),
            state, season, &result, static_cast<std::uint32_t>(sizeof(result)));
        SKSE::log::info("Condition query v1: trigger={} | recipe={} | rule='{}' | status={} | error={} | errorOffset={} "
            "| snapshotAvailable={} | year={} | month={} | day={} | hour={:.6f} | regionTrusted={} "
            "| sameSnapshot=true | model=explicit-field | readOnly=true",
            trigger, i+1, rules[i],
            aetherclime::ConditionStatusName(static_cast<aetherclime::ConditionStatus>(result.status)),
            result.error, result.errorOffset, state.available,
            state.date.year, state.date.month, state.date.day, state.date.hour, state.trustedRegion);
    }
}

// Separate DLL executes public-ABI calls; host only schedules and logs the report.
void LogExternalApiConsumerProbe() {
    if (!g_climate || !g_climate->Settings().apiConsumerProbeEnabled || g_apiConsumerProbeAttempted) return;
    g_apiConsumerProbeAttempted=true;
    const auto host=aetherclime::probe::RunExternalApiProbe();
    if (host.status!=aetherclime::probe::HostStatus::Loaded) {
        SKSE::log::warn("API consumer probe: trigger=save | status={} | windowsError={} | externalDll=true | readOnly=true",
            aetherclime::probe::HostStatusName(host.status),host.windowsError);
        return;
    }
    const auto& r=host.report;
    const char* status=!r.completed ? "incomplete" : r.failed ? "failed" : r.skipped ? "inconclusive" : "passed";
    SKSE::log::info("API consumer probe: trigger=save | status={} | exportsMask={} | completed={} | passed={} | failed={} | skipped={} "
        "| contextAvailable={} | externalDll=true | coreLinked=false | readOnly=true | oncePerSession=true",
        status,r.resolvedMask,r.completed,r.passed,r.failed,r.skipped,r.contextAvailable);
    for(std::uint32_t i=0;i<aetherclime::probe::kCheckCount;++i) {
        const char* outcome=r.checks[i]==aetherclime::probe::Passed ? "passed" : r.checks[i]==aetherclime::probe::Failed ? "failed" : "skipped";
        SKSE::log::info("API consumer check: index={} | name={} | status={}",i+1,aetherclime::probe::kCheckNames[i],outcome);
    }
    if(r.contextAvailable) {
        SKSE::log::info("API consumer observed V3: schema={} | flags={} | outdoorBasis={} "
            "| outdoorEstimateC={:.2f} | indoorBasis={} | indoorEstimateC={:.2f} "
            "| available={} | independentCalls=true | readOnly=true",
            r.observedV3.schemaVersion,r.observedV3.flags,r.observedV3.outdoorEstimateBasis,
            r.observedV3.outdoorEstimateC,r.observedV3.indoorEstimateBasis,
            r.observedV3.indoorEstimateC,r.checks[22]==aetherclime::probe::Passed);
        SKSE::log::info("API consumer observed: schema={} | cell=0x{:08X} | flags={} | legacyAirC={:.2f} | previewAirC={:.2f} "
            "| previewAvailable={} | interior={} | regionTrusted={} | localAmbientAvailable={} | modeledIndoor={} | independentCalls=true | readOnly=true",
            r.observed.schemaVersion,r.observed.legacy.cellFormId,r.observed.flags,r.observed.legacy.outdoorAirC,r.observed.previewAirC,
            (r.observed.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE)!=0,
            (r.observed.legacy.flags&AETHERCLIME_ENV_INTERIOR)!=0,
            (r.observed.legacy.flags&AETHERCLIME_ENV_TRUSTED_REGION)!=0,
            (r.observed.legacy.flags&AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE)!=0,
            (r.observed.legacy.flags&AETHERCLIME_ENV_LOCAL_AMBIENT_MODELED_INDOOR)!=0);
    }
}

void LogEnvironmentContextAudit(std::string_view trigger,const aetherclime::EnvironmentSnapshot& state) {
    if(!g_climate || !g_climate->Settings().apiConsumerProbeEnabled || (trigger!="load"&&trigger!="save"))return;
    const auto audit=aetherclime::AuditEnvironmentContext(state);
    SKSE::log::info("Context contract audit: trigger={} | context={} | status={} | checks={} | failedMask={} "
        "| available={} | interior={} | regionTrusted={} | provisional={} | localAmbientAvailable={} "
        "| modeledIndoor={} | previewAvailable={} | weatherSource={} | verifiedWeather=0x{:08X} "
        "| sampledContextOnly=true | readOnly=true",
        trigger,aetherclime::EnvironmentContextName(state),audit.Passed()?"passed":"failed",aetherclime::kEnvironmentContractChecks,audit.failedMask,
        state.available,state.interior,state.trustedRegion,state.provisionalRegion,state.localAmbientC.has_value(),
        state.localAmbientIsModeledIndoor,state.previewAirC.has_value(),aetherclime::OutdoorWeatherProvenanceName(state.weatherProvenance),state.verifiedOutdoorWeatherFormId);
    for(std::uint32_t i=0;i<aetherclime::kEnvironmentContractChecks;++i)if(audit.failedMask&(1u<<i))
        SKSE::log::warn("Context contract failure: trigger={} | check={}",trigger,aetherclime::kEnvironmentContractNames[i]);
}

void LogEnvironmentalSnapshot(std::string_view trigger) {
    const auto state = ReadCurrentEnvironmentSnapshot(true);
    if (!state) {
        LogEnvironmentContextAudit(trigger,aetherclime::EnvironmentSnapshot{});
        LogConditionQueries(trigger, aetherclime::EnvironmentSnapshot{});
        SKSE::log::info("Environment snapshot v1: trigger={} | available=false | reason=unsettled-player-cell-or-climate", trigger);
        return;
    }
    LogEnvironmentContextAudit(trigger,*state);
    LogConditionQueries(trigger, *state);
    SKSE::log::info("Climate estimate audit: trigger={} | cell=0x{:08X} | interior={} "
        "| legacyRegionTrusted={} | legacyAirC={:.2f} "
        "| outdoorBasis={} | outdoorEstimateAvailable={} | outdoorEstimateC={:.2f} "
        "| indoorBasis={} | indoorEstimateAvailable={} | indoorEstimateC={:.2f} "
        "| weatherSource={} | reference='{}' | referenceDistanceRaw={:.0f} "
        "| referenceInfluence={:.3f} | referenceCount={} "
        "| inferredNotMeasured=true | v1v2Unchanged=true | readOnly=true",
        trigger,state->local.cellFormId,state->interior,state->trustedRegion,state->outdoorAirC,
        aetherclime::OutdoorEstimateBasisName(state->outdoorEstimateBasis),
        state->estimatedOutdoorAirC.has_value(),state->estimatedOutdoorAirC.value_or(0.0),
        aetherclime::IndoorEstimateBasisName(state->indoorEstimateBasis),
        state->estimatedIndoorAmbientC.has_value(),state->estimatedIndoorAmbientC.value_or(0.0),
        aetherclime::OutdoorWeatherProvenanceName(state->weatherProvenance),
        state->outdoorReferenceLabel.empty()?"(none)":state->outdoorReferenceLabel,
        state->outdoorReferenceDistanceRaw,state->outdoorReferenceInfluence,
        state->outdoorReferenceCount);
    if (trigger=="load" || trigger=="save") {
        const auto positionEvidence=CollectPlayerClimateEvidence(RE::PlayerCharacter::GetSingleton());
        const auto p=positionEvidence.exteriorPosition;
        SKSE::log::info("Climate fallback position: trigger={} | worldspace=0x{:08X} "
            "| xRaw={:.1f} | yRaw={:.1f} | hasExteriorPosition={} "
            "| notProofOfHoldBoundary=true | readOnly=true",
            trigger,p?p->worldspaceFormId:0u,p?p->xRaw:0.0,p?p->yRaw:0.0,p.has_value());
        SKSE::log::info("Climate publication: trigger={} | policy=regional-v1 | publishedAirC={:.2f} "
            "| previewAvailable={} | previewAirC={:.2f} | regionTrusted={} | automaticModelSwitch=false | readOnly=true",
            trigger,state->outdoorAirC,state->previewAirC.has_value(),state->previewAirC.value_or(0.0),state->trustedRegion);
        AetherclimeEnvironmentSnapshotV2 v2{};
        if (aetherclime::ToPublicSnapshotV2(*state,v2)) {
            SKSE::log::info("Environment snapshot v2: trigger={} | available=true | flags={} | officialSeason={} "
                "| seasonWeights=({:.3f},{:.3f},{:.3f},{:.3f}) | biomeEvidence={} | biomeStatus='{}' "
                "| biomeA='{}' | weightA={:.3f} | biomeB='{}' | weightB={:.3f} "
                "| legacyAirC={:.2f} | previewAvailable={} | previewAirC={:.2f} "
                "| legacyModelUnchanged=true | readOnly=true",
                trigger,v2.flags,v2.officialSeason,v2.seasonWeights[0],v2.seasonWeights[1],v2.seasonWeights[2],v2.seasonWeights[3],
                aetherclime::BiomeEvidenceName(static_cast<aetherclime::BiomeEvidence>(v2.biomeEvidence)),v2.biomeStatus,
                v2.biomeA,v2.biomeWeightA,v2.biomeB,v2.biomeWeightB,v2.legacy.outdoorAirC,
                (v2.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE)!=0,v2.previewAirC);
        }
    }
    SKSE::log::info(
        "Environment snapshot v1: trigger={} | available=true | local='{}' "
        "| nameSource={} | nameCertainty={} | nameNotProofOfPreciseOutdoorLocation=true "
        "| namedLocation=0x{:08X} | cell=0x{:08X} | region='{}' | regionTrusted={} "
        "| climateSource={} | provisionalRegion={} "
        "| building={} | exteriorWeatherSource={} | verifiedWeather=0x{:08X} "
        "| outdoorAirC={:.2f} | localAmbientAvailable={} | localAmbientC={:.2f} "
        "| localAmbientModeledIndoor={} | exposedApi=GetEnvironmentSnapshotV1 "
        "| queryOnDemand=true | noWeatherWrites=true | perFramePolling=false",
        trigger, state->local.name,
        aetherclime::LocalNameSourceName(state->local.source),
        aetherclime::LocalNameCertaintyName(state->local.certainty),
        state->local.namedLocationFormId, state->local.cellFormId,
        state->regionKey, state->trustedRegion, state->regionSource, state->provisionalRegion,
        aetherclime::BuildingKindName(state->building.kind),
        aetherclime::OutdoorWeatherProvenanceName(state->weatherProvenance),
        state->verifiedOutdoorWeatherFormId,
        state->outdoorAirC, state->localAmbientC.has_value(),
        state->localAmbientC.value_or(0.0), state->localAmbientIsModeledIndoor);
}

void LogClimateSnapshot() {
    if (!g_climate) {
        SKSE::log::warn("Climate snapshot skipped: data has not initialized.");
        return;
    }

    try {
        const auto sample = Snapshot();
        const auto result = g_climate->Calculate(sample);
        const auto* player = RE::PlayerCharacter::GetSingleton();
        const auto* location = player ? player->GetCurrentLocation() : nullptr;
        const auto* cell = player ? player->GetParentCell() : nullptr;
        const auto environment = aetherclime::ClassifyEnvironment(
            cell ? std::optional<bool>(cell->IsInteriorCell()) : std::nullopt);
        const char* locationName = location && location->GetName() ? location->GetName() : "(wilderness or unknown)";
        const auto* sky = RE::Sky::GetSingleton();
        const auto* weather = sky ? sky->currentWeather : nullptr;
        const auto* worldspace = player ? player->GetWorldspace() : nullptr;
        const char* worldspaceName = worldspace ? worldspace->GetFormEditorID() : nullptr;
        const auto evidence = CollectPlayerClimateEvidence(player);
        const auto retainedWeather = ExteriorWeatherForSample(sample, evidence);
        const auto weatherId = retainedWeather.usable && retainedWeather.observation
            ? retainedWeather.observation->weatherFormId : (weather ? weather->GetFormID() : 0);
        const auto hour = static_cast<int>(sample.date.hour);
        const auto minute = std::clamp(static_cast<int>((sample.date.hour - hour) * 60.0), 0, 59);

        // Read-only, single-shot diagnostic after a save loads. In particular,
        // this does NOT force weather or modify NAT/Bottled render state.
        SKSE::log::info(
            "Climate snapshot: {} {} {}, {:02}:{:02} | location='{}' | region='{}' | "
            "weather=0x{:08X} ({}) | season={} progress={:.2f} | air={:.1f} C / {:.1f} F | feels={:.1f} C "
            "| altitudeOffsetC={:.3f} | temperatureBasis=outdoor-not-indoor | outdoorWeatherSource={}",
            aetherclime::kMonths.at(static_cast<std::size_t>(sample.date.month)),
            sample.date.day, sample.date.year, hour, minute,
            locationName, sample.region, weatherId, WeatherLabel(sample.weather),
            aetherclime::SeasonName(result.season.official), result.season.progress,
            result.airC, aetherclime::Climate::ToFahrenheit(result.airC), result.feelsLikeC,
            result.altitudeOffsetC, OutdoorWeatherSource(retainedWeather, evidence.interior));
        SKSE::log::info(
            "Environment context: worldspace='{}' (0x{:08X}) | locationChain='{}' | "
            "weatherFlags=[pleasant={}, cloudy={}, rainy={}, snow={}] | sky=[rain={}, snow={}]",
            worldspaceName ? worldspaceName : "(interior or unknown)",
            worldspace ? worldspace->GetFormID() : 0,
            LocationChain(location),
            weather && weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kPleasant),
            weather && weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kCloudy),
            weather && weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy),
            weather && weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kSnow),
            sky && sky->IsRaining(), sky && sky->IsSnowing());
        // Interior CELLs still inherit the engine's current sky/weather.
        // That is useful context, but the climate core's temperature remains
        // an outdoor-air estimate until an indoor model is implemented.
        SKSE::log::info(
            "Environment classification: cell='{}' (0x{:08X}) | cellType={} | "
            "outdoorWeatherExposure={} | temperatureBasis=outdoor-baseline{}",
            cell && cell->GetFormEditorID() ? cell->GetFormEditorID() : "(unavailable)",
            cell ? cell->GetFormID() : 0,
            aetherclime::ToString(environment.cell),
            aetherclime::ToString(environment.outdoorWeatherExposure),
            environment.cell == aetherclime::CellEnvironment::Interior ? " (separate indoor model: see Indoor climate audit)" : "");
        LogClimateRegionResolution("load");
        LogIndoorClimateContext("load");
        LogBuildingClassification("load");
        LogEnvironmentalSnapshot("load");
        const auto started = std::chrono::steady_clock::now();
        LogGeographyDiagnostics();
        LogWeatherRegionDiagnostics();
        const auto durationMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        SKSE::log::info("Performance audit: trigger=load | scope=geography-and-weather-diagnostics "
            "| inclusiveElapsedMs={:.3f} | includesSynchronousLogIO=true | perFramePolling=false",
            durationMs);
    } catch (const std::exception& e) {
        SKSE::log::error("Climate snapshot failed safely: {}", e.what());
    }
}

// The optional live hook invokes this reader from the player's normal Update
// callback. We never create a worker thread or queue asynchronous form reads.
void ObserveMonitorEvent(std::string_view trigger) {
    if (!g_monitor || !g_climate) return;
    try {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->GetParentCell()) return;
        const auto sample = Snapshot();
        const auto reading = g_climate->Calculate(sample);
        const auto* cell = player->GetParentCell();
        const auto* location = player->GetCurrentLocation();
        const auto* sky = RE::Sky::GetSingleton();
        const auto* weather = sky ? sky->currentWeather : nullptr;
        const auto* worldspace = player->GetWorldspace();
        aetherclime::MonitorObservation observation{
            .region = sample.region,
            .locationFormId = location ? location->GetFormID() : 0,
            .cellFormId = cell->GetFormID(),
            .worldspaceFormId = worldspace ? worldspace->GetFormID() : 0,
            .weatherFormId = weather ? weather->GetFormID() : 0,
            .cell = aetherclime::ClassifyEnvironment(cell->IsInteriorCell()).cell,
            .season = reading.season.official,
            .outdoorAirC = reading.airC
        };
        const double elapsedSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - g_monitorEpoch).count();
        const auto decision = g_monitor->Observe(observation, elapsedSeconds);
        if (decision.report) {
            SKSE::log::info(
                "Monitor observation: trigger={} | changes={} | region='{}' | location=0x{:08X} | "
                "cell=0x{:08X} | worldspace=0x{:08X} | weather=0x{:08X} | "
                "cellType={} | season={} | outdoorAir={:.1f} C",
                trigger, aetherclime::ReasonsToString(decision.reasons), observation.region,
                observation.locationFormId, observation.cellFormId,
                observation.worldspaceFormId, observation.weatherFormId,
                aetherclime::ToString(observation.cell),
                aetherclime::SeasonName(observation.season), observation.outdoorAirC);
            // The live hook only inspects regional records when the monitor
            // reports a change. Save and load diagnostics are handled by their
            // own event handlers for consistent one-shot sampling.
            if (trigger == "tick") LogGeographyDiagnostics();
        }
    } catch (const std::exception& e) {
        SKSE::log::error("Monitor observation failed safely: {}", e.what());
    }
}

// Alpha-10: cache the *recorded* weather-region polygons when the active
// exterior cell changes. The movement hook never scans weather records on a
// normal frame or on each sampled position within the same cell.
// Conservative global caps defend against malformed/mod-heavy region lists.
void RefreshBoundaryCellCache(RE::TESObjectCELL* cell, std::uint32_t worldspaceFormId) {
    BoundaryCellCache next;
    next.cellFormId = cell ? cell->GetFormID() : 0;
    next.worldspaceFormId = worldspaceFormId;
    next.initialized = true;
    next.complete = true;
    if (!cell || !cell->IsExteriorCell() || worldspaceFormId == 0) {
        next.complete = false;
        g_boundaryCellCache = std::move(next);
        return;
    }
    auto* list = cell->GetRegionList(false);
    if (!list) {
        g_boundaryCellCache = std::move(next); // No recorded candidates.
        return;
    }
    constexpr std::size_t kMaxCandidateRegions = 64;
    constexpr std::size_t kMaxWeatherRegions = 32;
    constexpr std::size_t kMaxDataEntriesPerRegion = 128;
    constexpr std::size_t kMaxTotalPolygons = 128;
    constexpr std::size_t kMaxVerticesPerPolygon = 2048;
    constexpr std::size_t kMaxTotalVertices = 16384;
    std::size_t visited = 0;
    std::size_t polygonsVisited = 0;
    std::size_t verticesVisited = 0;
    for (auto* region : *list) {
        if (++visited > kMaxCandidateRegions) { next.complete = false; break; }
        if (!region || !region->dataList) continue;
        bool weatherRecord = false;
        std::uint8_t weatherPriority = 0;
        bool weatherOverrideFlag = false;
        std::size_t dataCount = 0;
        for (auto* entry : region->dataList->regionDataList) {
            if (++dataCount > kMaxDataEntriesPerRegion) {
                next.complete = false;
                break;
            }
            if (entry && entry->GetType() == RE::TESRegionData::Type::kWeather) {
                // Snapshot the same region weather header used by the
                // save/load priority audit. No weather records are modified.
                if (!weatherRecord || entry->dataHeader.priority > weatherPriority) {
                    weatherPriority = entry->dataHeader.priority;
                    weatherOverrideFlag = entry->dataHeader.flags.any(
                        RE::TESRegionData::DataHeader::Flag::kOverride);
                }
                weatherRecord = true;
            }
        }
        if (!next.complete) break;
        if (!weatherRecord) continue;
        if (next.regions.size() >= kMaxWeatherRegions) {
            next.complete = false;
            break;
        }
        if (std::any_of(next.regions.begin(), next.regions.end(),
            [&](const BoundaryCachedRegion& r) {
                return r.regionFormId == region->GetFormID();
            })) continue;
        BoundaryCachedRegion cached;
        cached.regionFormId = region->GetFormID();
        cached.worldspaceFormId = region->worldSpace ? region->worldSpace->GetFormID() : 0;
        cached.priority = weatherPriority;
        cached.overrideFlag = weatherOverrideFlag;
        if (region->pointLists) {
            for (auto* polygonList : *region->pointLists) {
                if (++polygonsVisited > kMaxTotalPolygons) {
                    next.complete = false;
                    break;
                }
                if (!polygonList) { cached.incomplete = true; continue; }
                aetherclime::RegionPolygon polygon;
                for (auto* point : *polygonList) {
                    if (!point) { cached.incomplete = true; continue; }
                    if (polygon.size() >= kMaxVerticesPerPolygon ||
                        verticesVisited >= kMaxTotalVertices) {
                        next.complete = false;
                        break;
                    }
                    polygon.push_back({point->point.x, point->point.y});
                    ++verticesVisited;
                }
                if (!next.complete) break;
                cached.polygons.push_back(std::move(polygon));
            }
        }
        if (!next.complete) break;
        next.regions.push_back(std::move(cached));
    }
    // Partial results from a truncated scan must never imply a crossing.
    if (!next.complete) next.regions.clear();
    g_boundaryCellCache = std::move(next);
}

struct BoundaryObservationOutcome {
    std::uint32_t transitions{0};
    bool geographicDiscontinuity{false};
    // Alpha-15: copied, bounded FormIDs + definite inside/outside endpoints;
    // borrowed synchronously by the correlator, no live engine form pointers.
    std::vector<aetherclime::BoundaryTransition> crossings;
};

BoundaryObservationOutcome ObserveBoundaryProbe(RE::TESObjectCELL* cell,
                          const aetherclime::MovementPosition& position,
                          aetherclime::MovementReason reason) {
    if (!g_boundaryProbeSessionActive || !g_boundaryTransitionProbe) return {};
    const auto started = std::chrono::steady_clock::now();
    aetherclime::BoundaryFrame frame;
    frame.cellFormId = position.cellFormId;
    frame.worldspaceFormId = position.worldspaceFormId;
    bool cacheRefreshed = false;
    if (position.exteriorWorldPosition && cell && cell->IsExteriorCell() &&
        position.worldspaceFormId != 0) {
        const auto& p = *position.exteriorWorldPosition;
        frame.position = aetherclime::RegionPoint2{p.x, p.y};
        if (!g_boundaryCellCache.initialized ||
            g_boundaryCellCache.cellFormId != position.cellFormId ||
            g_boundaryCellCache.worldspaceFormId != position.worldspaceFormId) {
            RefreshBoundaryCellCache(cell, position.worldspaceFormId);
            cacheRefreshed = true;
        }
        frame.candidateSnapshotComplete = g_boundaryCellCache.complete;
        if (frame.candidateSnapshotComplete) {
            frame.regions.reserve(g_boundaryCellCache.regions.size());
            for (const auto& region : g_boundaryCellCache.regions) {
                const std::optional<bool> sameWorldspace = region.worldspaceFormId
                    ? std::optional<bool>{region.worldspaceFormId == position.worldspaceFormId}
                    : std::nullopt;
                const auto footprint = aetherclime::AssessRegionFootprint(
                    region.polygons, frame.position, sameWorldspace, region.incomplete);
                frame.regions.push_back({region.regionFormId, footprint.status});
            }
        }
    } else {
        g_boundaryCellCache = {}; // Never retain interior or other-worldspace geometry.
    }
    auto result = g_boundaryTransitionProbe->Observe(frame);
    const auto elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    SKSE::log::info(
        "Boundary probe: observation | status={} | reason={} | cell=0x{:08X} "
        "| cache={} | candidateSnapshotComplete={} | candidates={} | shared={} "
        "| transitions={} | step={:.1f} raw-units | evaluationMs={:.3f} "
        "| geometryOnly=true | engineSelection=unresolved",
        aetherclime::BoundaryFrameStatusName(result.status),
        aetherclime::MovementReasonName(reason), position.cellFormId,
        cacheRefreshed ? "refreshed" : (frame.position ? "hit" : "excluded"),
        frame.candidateSnapshotComplete, frame.regions.size(),
        result.sharedCandidates, result.transitions.size(),
        result.horizontalStepRawUnits, elapsedMs);
    for (const auto& crossing : result.transitions) {
        if (g_boundaryTransitionLogCount >= kBoundaryTransitionLogBudget) break;
        // Both points are exterior, finite, same-worldspace measurements.
        if (!result.fromPosition || !frame.position) break;
        SKSE::log::info(
            "Boundary probe: transition | region=0x{:08X} | from={} | to={} "
            "| fromXY=({:.1f},{:.1f}) | toXY=({:.1f},{:.1f}) "
            "| sampledEndpointsOnly=true | engineSelection=unresolved",
            crossing.regionFormId,
            aetherclime::RegionFootprintName(crossing.from),
            aetherclime::RegionFootprintName(crossing.to),
            result.fromPosition->x, result.fromPosition->y,
            frame.position->x, frame.position->y);
        ++g_boundaryTransitionLogCount;
    }
    if (g_boundaryTransitionLogCount >= kBoundaryTransitionLogBudget) {
        g_boundaryProbeSessionActive = false;
        SKSE::log::warn("Boundary probe: status=transition-log-budget-exhausted "
            "| maxTransitionReports={} | disabledUntilNextLoad=true",
            kBoundaryTransitionLogBudget);
    }
    return {static_cast<std::uint32_t>(result.transitions.size()),
        result.status == aetherclime::BoundaryFrameStatus::ResetLargeJump ||
        result.status == aetherclime::BoundaryFrameStatus::ResetUnavailable ||
        result.status == aetherclime::BoundaryFrameStatus::ResetWorldspace,
        std::move(result.transitions)};
}

// Weather and region pointers are observed RAW evidence; they do not identify
// the controlling regional weather selector or prove cause and effect.
// This path is bounded to the existing experimental 2-second movement cadence.
void ObserveWeatherCorrelation(const aetherclime::MovementPosition& position,
                               const BoundaryObservationOutcome& outcome) {
    if (!g_weatherCorrelationSessionActive || !g_weatherCorrelationProbe) return;
    aetherclime::WeatherSkyReading reading;
    reading.exterior = position.exteriorWorldPosition.has_value();
    reading.worldspaceFormId = position.worldspaceFormId;
    if (reading.exterior) {
        if (const auto* sky = RE::Sky::GetSingleton()) {
            reading.skyAvailable = true;
            reading.currentWeatherFormId = sky->currentWeather ? sky->currentWeather->GetFormID() : 0;
            reading.lastWeatherFormId = sky->lastWeather ? sky->lastWeather->GetFormID() : 0;
            reading.overrideWeatherFormId = sky->overrideWeather ? sky->overrideWeather->GetFormID() : 0;
            reading.skyRegionPointerFormId = sky->region ? sky->region->GetFormID() : 0;
            reading.currentWeatherPct = sky->currentWeatherPct;
        }
    }
    const double elapsedSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - g_monitorEpoch).count();
    const auto result = g_weatherCorrelationProbe->Observe(
        reading, elapsedSeconds, outcome.transitions, outcome.geographicDiscontinuity);
    // Alpha-14: measure only timestamps of already-sampled signals. This
    // introduces no new engine calls, hook, or geometry scan.
    if (g_regionTimingProbe && !g_regionTimingBudgetAnnounced) {
        aetherclime::RegionPointerTimingInput timingInput;
        timingInput.sky = reading;
        timingInput.cellFormId = position.cellFormId;
        timingInput.sampledPolygonTransitions = outcome.transitions;
        timingInput.geographyDiscontinuity = outcome.geographicDiscontinuity;
        timingInput.sessionSeconds = elapsedSeconds;
        const auto timing = g_regionTimingProbe->Observe(timingInput);
        if (timing.report) {
            SKSE::log::info(
                "Region timing: status={} | cell=0x{:08X} | previousCell=0x{:08X} "
                "| skyPointer=0x{:08X} | previousSkyPointer=0x{:08X} "
                "| currentWeather=0x{:08X} | previousWeatherPointer=0x{:08X} "
                "| cellChanged={} | crossings={} | skyPointerChanged={} "
                "| weatherPairChanged={} | overrideChanged={} | sameSampleEvents={} "
                "| sampleGapSeconds={:.1f} | pointerAfterCrossingSeconds={:.1f} "
                "| pointerAfterCellSeconds={:.1f} | weatherAfterPointerSeconds={:.1f} "
                "| weatherAfterCrossingSeconds={:.1f} | crossingAfterPointerSeconds={:.1f} "
                "| reports={} | eventTimes=sample-observation-only "
                "| withinSampleOrder=unresolved | weatherCausality=unresolved "
                "| engineSelection=unresolved",
                aetherclime::RegionPointerTimingStatusName(timing.status),
                position.cellFormId, timing.previousCellFormId,
                reading.skyRegionPointerFormId, timing.previousSkyPointerFormId,
                reading.currentWeatherFormId, reading.lastWeatherFormId,
                timing.cellChanged, timing.sampledPolygonTransitions,
                timing.skyPointerChanged, timing.weatherPairChanged,
                timing.overrideChanged, timing.simultaneousSample,
                timing.sampleGapSeconds, timing.pointerSecondsAfterBoundary,
                timing.pointerSecondsAfterCell, timing.weatherSecondsAfterPointer,
                timing.weatherSecondsAfterBoundary, timing.boundarySecondsAfterPointer,
                timing.totalReports);
        }
        if (timing.exhausted) {
            g_regionTimingBudgetAnnounced = true;
            SKSE::log::warn("Region timing: status=report-budget-exhausted "
                "| maxReports=48 | disabledUntilNextLoad=true");
        }
    }
    // Alpha-15: associate pointer changes with crossings of the SPECIFIC
    // FormIDs involved (from/to). Boundary crossings are copied by Alpha-10;
    // no additional engine query, per-frame work or geometry calculation.
    if (g_regionSpecificProbe && !g_regionSpecificBudgetAnnounced) {
        aetherclime::RegionSpecificCorrelationInput specificInput;
        specificInput.sky = reading;
        specificInput.cellFormId = position.cellFormId;
        specificInput.sampledCrossings = outcome.crossings;
        specificInput.geographyDiscontinuity = outcome.geographicDiscontinuity;
        specificInput.sessionSeconds = elapsedSeconds;
        const auto specific = g_regionSpecificProbe->Observe(specificInput);
        if (specific.report) {
            SKSE::log::info(
                "Region-specific: status={} | pointerFrom=0x{:08X} | pointerTo=0x{:08X} "
                "| cell=0x{:08X} | previousCell=0x{:08X} | pointerChanged={} "
                "| cellChanged={} | weatherPairChanged={} | crossedRegions={} "
                "| relevantCrossings={} | sameSamplePointerAndCrossing={} "
                "| secondsSincePointerSwitch={:.1f} | sampleGapSeconds={:.1f} "
                "| reports={} | eventTimes=sample-observation-only "
                "| withinSampleOrder=unresolved | weatherCausality=unresolved "
                "| engineSelection=unresolved",
                aetherclime::RegionSpecificStatusName(specific.status),
                specific.previousPointer, specific.currentPointer,
                specific.cell, specific.previousCell, specific.pointerChanged,
                specific.cellChanged, specific.weatherPairChanged,
                specific.allCrossingCount, specific.relatedCrossingCount,
                specific.sameSampleCrossingAndPointer,
                specific.secondsSinceLastPointerSwitch, specific.sampleGapSeconds,
                specific.reports);
            if (specific.pointerChanged) {
                const auto logCrossingMatch = [](const char* role, std::uint32_t formId,
                    const aetherclime::RegionSpecificCrossingEvidence& evidence) {
                    SKSE::log::info(
                        "Region-specific match: role={} | region=0x{:08X} "
                        "| crossingAvailable={} | from={} | to={} "
                        "| secondsSinceThisRegionCrossing={:.1f} | within10s={} "
                        "| observedEndpointsOnly=true | causality=unresolved",
                        role, formId, evidence.available,
                        aetherclime::RegionFootprintName(evidence.from),
                        aetherclime::RegionFootprintName(evidence.to),
                        evidence.secondsBeforeCurrentSample, evidence.nearInTime);
                };
                logCrossingMatch("old-pointer", specific.previousPointer,
                    specific.previousPointerCrossing);
                logCrossingMatch("new-pointer", specific.currentPointer,
                    specific.currentPointerCrossing);
            }
            for (std::uint32_t i = 0; i < specific.relatedCrossingCount; ++i) {
                const auto& crossing = specific.relatedCrossings[i];
                SKSE::log::info(
                    "Region-specific crossing: region=0x{:08X} | from={} | to={} "
                    "| matchesCurrentPointer={} | matchesLatestSwitchFrom={} "
                    "| matchesLatestSwitchTo={} | secondsAfterPointerSwitch={:.1f} "
                    "| sameSampleOrder=unresolved | causality=unresolved",
                    crossing.regionFormId,
                    aetherclime::RegionFootprintName(crossing.from),
                    aetherclime::RegionFootprintName(crossing.to),
                    crossing.matchesCurrentPointer, crossing.matchesLatestSwitchFrom,
                    crossing.matchesLatestSwitchTo,
                    crossing.secondsAfterPointerSwitch);
            }
        }
        if (specific.exhausted) {
            g_regionSpecificBudgetAnnounced = true;
            SKSE::log::warn("Region-specific: status=report-budget-exhausted "
                "| maxReports=48 | disabledUntilNextLoad=true");
        }
    }
    // Alpha-13: use the same already-read RE::Sky scalar snapshot; there is
    // no second hook or engine field access. No geometry work happens here.
    // Reports only changes and coarse [0, .25, .5, .75, 1] milestones.
    if (g_skyLifecycleProbe && !g_skyLifecycleLogBudgetAnnounced) {
        const auto timeline = g_skyLifecycleProbe->Observe(
            reading, elapsedSeconds, outcome.geographicDiscontinuity);
        if (timeline.report) {
            SKSE::log::info(
                "Sky lifecycle: status={} | current=0x{:08X} | lastPointer=0x{:08X} "
                "| overridePointer=0x{:08X} | skyRegionPointer=0x{:08X} "
                "| previousCurrent=0x{:08X} | previousLastPointer=0x{:08X} "
                "| previousOverride=0x{:08X} | previousSkyRegion=0x{:08X} "
                "| pairState={} | pairChanged={} | lastPointerCleared={} "
                "| overrideChanged={} | skyRegionChanged={} | pct={:.4f} "
                "| pctValid={} | previousQuarter={} | currentQuarter={} "
                "| quarterChanged={} | secondsSincePairChange={:.1f} "
                "| sampleGapSeconds={:.1f} | reports={} "
                "| sampledObservationsOnly=true | transitionCompletion=unresolved "
                "| weatherCausality=unresolved | engineSelection=unresolved",
                aetherclime::SkyLifecycleStatusName(timeline.status),
                reading.currentWeatherFormId, reading.lastWeatherFormId,
                reading.overrideWeatherFormId, reading.skyRegionPointerFormId,
                timeline.previousCurrent, timeline.previousLast,
                timeline.previousOverride, timeline.previousSkyRegion,
                aetherclime::SkyWeatherPairStateName(timeline.pairState),
                timeline.pairChanged, timeline.lastPointerCleared,
                timeline.overrideChanged, timeline.skyRegionChanged,
                timeline.blendValid ? reading.currentWeatherPct : 0.0f,
                timeline.blendValid, timeline.previousBlendQuarter,
                timeline.blendQuarter, timeline.blendQuarterChanged,
                timeline.secondsSincePairChange, timeline.secondsSincePreviousSample,
                timeline.totalReports);
        }
        if (timeline.exhausted) {
            g_skyLifecycleLogBudgetAnnounced = true;
            SKSE::log::warn("Sky lifecycle: status=report-budget-exhausted "
                "| maxReports=48 | disabledUntilNextLoad=true");
        }
    }
    if (result.report) {
        if (result.status == aetherclime::WeatherCorrelationStatus::ResetUnavailable) {
            SKSE::log::info("Weather correlation: status=reset-unavailable "
                "| reason=interior-or-weather-unavailable | exteriorWeatherExcluded=true "
                "| engineSelection=unresolved");
        } else if (result.status == aetherclime::WeatherCorrelationStatus::ResetGeography) {
            SKSE::log::info("Weather correlation: status=reset-geography "
                "| reason=movement-discontinuity-or-incomplete-geometry "
                "| previousWeatherComparison=excluded | weatherCausality=unresolved");
        } else if (result.status == aetherclime::WeatherCorrelationStatus::ResetWorldspace) {
            SKSE::log::info("Weather correlation: status=reset-worldspace "
                "| previousWeatherComparison=excluded | engineSelection=unresolved");
        } else {
            SKSE::log::info(
                "Weather correlation: status={} | worldspace=0x{:08X} "
                "| current=0x{:08X} | lastPointer=0x{:08X} | overridePointer=0x{:08X} "
                "| skyRegionPointer=0x{:08X} | skyBlendPct={:.4f} | blendValid={} "
                "| previousCurrent=0x{:08X} | previousLastPointer=0x{:08X} "
                "| weatherIdsChanged={} | skyRegionPointerChanged={} "
                "| sampledBoundaryTransitions={} | secondsSinceCrossing={:.1f} "
                "| reports={} | sampledObservationsOnly=true "
                "| weatherCausality=unresolved | engineSelection=unresolved",
                aetherclime::WeatherCorrelationStatusName(result.status), reading.worldspaceFormId,
                reading.currentWeatherFormId, reading.lastWeatherFormId,
                reading.overrideWeatherFormId, reading.skyRegionPointerFormId,
                result.blendValueValid ? reading.currentWeatherPct : 0.0f,
                result.blendValueValid, result.previousCurrentWeatherFormId,
                result.previousLastWeatherFormId, result.weatherIdsChanged,
                result.skyRegionPointerChanged, result.boundaryTransitions,
                result.secondsSinceLastCrossing, result.totalReports);
        }
        // Alpha-12: correlate the raw RE::Sky region pointer against the
        // *already cached* CELL weather-region polygons, only on an existing
        // bounded Alpha-11 report. No extra form scan or update hook.
        if (reading.exterior && reading.skyAvailable &&
            result.status != aetherclime::WeatherCorrelationStatus::ResetGeography &&
            result.status != aetherclime::WeatherCorrelationStatus::ResetWorldspace) {
            std::optional<aetherclime::RegionPoint2> playerXY;
            if (position.exteriorWorldPosition) {
                const auto& raw = *position.exteriorWorldPosition;
                playerXY = aetherclime::RegionPoint2{raw.x, raw.y};
            }
            const bool sameCellCache = g_boundaryCellCache.initialized &&
                g_boundaryCellCache.complete &&
                g_boundaryCellCache.cellFormId == position.cellFormId &&
                g_boundaryCellCache.worldspaceFormId == position.worldspaceFormId;
            const auto auditStarted = std::chrono::steady_clock::now();
            const auto cachedCandidates = sameCellCache
                ? std::span<const aetherclime::SkyPointerCandidateGeometry>{g_boundaryCellCache.regions}
                : std::span<const aetherclime::SkyPointerCandidateGeometry>{};
            const auto evidence = aetherclime::AuditSkyPointerGeography(
                cachedCandidates, reading.skyRegionPointerFormId, playerXY,
                reading.worldspaceFormId, sameCellCache);
            const double auditMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - auditStarted).count();
            SKSE::log::info(
                "Sky pointer audit: source={} | cell=0x{:08X} | skyPointer=0x{:08X} "
                "| pointerRelation={} | pointerListed={} | pointerPriority={} "
                "| pointerOverride={} | pointerNearestEdge={:.1f} raw-units "
                "| edgeMeasured={} | inside={} | outside={} | uncertain={} "
                "| recordedPriorityLeader=0x{:08X} | leaderPriority={} "
                "| pointerMatchesPriorityLeader={} | candidates={} | cacheComplete={} "
                "| skyWeatherIdsChanged={} | skyPointerChanged={} "
                "| secondsSinceCrossing={:.1f} | auditCpuMs={:.4f} "
                "| excludesLogIO=true | geometryOnly=true | engineSelection=unresolved",
                aetherclime::WeatherCorrelationStatusName(result.status),
                position.cellFormId, reading.skyRegionPointerFormId,
                aetherclime::SkyPointerRelationName(evidence.pointerRelation),
                evidence.pointerListed, static_cast<unsigned>(evidence.pointerPriority),
                evidence.pointerOverride, evidence.pointerNearestRecordedEdgeRawUnits,
                evidence.pointerEdgeMeasured, evidence.insideCount,
                evidence.outsideCount, evidence.uncertainCount,
                evidence.priorityLeaderFormId,
                static_cast<unsigned>(evidence.priorityLeaderPriority),
                evidence.pointerMatchesPriorityLeader,
                evidence.totalCandidates, evidence.cacheComplete,
                result.weatherIdsChanged, result.skyRegionPointerChanged,
                result.secondsSinceLastCrossing, auditMs);
        }
    }
    if (result.exhausted) {
        g_weatherCorrelationSessionActive = false;
        SKSE::log::warn("Weather correlation: status=report-budget-exhausted "
            "| maxReports=64 | disabledUntilNextLoad=true");
    }
}

// Alpha-9 movement telemetry: at most one cheap position read per interval,
// no per-tick weather-record scans, and a strictly capped observation budget.
// This path is never called unless the experimental vtable hook is enabled.
void ObserveMovementProbe(RE::PlayerCharacter* player) {
    if (!player || !g_movementProbe || !g_movementProbeSessionActive) return;
    auto* cell = player->GetParentCell();
    const auto* worldspace = player->GetWorldspace();
    aetherclime::MovementPosition position;
    position.cellFormId = cell ? cell->GetFormID() : 0;
    position.worldspaceFormId = worldspace ? worldspace->GetFormID() : 0;
    if (cell && cell->IsExteriorCell() && worldspace) {
        const auto& raw = player->GetPosition();
        if (std::isfinite(raw.x) && std::isfinite(raw.y) && std::isfinite(raw.z)) {
            position.exteriorWorldPosition = aetherclime::RawPosition{raw.x, raw.y, raw.z};
        }
    }
    const auto decision = g_movementProbe->Observe(position);
    if (decision.report) {
        if (position.exteriorWorldPosition) {
            const auto& xyz = *position.exteriorWorldPosition;
            SKSE::log::info(
                "Movement probe: trigger=tick | reason={} | worldspace=0x{:08X} "
                "| cell=0x{:08X} | positionStatus=exterior-world-coordinates "
                "| rawXYZ=({:.1f},{:.1f},{:.1f}) | distanceSinceReport={:.1f} raw-units "
                "| samples={} | reports={} | weatherSelection=unresolved",
                aetherclime::MovementReasonName(decision.reason),
                position.worldspaceFormId, position.cellFormId,
                xyz.x, xyz.y, xyz.z, decision.distanceRawUnits,
                g_movementProbe->Samples(), g_movementProbe->Reports());
        } else {
            SKSE::log::info(
                "Movement probe: trigger=tick | reason={} | worldspace=0x{:08X} "
                "| cell=0x{:08X} | positionStatus=interior-or-unavailable "
                "| rawXYZ=(excluded) | samples={} | reports={} | weatherSelection=unresolved",
                aetherclime::MovementReasonName(decision.reason),
                position.worldspaceFormId, position.cellFormId,
                g_movementProbe->Samples(), g_movementProbe->Reports());
        }
    }
    // Alpha-10 polygon work occurs only on Alpha-9's bounded position
    // REPORTS (never every frame, never on each 2-second sample).
    BoundaryObservationOutcome boundaryOutcome;
    if (decision.report) boundaryOutcome = ObserveBoundaryProbe(cell, position, decision.reason);
    // Alpha-11 reads a few scalar Sky fields once per scheduled 2-second
    // movement sample, even if stationary, so 10/30s follow-ups can arrive.
    // No region scans, polygon checks, or weather writes on this path.
    ObserveWeatherCorrelation(position, boundaryOutcome);
    if (decision.exhausted) {
        g_boundaryProbeSessionActive = false;
        g_weatherCorrelationSessionActive = false;
        g_boundaryCellCache = {};
        if (g_boundaryTransitionProbe) g_boundaryTransitionProbe->Reset();
        g_movementProbeSessionActive = false;
        SKSE::log::info(
            "Movement probe: status=budget-exhausted | samples={} | reports={} "
            "| automaticallyDisabled=true | resetOnNextLoad=true",
            g_movementProbe->Samples(), g_movementProbe->Reports());
    }
}

// 0xAD is PlayerCharacter::Update(float) for documented SE/AE layouts.
// This remains experimental and *disabled by default*. Its initial HexRim
// test passed, but longer-term compatibility is unproven. Never install on
// VR, whose virtual slots differ.
void PlayerUpdate(RE::PlayerCharacter* player, float deltaSeconds);
REL::Relocation<decltype(PlayerUpdate)> g_originalPlayerUpdate;

void PlayerUpdate(RE::PlayerCharacter* player, float deltaSeconds) {
    // Preserve Skyrim's original player update in all cases.
    g_originalPlayerUpdate(player, deltaSeconds);

    // The hook itself is never installed when BOTH experimental modes are
    // disabled. When enabled, only one clock check is performed per frame.
    if (!g_liveHookInstalled || !g_liveSessionActive || !g_climate) return;
    const bool liveDue = g_climate->Settings().experimentalLiveMonitor;
    const bool moveDue = g_movementProbeSessionActive && g_movementProbe;
    if (!liveDue && !moveDue) return;

    const auto now = std::chrono::steady_clock::now();
    const bool observeLive = liveDue && now >= g_nextLiveObservation;
    const bool observeMovement = moveDue && now >= g_nextMovementObservation;
    if (!observeLive && !observeMovement) return;

    if (observeLive) {
        const auto interval = std::chrono::duration<double>(
            g_climate->Settings().liveMonitorIntervalSeconds);
        g_nextLiveObservation = now +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
    }
    if (observeMovement) {
        const auto interval = std::chrono::duration<double>(
            g_climate->Settings().movementProbeIntervalSeconds);
        g_nextMovementObservation = now +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
    }
    if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) return;

    try {
        if (observeLive) { ObserveMonitorEvent("tick"); ObserveHud("experimental-monitor-tick"); }
        if (observeMovement) ObserveMovementProbe(player);
    } catch (const std::exception& e) {
        SKSE::log::error("Experimental observation disabled after exception: {}", e.what());
        g_liveSessionActive = false;
        g_movementProbeSessionActive = false;
        g_boundaryProbeSessionActive = false;
        g_weatherCorrelationSessionActive = false;
    } catch (...) {
        SKSE::log::error("Experimental observation disabled after unknown exception.");
        g_liveSessionActive = false;
        g_movementProbeSessionActive = false;
        g_boundaryProbeSessionActive = false;
        g_weatherCorrelationSessionActive = false;
    }
}

void TryInstallLiveHook() {
    if (!g_climate) return;
    const auto& settings = g_climate->Settings();
    if (!settings.experimentalLiveMonitor && !settings.experimentalMovementProbe) {
        SKSE::log::info("Experimental live monitor OFF; movement probe OFF (defaults). "
            "No PlayerCharacter update hook installed; load/save probes remain active.");
        return;
    }
    if (REL::Module::IsVR()) {
        SKSE::log::warn("Experimental live monitor requires Skyrim SE/AE; refusing VR vtable hook.");
        return;
    }
    if (g_liveHookInstalled) return;
    try {
        // vtable[0] is the primary PlayerCharacter table, not the derived class's
        // additional interface vtables. The original function is chained.
        REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_PlayerCharacter[0]};
        g_originalPlayerUpdate = vtable.write_vfunc(0xAD, PlayerUpdate);
        g_liveHookInstalled = true;
        SKSE::log::warn("EXPERIMENTAL PlayerCharacter update hook installed: liveMonitor={} "
            "| movementProbe={} | boundaryProbe={} | movementInterval={:.1f}s "
            "| movementMaxSamples={} | movementMaxReports={} | weatherCorrelation={} | readOnly=true",
            settings.experimentalLiveMonitor, settings.experimentalMovementProbe,
            settings.experimentalBoundaryProbe,
            settings.movementProbeIntervalSeconds, settings.movementProbeMaxSamples,
            settings.movementProbeMaxReports, settings.experimentalWeatherCorrelationProbe);
    } catch (const std::exception& e) {
        SKSE::log::error("Experimental live monitor hook installation failed: {}. Load/save probes remain active.", e.what());
    }
}

void ResetMonitorSession() {
    g_apiConsumerProbeAttempted=false;
    g_hud.Reset();
    g_hudFailed=false;
    g_deferredSessionEpoch.fetch_add(1, std::memory_order_relaxed);
    // Do not reset the menu pending flag or g_deferredPendingTasks here: queued tasks must
    // release their own slots when they observe the changed epoch.
    g_exteriorWeather.Clear(); // Do not inherit exterior weather from a different save/new game.
    g_climateEventGate.Reset();
    g_outdoorChangeGate.Reset();
    g_regionalContinuity.Reset();
    g_travelDiscontinuity.Reset();
    g_fallbackTransitionAudit.Reset();
    g_temperatureHandoffAudit.Reset();
    g_temperatureContinuityPreview.Reset();
    g_temperatureHandoffCoverage.Reset();
    g_geographicGapReport.Reset();
    g_atlasProposalReview.Reset();
    g_nativeShadowCoverage={}; g_nativeShadowLines=0; g_nativeShadowSummaries=0;
    g_biomePreviewLines=g_biomePreviewSummaries=0; g_biomeSamples=g_biomeAvailable=g_biomeOverlap=g_biomeGap=0;
    g_temperatureHandoffReportBudget.Reset();
    g_temperatureHandoffSummaryReports = 0;
    g_untrustedTransitionReports = 0;
    g_outdoorChangeLogCount = 0;
    g_eventReportCount = 0;
    g_earlyEventReportCount = 0;
    g_liveSessionActive = false;
    g_monitorEpoch = std::chrono::steady_clock::now();
    g_nextLiveObservation = g_monitorEpoch;
    g_nextMovementObservation = g_monitorEpoch;
    g_movementProbeSessionActive = false;
    g_boundaryProbeSessionActive = false;
    g_weatherCorrelationSessionActive = false;
    g_boundaryTransitionLogCount = 0;
    g_boundaryCellCache = {};
    if (g_boundaryTransitionProbe) g_boundaryTransitionProbe->Reset();
    if (g_weatherCorrelationProbe) g_weatherCorrelationProbe->Reset();
    if (g_skyLifecycleProbe) g_skyLifecycleProbe->Reset();
    if (g_regionTimingProbe) g_regionTimingProbe->Reset();
    if (g_regionSpecificProbe) g_regionSpecificProbe->Reset();
    g_skyLifecycleLogBudgetAnnounced = false;
    g_regionTimingBudgetAnnounced = false;
    g_regionSpecificBudgetAnnounced = false;
    if (g_monitor) g_monitor->Reset();
    if (g_movementProbe) g_movementProbe->Reset();
}

// Alpha-50: tiny bounded event-time audit of secondary outdoor fallback
// handoffs. Enabled only when the existing cell/location event subscriptions
// are active (HUD or eventClimate). No per-frame sampling and no extra hook.
void ObserveFallbackTransition(std::string_view trigger) {
    if(g_fallbackTransitionAudit.Reports()>=aetherclime::FallbackTransitionAudit::kMaxReports) return;
    try {
        const auto state=ReadCurrentEnvironmentSnapshot(false);
        if(!state) return;
        aetherclime::FallbackTransitionSample sample;
        sample.interior=state->interior;
        sample.basis=state->outdoorEstimateBasis;
        sample.estimateC=state->estimatedOutdoorAirC;
        sample.cellFormId=state->local.cellFormId;
        // Only collect coordinates for secondary outdoor estimates.
        if(!state->interior && state->estimatedOutdoorAirC) {
            const auto evidence=CollectPlayerClimateEvidence(RE::PlayerCharacter::GetSingleton());
            if(evidence.exteriorPosition) {
                sample.worldspaceFormId=evidence.exteriorPosition->worldspaceFormId;
                sample.xRaw=evidence.exteriorPosition->xRaw;
                sample.yRaw=evidence.exteriorPosition->yRaw;
            }
        }
        const auto report=g_fallbackTransitionAudit.Observe(sample);
        if(!report.Report()) return;
        SKSE::log::info("Climate fallback transition: trigger={} | reason={} | report={}/{} "
            "| cell=0x{:08X} | worldspace=0x{:08X} | xRaw={:.1f} | yRaw={:.1f} "
            "| hasExteriorPosition={} | basis={} | estimateC={:.2f} | legacyAirC={:.2f} "
            "| regionTrusted={} | reference='{}' | referenceDistanceRaw={:.0f} "
            "| referenceInfluence={:.3f} | referenceCount={} | readOnly=true "
            "| eventDriven=true | perFramePolling=false",
            trigger,aetherclime::FallbackReportReasonName(report.reason),
            report.reportNumber,aetherclime::FallbackTransitionAudit::kMaxReports,
            sample.cellFormId,sample.worldspaceFormId,sample.xRaw.value_or(0.0),
            sample.yRaw.value_or(0.0),sample.xRaw.has_value()&&sample.yRaw.has_value(),
            aetherclime::OutdoorEstimateBasisName(sample.basis),sample.estimateC.value_or(0.0),
            state->outdoorAirC,state->trustedRegion,
            state->outdoorReferenceLabel.empty()?"(none)":state->outdoorReferenceLabel,
            state->outdoorReferenceDistanceRaw,state->outdoorReferenceInfluence,
            state->outdoorReferenceCount);
    } catch(const std::exception& e) {
        SKSE::log::warn("Climate fallback transition audit failed safely: {}",e.what());
    } catch(...) {
        SKSE::log::warn("Climate fallback transition audit failed safely: unknown exception");
    }
}

// Only called from deferred event tasks after their expected target becomes
// the player's CURRENT resolved cell/location. The callback records IDs only;
// no borrowed engine event pointers survive the game-thread task handoff.
void ObserveClimateTransition(std::string_view trigger) {
    if (!g_liveSessionActive || !ClimateEventSamplingEnabled()) return;
    ObserveFallbackTransition(trigger);
    // HUD-only listeners must not enable the separate climate observation/cache path.
    if (!GetClimate().Settings().eventClimateEnabled) { ObserveHud(trigger); return; }
    const auto* player = RE::PlayerCharacter::GetSingleton();
    const auto evidence = CollectPlayerClimateEvidence(player);
    const auto sample = Snapshot();
    LogTemperatureHandoff(trigger);
    const auto region = aetherclime::ResolveClimateRegion(GetClimate().Settings(), evidence.Context());
    if (GetClimate().Settings().geographicContinuityEnabled) {
        (void)g_regionalContinuity.ObserveVerified(region, evidence.Context(), sample.date);
    }
    bool trusted = region.matched();
    if (evidence.interior && !trusted && player) {
        const auto* cell = player->GetParentCell();
        const auto* location = player->GetCurrentLocation();
        const auto indoor = aetherclime::ResolveInteriorClimateContext(
            GetClimate().Settings(), evidence.hasCell, true,
            location && location->GetName() ? location->GetName() : "",
            cell && cell->GetName() ? cell->GetName() : "",
            region.regionKey, false);
        trusted = indoor.eligible && indoor.regionKey == sample.region;
    }
    const auto* cell = player ? player->GetParentCell() : nullptr;
    const auto* sky = RE::Sky::GetSingleton();
    const auto status = g_climateEventGate.Evaluate({
        GetClimate().Settings().eventClimateEnabled, evidence.hasCell, evidence.interior,
        cell ? cell->GetFormID() : 0, sample.region, trusted,
        sky && sky->currentWeather ? sky->currentWeather->GetFormID() : 0,
        sample.date
    });
    if (status != aetherclime::ClimateEventGateStatus::Accept) {
        // Untrusted cells were historically invisible to the event diagnostic
        // gate. Report a bounded audit now, without capturing exterior Sky.
        // This is particularly useful in unnamed northern wilderness gaps.
        if (status == aetherclime::ClimateEventGateStatus::UntrustedRegion &&
            g_untrustedTransitionReports < kUntrustedTransitionReportLimit) {
            ++g_untrustedTransitionReports;
            LogRegionalContinuity(trigger);
            LogEnvironmentalSnapshot(trigger);
        }
        ObserveHud(trigger); // independent of diagnostic trust and report budget
        return;
    }
    // Only the first bounded number of event reports per loaded session is
    // logged; the pure gate and exterior context can still refresh afterwards.
    const bool report = g_eventReportCount < kEventReportLimit;
    if (report) {
        ++g_eventReportCount;
        SKSE::log::info(
            "Event climate context: trigger={} | status={} | cell=0x{:08X} "
            "| cellType={} | region='{}' | trusted={} | skyWeather=0x{:08X} "
            "| report={}/{} | eventDriven=true | updateHook=false | perFramePolling=false",
            trigger, aetherclime::ClimateEventGateStatusName(status),
            cell ? cell->GetFormID() : 0, evidence.interior ? "interior" : "exterior",
            sample.region, trusted,
            sky && sky->currentWeather ? sky->currentWeather->GetFormID() : 0,
            g_eventReportCount, kEventReportLimit);
    }
    // Always update the in-memory exterior observation for trusted outdoor
    // cells, even when the diagnostic print budget has been exhausted.
    // A stale interior Sky reading can NEVER be captured as exterior weather.
    if (evidence.hasCell && !evidence.interior) {
        const auto* worldspace = player ? player->GetWorldspace() : nullptr;
        if (worldspace && sky && sky->currentWeather && trusted) {
            const bool captured = g_exteriorWeather.CaptureExterior(
                true, true, sample.region, sample.weather,
                sky->currentWeather->GetFormID(), worldspace->GetFormID(), sample.date);
            if (captured) ObserveOutdoorChange(trigger, sample.region, sky->currentWeather->GetFormID());
        }
    }
    ObserveHud(trigger); // cache capture precedes snapshot; logging cap is irrelevant
    if (report) {
        // Log details on accepted transitions; avoid the expensive polygon
        // weather-region diagnostics used by the older manual-save probes.
        const auto assessment = ExteriorWeatherForSample(Snapshot(), evidence);
        const auto& obs = assessment.observation;
        SKSE::log::info(
            "Event exterior weather: trigger={} | status={} | usable={} "
            "| region='{}' | capturedRegion='{}' | capturedWeather=0x{:08X} "
            "| source={} | fromExteriorCellOnly=true | eventDriven=true",
            trigger, aetherclime::ExteriorWeatherStatusName(assessment.status),
            assessment.usable, sample.region, obs ? obs->regionKey : "(none)",
            obs ? obs->weatherFormId : 0,
            OutdoorWeatherSource(assessment, evidence.interior));
        LogBuildingClassification(trigger);
        LogEnvironmentalSnapshot(trigger);
        if (evidence.interior) LogIndoorClimateContext(trigger);
    }
}

// Native player-cell and actor-location events may fire BEFORE the player has
// adopted the target cell/location. Queue a bounded follow-up on SKSE's game
// task interface, then verify the CURRENT player state (not stale pointers).
// Each task carries only copied scalar evidence and a session epoch; no engine
// form/event pointers survive the original callback.
struct DeferredClimateRequest {
    aetherclime::DeferredClimateTarget target;
    std::uint32_t expectedId{0};
    std::uint64_t sessionEpoch{0};
    unsigned checkNumber{1};
};

std::string_view DeferredTrigger(aetherclime::DeferredClimateTarget target) noexcept {
    return target == aetherclime::DeferredClimateTarget::Cell
        ? "player-cell-enter-deferred" : "player-location-change-deferred";
}

void ReleaseDeferredSlot() noexcept {
    g_deferredPendingTasks.fetch_sub(1, std::memory_order_relaxed);
}

void ProcessDeferredClimateRequest(DeferredClimateRequest request);

bool PostDeferredClimateTask(const DeferredClimateRequest& request) {
    const auto* tasks = SKSE::GetTaskInterface();
    if (!tasks) return false;
    tasks->AddTask([request]() { ProcessDeferredClimateRequest(request); });
    return true;
}

void ProcessDeferredClimateRequest(DeferredClimateRequest request) {
    // SKSE executes queued tasks on the game thread. Never dereference a
    // borrowed event pointer or use data retained across game loads.
    if (request.sessionEpoch != g_deferredSessionEpoch.load(std::memory_order_relaxed)) {
        ReleaseDeferredSlot();
        return;
    }
    try {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        const auto* cell = player ? player->GetParentCell() : nullptr;
        const auto* location = player ? player->GetCurrentLocation() : nullptr;
        const auto status = aetherclime::EvaluateDeferredClimate({
            g_liveSessionActive && ClimateEventSamplingEnabled(),
            player != nullptr,
            cell ? cell->GetFormID() : 0,
            location ? location->GetFormID() : 0,
            request.target, request.expectedId, request.checkNumber
        });
        if (status == aetherclime::DeferredClimateStatus::Retry) {
            // Hard stop after kMaxDeferredClimateChecks scheduled evaluations;
            // no timers, game Update hook, or permanent polling loop.
            ++request.checkNumber;
            if (PostDeferredClimateTask(request)) return;
        }
        if (status == aetherclime::DeferredClimateStatus::Ready) {
            if (g_earlyEventReportCount < 16) {
                ++g_earlyEventReportCount;
                SKSE::log::info(
                    "Event climate timing: source={} | status=deferred-ready "
                    "| checks={} | currentCell=0x{:08X} | targetId=0x{:08X} "
                    "| queuedTask=true | perFramePolling=false",
                    DeferredTrigger(request.target), request.checkNumber,
                    cell ? cell->GetFormID() : 0, request.expectedId);
            }
            ObserveClimateTransition(DeferredTrigger(request.target));
        } else if (status == aetherclime::DeferredClimateStatus::Exhausted) {
            if (g_earlyEventReportCount < 16) {
                ++g_earlyEventReportCount;
                SKSE::log::info(
                    "Event climate timing: source={} | status=deferred-retry-budget-exhausted "
                    "| checks={} | currentCell=0x{:08X} | targetId=0x{:08X} "
                    "| ignoredSafely=true | capture=false | perFramePolling=false",
                    DeferredTrigger(request.target), request.checkNumber,
                    cell ? cell->GetFormID() : 0, request.expectedId);
            }
        }
    } catch (const std::exception& e) {
        SKSE::log::warn("Deferred climate event skipped safely: {}", e.what());
    } catch (...) {
        SKSE::log::warn("Deferred climate event skipped after unknown exception.");
    }
    ReleaseDeferredSlot();
}

void QueueDeferredClimateRequest(aetherclime::DeferredClimateTarget target, std::uint32_t expectedId) {
    // Bounded in-flight queue even if the player crosses many cells rapidly.
    auto pending = g_deferredPendingTasks.load(std::memory_order_relaxed);
    while (pending < kMaxConcurrentDeferredTasks) {
        if (g_deferredPendingTasks.compare_exchange_weak(pending, pending + 1,
                std::memory_order_relaxed)) {
            const DeferredClimateRequest request{target, expectedId,
                g_deferredSessionEpoch.load(std::memory_order_relaxed), 1};
            try {
                if (PostDeferredClimateTask(request)) return;
            } catch (const std::exception& e) {
                SKSE::log::warn("Unable to schedule deferred climate check: {}", e.what());
            }
            ReleaseDeferredSlot();
            return;
        }
    }
    if (g_earlyEventReportCount < 16) {
        ++g_earlyEventReportCount;
        SKSE::log::info("Event climate timing: status=deferred-queue-full "
            "| maxPending={} | ignoredSafely=true | capture=false", kMaxConcurrentDeferredTasks);
    }
}

class PlayerCellClimateSink final : public RE::BSTEventSink<RE::BGSActorCellEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BGSActorCellEvent* event,
        RE::BSTEventSource<RE::BGSActorCellEvent>*) override {
        if (!event || event->flags != RE::BGSActorCellEvent::CellFlag::kEnter || !g_liveSessionActive)
            return RE::BSEventNotifyControl::kContinue;
        const auto* player = RE::PlayerCharacter::GetSingleton();
        auto actor = event->actor.get();
        if (!player || !actor || actor.get() != player)
            return RE::BSEventNotifyControl::kContinue;
        QueueDeferredClimateRequest(aetherclime::DeferredClimateTarget::Cell, event->cellID);
        return RE::BSEventNotifyControl::kContinue;
    }
};
class PlayerLocationClimateSink final : public RE::BSTEventSink<RE::TESActorLocationChangeEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESActorLocationChangeEvent* event,
        RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override {
        if (!event || !g_liveSessionActive) return RE::BSEventNotifyControl::kContinue;
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || event->actor.get() != player)
            return RE::BSEventNotifyControl::kContinue;
        QueueDeferredClimateRequest(aetherclime::DeferredClimateTarget::Location,
            event->newLoc ? event->newLoc->GetFormID() : 0);
        return RE::BSEventNotifyControl::kContinue;
    }
};
PlayerCellClimateSink g_playerCellClimateSink;
PlayerLocationClimateSink g_playerLocationClimateSink;

// A close event itself may precede UI unpausing. Schedule at most one
// one-shot task; if the UI is still not ready, do not retry or queue text.
// The task only holds a static trigger label and the existing session epoch.
class HudMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if(!event||event->opening)return RE::BSEventNotifyControl::kContinue;
        const auto* name=event->menuName.c_str();
        if(!name)return RE::BSEventNotifyControl::kContinue;
        const auto trigger=aetherclime::HudMenuCloseTrigger(name);
        if(trigger.empty())return RE::BSEventNotifyControl::kContinue;
        bool expected=false;
        if(!g_hudMenuTaskPending.compare_exchange_strong(expected,true,std::memory_order_relaxed))
            return RE::BSEventNotifyControl::kContinue;
        const auto epoch=g_deferredSessionEpoch.load(std::memory_order_relaxed);
        try {
            if(auto* tasks=SKSE::GetTaskInterface()) {
                tasks->AddTask([epoch,trigger]() {
                    if(epoch==g_deferredSessionEpoch.load(std::memory_order_relaxed))ObserveHud(trigger);
                    g_hudMenuTaskPending.store(false,std::memory_order_relaxed);
                });
                return RE::BSEventNotifyControl::kContinue;
            }
        } catch(const std::exception& e) {
            SKSE::log::warn("HUD menu-close task skipped: {}",e.what());
        } catch(...) {
            SKSE::log::warn("HUD menu-close task skipped after unknown exception.");
        }
        g_hudMenuTaskPending.store(false,std::memory_order_relaxed);
        return RE::BSEventNotifyControl::kContinue;
    }
};
HudMenuSink g_hudMenuSink;
void TryRegisterHudMenuSink() {
    if(!HudEnabled()||!g_climate->Settings().hud.sampleOnMenuClose||g_hudMenuSinkRegistered)return;
    try {
        if(auto* ui=RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_hudMenuSink);
            g_hudMenuSinkRegistered=true;
            SKSE::log::info("HUD menu listener: registered=true | onePendingTask=true | noPolling=true");
        }
    } catch(const std::exception& e) {
        SKSE::log::warn("HUD menu listener not registered: {}",e.what());
    } catch(...) {
        SKSE::log::warn("HUD menu listener not registered after unknown exception.");
    }
}

void TryRegisterClimateEventSinks() {
    if (!ClimateEventSamplingEnabled()) return;
    TryRegisterHudMenuSink();
    if (!g_locationEventSinkRegistered) {
        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESActorLocationChangeEvent>(&g_playerLocationClimateSink);
            g_locationEventSinkRegistered = true;
        }
    }
    if (!g_cellEventSinkRegistered) {
        // CommonLibSSE-NG multi-runtime PlayerCharacter is not publicly derived
        // from BSTEventSource<BGSActorCellEvent> in AE builds. Use its accessor:
        // the source is an embedded member, not a safe base-class static_cast.
        // CommonLibSSE-NG 3.7.0's accessor has an unresolved Skyrim VR offset
        // concern; do not subscribe the cell sink on VR until that is validated.
        if (REL::Module::IsVR()) {
            SKSE::log::warn("Event climate player-cell sink skipped on Skyrim VR: "
                "BGSActorCellEvent source offset unverified; location listener only.");
        } else if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (auto* source = player->AsBGSActorCellEventSource()) {
                source->AddEventSink(&g_playerCellClimateSink);
                g_cellEventSinkRegistered = true;
            }
        }
    }
    SKSE::log::info(
        "Event climate subscriptions: enabled={} | hudEnabled={} | cellSink={} | locationSink={} "
        "| playerUpdateHookInstalled={} | listenerOnly=true | noWeatherWrites=true",
        GetClimate().Settings().eventClimateEnabled,HudEnabled(),g_cellEventSinkRegistered, g_locationEventSinkRegistered, g_liveHookInstalled);
}

void InitLog() {
    if (auto directory = SKSE::log::log_directory()) {
        // Recent spdlog versions accept their filename_t, not a filesystem::path.
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            (*directory / "Aetherclime.log").string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_level(spdlog::level::info);
        // Flush each informational message so a clean game exit is not
        // required to inspect a fresh SKSE startup or configuration failure.
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("Aetherclime logging initialized.");
    }
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (!message) return;
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        SKSE::log::info("Aetherclime milestone: {}",kMilestoneMarker);
        bool configAccepted=false;
        try {
            g_climate = std::make_unique<aetherclime::Climate>(
                aetherclime::LoadOrCreateConfig("Data/SKSE/Plugins/Aetherclime/config.json"));
            g_monitor = std::make_unique<aetherclime::Monitor>(aetherclime::MonitorOptions{
                .minimumReportIntervalSeconds = g_climate->Settings().liveMonitorIntervalSeconds
            });
            if (g_climate->Settings().experimentalMovementProbe) {
                const auto& c = g_climate->Settings();
                g_movementProbe = std::make_unique<aetherclime::MovementProbe>(
                    aetherclime::MovementProbeOptions{
                        .minimumDistanceRawUnits = c.movementProbeMinDistanceRawUnits,
                        .maxSamples = c.movementProbeMaxSamples,
                        .maxReports = c.movementProbeMaxReports
                    });
            } else {
                g_movementProbe.reset();
            }
            const auto& settings = g_climate->Settings();
            if (settings.experimentalBoundaryProbe && settings.experimentalMovementProbe) {
                g_boundaryTransitionProbe = std::make_unique<aetherclime::RegionTransitionProbe>();
            } else {
                g_boundaryTransitionProbe.reset();
                if (settings.experimentalBoundaryProbe) {
                    SKSE::log::warn("Boundary probe requested without movement probe; "
                        "enable experimentalMovementProbe as well. Boundary probe remains OFF.");
                }
            }
            if (settings.experimentalWeatherCorrelationProbe &&
                settings.experimentalMovementProbe && settings.experimentalBoundaryProbe) {
                g_weatherCorrelationProbe = std::make_unique<aetherclime::WeatherCorrelationProbe>();
                g_skyLifecycleProbe = std::make_unique<aetherclime::SkyTransitionLifecycleProbe>();
                g_regionTimingProbe = std::make_unique<aetherclime::RegionPointerTimingProbe>();
                g_regionSpecificProbe = std::make_unique<aetherclime::RegionSpecificCorrelationProbe>();
            } else {
                g_weatherCorrelationProbe.reset();
                g_skyLifecycleProbe.reset();
                g_regionTimingProbe.reset();
                g_regionSpecificProbe.reset();
                if (settings.experimentalWeatherCorrelationProbe) {
                    SKSE::log::warn("Weather correlation requested without BOTH movement and boundary probes; "
                        "enable those two probes too. Weather correlation remains OFF.");
                }
            }
            SKSE::log::info("Aetherclime 0.5.0 RC1 initialized "
                "(bounded thermal shadow preview enabled={}, fadeDistanceRawUnits={:.1f}, "
                "maxCompensationC={:.2f}, maxLifetimeGameHours={:.3f}, "
                "native region atlas shadow available, geographic gap review enabled, "
                "raw temperatures and public V1 snapshot unchanged). "
                "No forms or weather records modified.",
                settings.temperatureContinuity.previewEnabled,
                settings.temperatureContinuity.fadeDistanceRawUnits,
                settings.temperatureContinuity.maxCompensationC,
                settings.temperatureContinuity.maxLifetimeGameHours);
            configAccepted=true;
        } catch (const std::exception& e) {
            SKSE::log::error("Config rejected: {}. Using safe built-in defaults, preserving existing file.", e.what());
            auto safe=aetherclime::DefaultConfig();
            safe.hud.mode=aetherclime::HudMode::Off; // reject invalid config without registering HUD listeners
            g_climate = std::make_unique<aetherclime::Climate>(std::move(safe));
            g_monitor = std::make_unique<aetherclime::Monitor>();
            g_movementProbe.reset(); // Never hook after an invalid opt-in config.
            g_boundaryTransitionProbe.reset();
            g_weatherCorrelationProbe.reset();
            g_skyLifecycleProbe.reset();
            g_regionTimingProbe.reset();
            g_regionSpecificProbe.reset();
        }
        const auto& policySettings=g_climate->Settings();
        const auto& hud=policySettings.hud;
        SKSE::log::info("HUD settings: mode={} | unit={} | cooldownSeconds={:.1f} | loadGraceSeconds={:.1f} "
            "| sampleIntervalSeconds={:.1f} | temperatureDeltaC={:.1f} | heartbeatSeconds={:.1f} "
            "| sampleOnMenuClose={} | showOnFirstObservation={} | showProvisionalEstimates={} | debugInitializationMessage={} "
            "| configAccepted={} | runtimeSupported={} | installsUpdateHook=false | displayDuration=game-managed",
            aetherclime::HudModeName(hud.mode),aetherclime::HudTemperatureUnitName(hud.temperatureUnit),hud.cooldownSeconds,hud.loadGraceSeconds,
            hud.sampleIntervalSeconds,hud.temperatureDeltaC,hud.heartbeatSeconds,hud.sampleOnMenuClose,
            hud.showOnFirstObservation,hud.showProvisionalEstimates,hud.debugInitializationMessage,configAccepted,!REL::Module::IsVR());
        SKSE::log::info("Climate model policy: published=regional-v1 | biome=experimental-preview-only "
            "| automaticPromotion=false | biomePreviewEnabled={} | spatialGradientEnabled={} | configAccepted={}",
            policySettings.biomeClimate.enabled,policySettings.biomeClimate.spatialGradientEnabled,configAccepted);
        try { BuildNativeRegionAtlas(g_climate->Settings()); }
        catch (const std::exception& e) { g_nativeAtlas.Build({},false); SKSE::log::warn("Native atlas census failed safely: {}",e.what()); }
        TryInstallLiveHook();
    } else if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
        ResetMonitorSession();
    } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
        ResetMonitorSession();
        try { LogExteriorWeatherContext("load"); }
        catch (const std::exception& e) {
            SKSE::log::error("Exterior weather load probe failed safely: {}", e.what());
        }
        LogClimateSnapshot();
        try { LogTemperatureHandoff("load"); }
        catch (const std::exception& e) { SKSE::log::warn("Temperature handoff load audit skipped: {}", e.what()); }
        ObserveMonitorEvent("load");
        g_liveSessionActive = true;
        ObserveHud("load"); // obeys grace and UI gate; never queues stale text
        try { TryRegisterClimateEventSinks(); }
        catch (const std::exception& e) { SKSE::log::warn("Event climate registration failed safely: {}", e.what()); }
        g_movementProbeSessionActive = g_movementProbe && g_liveHookInstalled;
        g_boundaryProbeSessionActive = g_boundaryTransitionProbe && g_movementProbeSessionActive;
        g_weatherCorrelationSessionActive = g_weatherCorrelationProbe && g_boundaryProbeSessionActive;
        const auto now = std::chrono::steady_clock::now();
        g_nextLiveObservation = now +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(g_climate ? g_climate->Settings().liveMonitorIntervalSeconds : 10.0));
        g_nextMovementObservation = now +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(g_climate ? g_climate->Settings().movementProbeIntervalSeconds : 2.0));
    } else if (message->type == SKSE::MessagingInterface::kSaveGame) {
        try { LogExteriorWeatherContext("save"); }
        catch (const std::exception& e) {
            SKSE::log::error("Exterior weather save probe failed safely: {}", e.what());
        }
        ObserveMonitorEvent("save");
        ObserveHud("save");
        LogHudSummary();
        // A manual save is a convenient one-shot geographic probe even if
        // the change detector has no reason to emit a new monitor line.
        try {
            const auto started = std::chrono::steady_clock::now();
            LogClimateRegionResolution("save");
            LogRegionalContinuity("save");
            LogTemperatureHandoff("save");
            LogIndoorClimateContext("save");
            LogBuildingClassification("save");
            LogEnvironmentalSnapshot("save");
            LogExternalApiConsumerProbe();
            LogGeographyDiagnostics();
            LogWeatherRegionDiagnostics();
            const auto durationMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            SKSE::log::info("Performance audit: trigger=save | scope=geography-and-weather-diagnostics "
                "| inclusiveElapsedMs={:.3f} | includesSynchronousLogIO=true | perFramePolling=false",
                durationMs);
        } catch (const std::exception& e) {
            SKSE::log::error("Geography save probe failed safely: {}", e.what());
        }
    } else if (message->type == SKSE::MessagingInterface::kNewGame) {
        ResetMonitorSession();
        g_liveSessionActive = true;
        try { TryRegisterClimateEventSinks(); }
        catch (const std::exception& e) { SKSE::log::warn("Event climate registration failed safely: {}", e.what()); }
        g_movementProbeSessionActive = g_movementProbe && g_liveHookInstalled;
        g_boundaryProbeSessionActive = g_boundaryTransitionProbe && g_movementProbeSessionActive;
        g_weatherCorrelationSessionActive = g_weatherCorrelationProbe && g_boundaryProbeSessionActive;
        const auto now = std::chrono::steady_clock::now();
        if (g_climate) {
            g_nextMovementObservation = now +
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(g_climate->Settings().movementProbeIntervalSeconds));
        }
        SKSE::log::info("New game started; load snapshot deferred. Experimental observations require explicit opt-in.");
    }
}
} // namespace

// v0.1 C ABI. Call only from Skyrim's game thread. Consumers must query each
// time; results are not stored in a stale global. Versioned header follows.
extern "C" __declspec(dllexport) int Aetherclime_ApiVersion() { return 1; }
extern "C" __declspec(dllexport) int Aetherclime_GetSeason() {
    return static_cast<int>(GetClimate().GetSeason(Snapshot().date).official);
}
extern "C" __declspec(dllexport) double Aetherclime_GetSeasonProgress() {
    return GetClimate().GetSeason(Snapshot().date).progress;
}
extern "C" __declspec(dllexport) double Aetherclime_GetAmbientC() {
    return GetClimate().Calculate(Snapshot()).airC;
}
extern "C" __declspec(dllexport) double Aetherclime_GetFeelsLikeC() {
    return GetClimate().Calculate(Snapshot()).feelsLikeC;
}

// Alpha-25 preview C ABI: fixed-size versioned *value* snapshot. The only
// engine access happens synchronously inside ReadCurrentEnvironmentSnapshot.
// No global event publication or cross-thread data ownership is implied.
extern "C" __declspec(dllexport) std::int32_t Aetherclime_GetEnvironmentSnapshotV1(
    AetherclimeEnvironmentSnapshotV1* output, std::uint32_t outputBytes) noexcept {
    if (!output || outputBytes != sizeof(AetherclimeEnvironmentSnapshotV1)) return 0;
    std::memset(output, 0, sizeof(AetherclimeEnvironmentSnapshotV1));
    try {
        const auto state = ReadCurrentEnvironmentSnapshot();
        if (!state) return 0;
        return aetherclime::ToPublicSnapshotV1(*state, *output) ? 1 : 0;
    } catch (const std::exception& e) {
        SKSE::log::warn("Environment snapshot v1 API failed safely: {}", e.what());
    } catch (...) {
        SKSE::log::warn("Environment snapshot v1 API failed safely (unknown exception).");
    }
    std::memset(output, 0, sizeof(AetherclimeEnvironmentSnapshotV1));
    return 0;
}

// Alpha-49 additive snapshot ABI. V1 and V2 exports/flags remain unchanged.
extern "C" __declspec(dllexport) std::int32_t Aetherclime_GetEnvironmentSnapshotV3(
    AetherclimeEnvironmentSnapshotV3* output, std::uint32_t outputBytes) noexcept {
    if (!output || outputBytes!=sizeof(*output)) return 0;
    std::memset(output,0,sizeof(*output));
    try {
        const auto state=ReadCurrentEnvironmentSnapshot(true);
        if (!state) return 0;
        return aetherclime::ToPublicSnapshotV3(*state,*output)?1:0;
    } catch(const std::exception& e) {
        SKSE::log::warn("Environment snapshot v3 API failed safely: {}",e.what());
    } catch(...) {
        SKSE::log::warn("Environment snapshot v3 API failed safely: unknown exception");
    }
    std::memset(output,0,sizeof(*output));return 0;
}

// Alpha-41 optional native rule API. Validate before touching the engine;
// then collect once and derive season from the same date and active config.
extern "C" __declspec(dllexport) std::int32_t Aetherclime_EvaluateConditionV1(
    const char* rule, std::uint32_t ruleBytes,
    AetherclimeConditionResultV1* output, std::uint32_t outputBytes) noexcept {
    if (!output || outputBytes != sizeof(*output)) return 0;
    try {
        const aetherclime::EnvironmentSnapshot empty{};
        aetherclime::WriteConditionResultV1(rule, ruleBytes, empty, std::nullopt, output, outputBytes);
        if (output->status == static_cast<std::uint32_t>(aetherclime::ConditionStatus::Invalid)) return 1;
        const auto state = ReadCurrentEnvironmentSnapshot(true);
        if (!state) return 1;
        const auto season = GetClimate().GetSeason(state->date).official;
        return aetherclime::WriteConditionResultV1(rule, ruleBytes, *state, season, output, outputBytes);
    } catch (...) {
        *output = {sizeof(*output), 1,
            static_cast<std::uint32_t>(aetherclime::ConditionStatus::Invalid),
            static_cast<std::uint32_t>(aetherclime::ConditionError::InternalError), 0};
        return 1;
    }
}

// Alpha-42 richer optional snapshot; existing V1 export above stays untouched.
extern "C" __declspec(dllexport) std::int32_t Aetherclime_GetEnvironmentSnapshotV2(
    AetherclimeEnvironmentSnapshotV2* output, std::uint32_t outputBytes) noexcept {
    if (!output || outputBytes!=sizeof(*output)) return 0;
    std::memset(output,0,sizeof(*output));
    try {
        const auto state=ReadCurrentEnvironmentSnapshot(true);
        if (!state) return 0;
        return aetherclime::ToPublicSnapshotV2(*state,*output) ? 1 : 0;
    } catch (...) {
        std::memset(output,0,sizeof(*output));
        return 0;
    }
}

// Alpha-47: all rules and the returned V2 record share this single capture.
extern "C" __declspec(dllexport) std::int32_t Aetherclime_EvaluateConditionsV1(
    const AetherclimeConditionRuleV1* rules, std::uint32_t count, std::uint32_t descriptorBytes,
    AetherclimeConditionBatchV1* output, std::uint32_t outputBytes) noexcept {
    if(!aetherclime::ValidConditionBatchEnvelope(rules,count,descriptorBytes,output,outputBytes)) return 0;
    try {
        const auto state=ReadCurrentEnvironmentSnapshot(true);
        return aetherclime::WriteConditionBatchV1(rules,count,descriptorBytes,
            state ? *state : aetherclime::EnvironmentSnapshot{},output,outputBytes);
    } catch(...) {
        return aetherclime::WriteConditionBatchV1(rules,count,descriptorBytes,
            aetherclime::EnvironmentSnapshot{},output,outputBytes,true);
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    InitLog();
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    SKSE::log::info("Aetherclime plugin loaded; waiting for Skyrim data.");
    return true;
}
