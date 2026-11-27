#include "SettingsJson.h"
#include "Aetherclime/TemperatureContinuityPreview.h"

#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace aetherclime {
namespace {
using json = nlohmann::json;

json Encode(const Config& c) {
    json nativeMappings = json::array();
    for (const auto& m : c.nativeRegionProfiles) nativeMappings.push_back({{"plugin",m.plugin},{"localFormId",m.localFormId},{"regionKey",m.regionKey}});
    json months = json::object();
    for (int i = 0; i < 12; ++i) months[std::string(kMonths[i])] = std::string(SeasonName(c.months[i]));
    json regions = json::object();
    for (const auto& [id, r] : c.regions) {
        regions[id] = {
            {"name", r.name}, {"seasonalMeanC", r.seasonalMeanC},
            {"diurnalAmplitudeC", r.diurnalAmplitudeC},
            {"altitudeLapseCPerKm", r.altitudeLapseCPerKm},
            {"permanentSnowBiasC", r.permanentSnowBiasC},
            {"waterMeanC", r.waterMeanC}, {"saltWater", r.saltWater}
        };
    }
    json biomeProfiles=json::object();
    for(const auto& [id,p]:c.biomeClimate.profiles) biomeProfiles[id]={{"seasonalMeanC",p.seasonalMeanC},{"diurnalAmplitudeC",p.diurnalAmplitudeC}};
    json gradients=json::object();
    for(const auto& [id,g]:c.biomeClimate.gradients) gradients[id]={{"southOffsetC",g.southOffsetC},{"northOffsetC",g.northOffsetC}};
    json anchors = json::array();
    for (const auto& a : c.geographicAnchors) {
        anchors.push_back({{"label", a.label}, {"regionKey", a.regionKey},
            {"worldspaceFormId", a.worldspaceFormId},
            {"xRaw", a.xRaw}, {"yRaw", a.yRaw},
            {"radiusRawUnits", a.radiusRawUnits}});
    }
    json provisionalSamples = json::array();
    for (const auto& a : c.provisionalSurveySamples) {
        provisionalSamples.push_back({{"label", a.label}, {"regionKey", a.regionKey},
            {"worldspaceFormId", a.worldspaceFormId},
            {"xRaw", a.xRaw}, {"yRaw", a.yRaw},
            {"radiusRawUnits", a.radiusRawUnits}});
    }
    json atlasZones = json::array();
    for (const auto& a : c.provisionalAtlasZones) {
        atlasZones.push_back({{"label", a.label}, {"regionKey", a.regionKey},
            {"worldspaceFormId", a.worldspaceFormId},
            {"minXRaw", a.minXRaw}, {"maxXRaw", a.maxXRaw},
            {"minYRaw", a.minYRaw}, {"maxYRaw", a.maxYRaw}});
    }
    json elevationReferences = json::array();
    for (const auto& r : c.elevation.references) {
        elevationReferences.push_back({{"label", r.label}, {"regionKey", r.regionKey},
            {"worldspaceFormId", r.worldspaceFormId}, {"xRaw", r.xRaw}, {"yRaw", r.yRaw},
            {"zRaw", r.zRaw}, {"radiusRawUnits", r.radiusRawUnits}});
    }
    json indoorRules = json::array();
    for (const auto& r : c.interior.locations) {
        indoorRules.push_back({{"name", r.name}, {"regionKey", r.regionKey}});
    }
    return {
        {"schemaVersion", 1},
        {"biomeClimate", {{"enabled",c.biomeClimate.enabled},{"maxGapRawUnits",c.biomeClimate.maxGapRawUnits},
            {"gapSeparationRawUnits",c.biomeClimate.gapSeparationRawUnits},{"profiles",biomeProfiles},
            {"spatialGradientEnabled",c.biomeClimate.spatialGradientEnabled},{"gradientSouthYRaw",c.biomeClimate.gradientSouthYRaw},
            {"gradientNorthYRaw",c.biomeClimate.gradientNorthYRaw},{"gradients",gradients}}},
        {"hud", {{"mode", std::string(HudModeName(c.hud.mode))},
            {"temperatureUnit", std::string(HudTemperatureUnitName(c.hud.temperatureUnit))},
            {"cooldownSeconds", c.hud.cooldownSeconds}, {"loadGraceSeconds", c.hud.loadGraceSeconds},
            {"sampleIntervalSeconds", c.hud.sampleIntervalSeconds}, {"temperatureDeltaC", c.hud.temperatureDeltaC},
            {"heartbeatSeconds", c.hud.heartbeatSeconds}, {"sampleOnMenuClose", c.hud.sampleOnMenuClose},
            {"showOnFirstObservation", c.hud.showOnFirstObservation},
            {"showProvisionalEstimates", c.hud.showProvisionalEstimates},
            {"debugInitializationMessage", c.hud.debugInitializationMessage}}},
        {"interior", {{"enabled", c.interior.enabled},
            {"estimateUnclassified", c.interior.estimateUnclassified},
            {"baselineC", c.interior.baselineC},
            {"outdoorCoupling", c.interior.outdoorCoupling},
            {"minAmbientC", c.interior.minAmbientC},
            {"maxAmbientC", c.interior.maxAmbientC},
            {"locations", indoorRules}}},
        {"elevation", {{"enabled", c.elevation.enabled},
            {"rawUnitsPerMeter", c.elevation.rawUnitsPerMeter},
            {"maxRiseMeters", c.elevation.maxRiseMeters},
            {"maxCoolingC", c.elevation.maxCoolingC},
            {"references", elevationReferences}}},
        {"geography", {{"anchors", anchors}, {"provisionalSamples", provisionalSamples},
            {"atlasZones", atlasZones}, {"nativeRegionProfiles", nativeMappings}}},
        {"settings", {
            {"transitionDays", c.transitionDays},
            {"anomalyAmplitudeC", c.anomalyAmplitudeC},
            {"weatherSeed", c.weatherSeed},
            {"fallbackRegion", c.fallbackRegion},
            {"geographicFallbackEnabled", c.geographicFallbackEnabled},
            {"geographicContinuityEnabled", c.geographicContinuityEnabled},
            {"geographicSurveyEnabled", c.geographicSurveyEnabled},
            {"geographicAtlasEnabled", c.geographicAtlasEnabled},
            {"apiConsumerProbeEnabled", c.apiConsumerProbeEnabled},
            {"nativeRegionShadowEnabled", c.nativeRegionShadowEnabled},
            {"nativeRegionGeometryExportEnabled", c.nativeRegionGeometryExportEnabled},
            {"eventClimateEnabled", c.eventClimateEnabled},
            {"temperatureContinuityPreviewEnabled", c.temperatureContinuity.previewEnabled},
            {"temperatureContinuityFadeDistanceRawUnits", c.temperatureContinuity.fadeDistanceRawUnits},
            {"temperatureContinuityMaxCompensationC", c.temperatureContinuity.maxCompensationC},
            {"temperatureContinuityMaxLifetimeGameHours", c.temperatureContinuity.maxLifetimeGameHours},
            {"experimentalLiveMonitor", c.experimentalLiveMonitor},
            {"liveMonitorIntervalSeconds", c.liveMonitorIntervalSeconds},
            {"experimentalMovementProbe", c.experimentalMovementProbe},
            {"experimentalBoundaryProbe", c.experimentalBoundaryProbe},
            {"experimentalWeatherCorrelationProbe", c.experimentalWeatherCorrelationProbe},
            {"movementProbeIntervalSeconds", c.movementProbeIntervalSeconds},
            {"movementProbeMinDistanceRawUnits", c.movementProbeMinDistanceRawUnits},
            {"movementProbeMaxSamples", c.movementProbeMaxSamples},
            {"movementProbeMaxReports", c.movementProbeMaxReports},
            {"daysInMonth", c.daysInMonth}
        }},
        {"months", months}, {"regions", regions}
    };
}

Config Decode(const json& j) {
    if (!j.is_object() || j.value("schemaVersion", 0) != 1) {
        throw std::runtime_error("Unsupported Aetherclime config schema");
    }
    Config config = DefaultConfig();
    if (j.contains("hud")) {
        const auto& h=j.at("hud");
        if (!h.is_object()) throw std::runtime_error("hud must be an object");
        auto& v=config.hud;
        const auto mode=ParseHudMode(h.value("mode",std::string(HudModeName(v.mode))));
        const auto unit=ParseHudTemperatureUnit(h.value("temperatureUnit",std::string(HudTemperatureUnitName(v.temperatureUnit))));
        if (!mode||!unit) throw std::runtime_error("Invalid hud.mode or hud.temperatureUnit");
        v.mode=*mode;v.temperatureUnit=*unit;
        v.cooldownSeconds=h.value("cooldownSeconds",v.cooldownSeconds);
        v.loadGraceSeconds=h.value("loadGraceSeconds",v.loadGraceSeconds);
        v.sampleIntervalSeconds=h.value("sampleIntervalSeconds",v.sampleIntervalSeconds);
        v.temperatureDeltaC=h.value("temperatureDeltaC",v.temperatureDeltaC);
        v.heartbeatSeconds=h.value("heartbeatSeconds",v.heartbeatSeconds);
        v.sampleOnMenuClose=h.value("sampleOnMenuClose",v.sampleOnMenuClose);
        v.showOnFirstObservation=h.value("showOnFirstObservation",v.showOnFirstObservation);
        v.showProvisionalEstimates=h.value("showProvisionalEstimates",v.showProvisionalEstimates);
        v.debugInitializationMessage=h.value("debugInitializationMessage",v.debugInitializationMessage);
        if(!ValidHudSettings(v)) throw std::runtime_error("Invalid hud timing/threshold; see HUD_NOTIFICATIONS_ALPHA48.md");
    }
    const auto& settings = j.at("settings");
    config.transitionDays = settings.value("transitionDays", config.transitionDays);
    config.anomalyAmplitudeC = settings.value("anomalyAmplitudeC", config.anomalyAmplitudeC);
    config.weatherSeed = settings.value("weatherSeed", config.weatherSeed);
    config.fallbackRegion = settings.value("fallbackRegion", config.fallbackRegion);
    config.geographicFallbackEnabled = settings.value(
        "geographicFallbackEnabled", config.geographicFallbackEnabled);
    config.geographicContinuityEnabled = settings.value(
        "geographicContinuityEnabled", config.geographicContinuityEnabled);
    config.geographicSurveyEnabled = settings.value(
        "geographicSurveyEnabled", config.geographicSurveyEnabled);
    config.geographicAtlasEnabled = settings.value(
        "geographicAtlasEnabled", config.geographicAtlasEnabled);
    config.apiConsumerProbeEnabled = settings.value("apiConsumerProbeEnabled",config.apiConsumerProbeEnabled);
    config.nativeRegionShadowEnabled = settings.value("nativeRegionShadowEnabled",config.nativeRegionShadowEnabled);
    config.nativeRegionGeometryExportEnabled = settings.value("nativeRegionGeometryExportEnabled",config.nativeRegionGeometryExportEnabled);
    config.eventClimateEnabled = settings.value("eventClimateEnabled", config.eventClimateEnabled);
    auto& thermal = config.temperatureContinuity;
    thermal.previewEnabled = settings.value("temperatureContinuityPreviewEnabled", thermal.previewEnabled);
    thermal.fadeDistanceRawUnits = settings.value("temperatureContinuityFadeDistanceRawUnits", thermal.fadeDistanceRawUnits);
    thermal.maxCompensationC = settings.value("temperatureContinuityMaxCompensationC", thermal.maxCompensationC);
    thermal.maxLifetimeGameHours = settings.value("temperatureContinuityMaxLifetimeGameHours", thermal.maxLifetimeGameHours);
    if (!ValidTemperatureContinuitySettings(thermal)) {
        throw std::runtime_error("Invalid settings.temperatureContinuity* (fade distance 512..16384 raw units, "
            "compensation 0.5..10 C, lifetime 0.05..1 game hours; all finite)");
    }
    config.experimentalLiveMonitor = settings.value("experimentalLiveMonitor", config.experimentalLiveMonitor);
    config.liveMonitorIntervalSeconds = settings.value("liveMonitorIntervalSeconds", config.liveMonitorIntervalSeconds);
    config.experimentalMovementProbe = settings.value("experimentalMovementProbe", config.experimentalMovementProbe);
    config.experimentalBoundaryProbe = settings.value("experimentalBoundaryProbe", config.experimentalBoundaryProbe);
    config.experimentalWeatherCorrelationProbe = settings.value(
        "experimentalWeatherCorrelationProbe", config.experimentalWeatherCorrelationProbe);
    config.movementProbeIntervalSeconds = settings.value("movementProbeIntervalSeconds", config.movementProbeIntervalSeconds);
    config.movementProbeMinDistanceRawUnits = settings.value("movementProbeMinDistanceRawUnits", config.movementProbeMinDistanceRawUnits);
    config.movementProbeMaxSamples = settings.value("movementProbeMaxSamples", config.movementProbeMaxSamples);
    config.movementProbeMaxReports = settings.value("movementProbeMaxReports", config.movementProbeMaxReports);
    if (!std::isfinite(config.movementProbeIntervalSeconds) ||
        config.movementProbeIntervalSeconds < 1.0 || config.movementProbeIntervalSeconds > 30.0) {
        throw std::runtime_error("settings.movementProbeIntervalSeconds must be between 1 and 30");
    }
    if (!std::isfinite(config.movementProbeMinDistanceRawUnits) ||
        config.movementProbeMinDistanceRawUnits < 32.0 || config.movementProbeMinDistanceRawUnits > 8192.0) {
        throw std::runtime_error("settings.movementProbeMinDistanceRawUnits must be between 32 and 8192");
    }
    if (config.movementProbeMaxSamples < 1 || config.movementProbeMaxSamples > 3600 ||
        config.movementProbeMaxReports < 1 || config.movementProbeMaxReports > 600) {
        throw std::runtime_error("settings.movementProbeMaxSamples must be 1..3600 and movementProbeMaxReports must be 1..600");
    }
    if (!std::isfinite(config.liveMonitorIntervalSeconds) ||
        config.liveMonitorIntervalSeconds < 5.0 || config.liveMonitorIntervalSeconds > 60.0) {
        throw std::runtime_error("settings.liveMonitorIntervalSeconds must be between 5 and 60");
    }
    if (settings.contains("daysInMonth")) config.daysInMonth = settings.at("daysInMonth").get<std::array<int, 12>>();

    if (j.contains("months")) {
        for (int month = 0; month < 12; ++month) {
            auto key = std::string(kMonths[month]);
            if (!j.at("months").contains(key)) continue;
            const auto parsed = ParseSeason(j.at("months").at(key).get<std::string>());
            if (!parsed) throw std::runtime_error("Invalid season for " + key);
            config.months[month] = *parsed;
        }
    }
    if (j.contains("regions")) {
        for (auto iter = j.at("regions").begin(); iter != j.at("regions").end(); ++iter) {
            const auto& p = iter.value();
            Region r = config.regions.contains(iter.key()) ? config.regions.at(iter.key()) : Region{};
            r.name = p.value("name", iter.key());
            if (p.contains("seasonalMeanC")) r.seasonalMeanC = p.at("seasonalMeanC").get<std::array<double, 4>>();
            r.diurnalAmplitudeC = p.value("diurnalAmplitudeC", r.diurnalAmplitudeC);
            r.altitudeLapseCPerKm = p.value("altitudeLapseCPerKm", r.altitudeLapseCPerKm);
            r.permanentSnowBiasC = p.value("permanentSnowBiasC", r.permanentSnowBiasC);
            r.waterMeanC = p.value("waterMeanC", r.waterMeanC);
            r.saltWater = p.value("saltWater", r.saltWater);
            config.regions[iter.key()] = r;
        }
    }
    if(j.contains("biomeClimate")) {
        const auto& bc=j.at("biomeClimate");
        auto& b=config.biomeClimate;
        b.enabled=bc.value("enabled",b.enabled);
        b.maxGapRawUnits=bc.value("maxGapRawUnits",b.maxGapRawUnits);
        b.gapSeparationRawUnits=bc.value("gapSeparationRawUnits",b.gapSeparationRawUnits);
        b.spatialGradientEnabled=bc.value("spatialGradientEnabled",b.spatialGradientEnabled);
        b.gradientSouthYRaw=bc.value("gradientSouthYRaw",b.gradientSouthYRaw);
        b.gradientNorthYRaw=bc.value("gradientNorthYRaw",b.gradientNorthYRaw);
        if(bc.contains("gradients")) {
            const auto& gradients=bc.at("gradients");
            if(!gradients.is_object()||gradients.size()>32) throw std::runtime_error("Invalid biomeClimate.gradients object");
            for(auto it=gradients.begin();it!=gradients.end();++it) {
                if(!it.value().is_object()) throw std::runtime_error("Invalid biome gradient object");
                auto g=b.gradients.contains(it.key())?b.gradients.at(it.key()):BiomeNorthSouthGradient{};
                if(it.value().contains("southOffsetC")) g.southOffsetC=it.value().at("southOffsetC").get<std::array<double,4>>();
                if(it.value().contains("northOffsetC")) g.northOffsetC=it.value().at("northOffsetC").get<std::array<double,4>>();
                b.gradients[it.key()]=g;
            }
        }
        if(bc.contains("profiles")) {
            const auto& profiles=bc.at("profiles");
            if(!profiles.is_object()||profiles.size()>32) throw std::runtime_error("Invalid biomeClimate.profiles object");
            for(auto it=profiles.begin();it!=profiles.end();++it) {
                auto p=b.profiles.contains(it.key())?b.profiles.at(it.key()):BiomeClimateProfile{};
                if(it.value().contains("seasonalMeanC")) p.seasonalMeanC=it.value().at("seasonalMeanC").get<std::array<double, 4>>();
                p.diurnalAmplitudeC=it.value().value("diurnalAmplitudeC",p.diurnalAmplitudeC);
                b.profiles[it.key()]=p;
            }
        }
    }
    // Existing configs lacking geography keep the built-in limited samples.
    // A user-specified empty anchor array explicitly disables mapping via
    // anchors even if geographicFallbackEnabled is true.
    if (j.contains("geography")) {
        const auto& geo = j.at("geography");
        if (!geo.is_object()) throw std::runtime_error("geography must be an object");
        if (geo.contains("nativeRegionProfiles")) {
            const auto& mappings=geo.at("nativeRegionProfiles");
            if(!mappings.is_array() || mappings.size()>128) throw std::runtime_error("Invalid nativeRegionProfiles array");
            config.nativeRegionProfiles.clear();
            for(const auto& m:mappings) config.nativeRegionProfiles.push_back({m.at("plugin").get<std::string>(),m.at("localFormId").get<std::uint32_t>(),m.at("regionKey").get<std::string>()});
        }
        if (geo.contains("anchors")) {
            const auto& anchors = geo.at("anchors");
            if (!anchors.is_array() || anchors.size() > 64) {
                throw std::runtime_error("geography.anchors must be an array of at most 64 entries");
            }
            config.geographicAnchors.clear();
            for (const auto& a : anchors) {
                if (!a.is_object()) throw std::runtime_error("geography.anchors entries must be objects");
                config.geographicAnchors.push_back({
                    a.at("label").get<std::string>(),
                    a.at("regionKey").get<std::string>(),
                    a.at("worldspaceFormId").get<std::uint32_t>(),
                    a.at("xRaw").get<double>(),
                    a.at("yRaw").get<double>(),
                    a.at("radiusRawUnits").get<double>()
                });
            }
        }
        if (geo.contains("atlasZones")) {
            const auto& zones = geo.at("atlasZones");
            if (!zones.is_array() || zones.size() > 32) {
                throw std::runtime_error("geography.atlasZones must be an array of at most 32 entries");
            }
            config.provisionalAtlasZones.clear();
            for (const auto& a : zones) {
                if (!a.is_object()) throw std::runtime_error("geography.atlasZones entries must be objects");
                config.provisionalAtlasZones.push_back({
                    a.at("label").get<std::string>(),
                    a.at("regionKey").get<std::string>(),
                    a.at("worldspaceFormId").get<std::uint32_t>(),
                    a.at("minXRaw").get<double>(), a.at("maxXRaw").get<double>(),
                    a.at("minYRaw").get<double>(), a.at("maxYRaw").get<double>()
                });
            }
        }
        if (geo.contains("provisionalSamples")) {
            const auto& samples = geo.at("provisionalSamples");
            if (!samples.is_array() || samples.size() > 16) {
                throw std::runtime_error("geography.provisionalSamples must be an array of at most 16 entries");
            }
            config.provisionalSurveySamples.clear();
            for (const auto& a : samples) {
                if (!a.is_object()) throw std::runtime_error("geography.provisionalSamples entries must be objects");
                config.provisionalSurveySamples.push_back({
                    a.at("label").get<std::string>(),
                    a.at("regionKey").get<std::string>(),
                    a.at("worldspaceFormId").get<std::uint32_t>(),
                    a.at("xRaw").get<double>(),
                    a.at("yRaw").get<double>(),
                    a.at("radiusRawUnits").get<double>()
                });
            }
        }
    }
    if (j.contains("elevation")) {
        const auto& el = j.at("elevation");
        if (!el.is_object()) throw std::runtime_error("elevation must be an object");
        config.elevation.enabled = el.value("enabled", config.elevation.enabled);
        config.elevation.rawUnitsPerMeter = el.value("rawUnitsPerMeter", config.elevation.rawUnitsPerMeter);
        config.elevation.maxRiseMeters = el.value("maxRiseMeters", config.elevation.maxRiseMeters);
        config.elevation.maxCoolingC = el.value("maxCoolingC", config.elevation.maxCoolingC);
        if (el.contains("references")) {
            if (!el.at("references").is_array() || el.at("references").size() > 32) {
                throw std::runtime_error("elevation.references must be an array of at most 32 entries");
            }
            config.elevation.references.clear();
            for (const auto& r : el.at("references")) {
                if (!r.is_object()) throw std::runtime_error("elevation.references entries must be objects");
                config.elevation.references.push_back({r.at("label").get<std::string>(),
                    r.at("regionKey").get<std::string>(), r.at("worldspaceFormId").get<std::uint32_t>(),
                    r.at("xRaw").get<double>(), r.at("yRaw").get<double>(),
                    r.at("zRaw").get<double>(), r.at("radiusRawUnits").get<double>()});
            }
        }
    }
    if (j.contains("interior")) {
        const auto& indoor = j.at("interior");
        if (!indoor.is_object()) throw std::runtime_error("interior must be an object");
        config.interior.enabled = indoor.value("enabled", config.interior.enabled);
        config.interior.estimateUnclassified = indoor.value("estimateUnclassified", config.interior.estimateUnclassified);
        config.interior.baselineC = indoor.value("baselineC", config.interior.baselineC);
        config.interior.outdoorCoupling = indoor.value("outdoorCoupling", config.interior.outdoorCoupling);
        config.interior.minAmbientC = indoor.value("minAmbientC", config.interior.minAmbientC);
        config.interior.maxAmbientC = indoor.value("maxAmbientC", config.interior.maxAmbientC);
        if (indoor.contains("locations")) {
            const auto& rules = indoor.at("locations");
            if (!rules.is_array() || rules.size() > 32) {
                throw std::runtime_error("interior.locations must contain at most 32 entries");
            }
            config.interior.locations.clear();
            for (const auto& rule : rules) {
                if (!rule.is_object()) throw std::runtime_error("interior.locations entries must be objects");
                config.interior.locations.push_back({
                    rule.at("name").get<std::string>(), rule.at("regionKey").get<std::string>()
                });
            }
        }
    }
    // The constructor performs numeric/calendar validation before acceptance.
    Climate validate(config);
    return config;
}
} // namespace

Config LoadOrCreateConfig(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Unable to create Aetherclime config");
        out << Encode(DefaultConfig()).dump(2) << '\n';
        if (!out) throw std::runtime_error("Unable to write Aetherclime config");
        return DefaultConfig();
    }
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Unable to read Aetherclime config");
    return Decode(json::parse(in));
}
} // namespace aetherclime
