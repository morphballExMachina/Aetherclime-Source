#include "Aetherclime/Climate.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aetherclime {
namespace {
constexpr double kPi = 3.14159265358979323846;

double Smooth(double t) noexcept {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

std::uint32_t Hash(std::uint32_t n) noexcept {
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    return n ^ (n >> 16);
}

std::uint32_t RegionHash(std::string_view s) noexcept {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}

double SignedNoise(std::uint32_t seed) noexcept {
    return (static_cast<double>(Hash(seed)) / 4294967295.0) * 2.0 - 1.0;
}

int YearLength(const Config& c) {
    int total = 0;
    for (int d : c.daysInMonth) total += d;
    return total;
}

int StartDay(const Config& c, int month) {
    int result = 0;
    for (int i = 0; i < month; ++i) result += c.daysInMonth[i];
    return result;
}

double AbsoluteDay(const Config& c, const Date& d) {
    if (d.year < 0 || d.month < 0 || d.month >= 12 || d.day < 1 ||
        d.day > c.daysInMonth[d.month] || !std::isfinite(d.hour) ||
        d.hour < 0.0 || d.hour >= 24.0) {
        throw std::invalid_argument("Invalid Tamrielic calendar date");
    }
    return static_cast<double>(StartDay(c, d.month) + d.day - 1) + d.hour / 24.0;
}

double WeatherDelta(Weather weather) noexcept {
    switch (weather) {
    case Weather::Clear: return 1.5;
    case Weather::Cloudy: return -0.5;
    case Weather::Rain: return -2.0;
    case Weather::Storm: return -3.5;
    case Weather::Snow: return -2.5;
    case Weather::Blizzard: return -5.0;
    default: return 0.0;
    }
}

} // namespace

std::string_view SeasonName(Season season) noexcept {
    switch (season) {
    case Season::Winter: return "WIN";
    case Season::Spring: return "SPR";
    case Season::Summer: return "SUM";
    case Season::Autumn: return "AUT";
    }
    return "WIN";
}

std::optional<Season> ParseSeason(std::string_view name) noexcept {
    if (name == "WIN" || name == "Winter" || name == "winter") return Season::Winter;
    if (name == "SPR" || name == "Spring" || name == "spring") return Season::Spring;
    if (name == "SUM" || name == "Summer" || name == "summer") return Season::Summer;
    if (name == "AUT" || name == "Autumn" || name == "autumn" || name == "Fall") return Season::Autumn;
    return std::nullopt;
}

Config DefaultConfig() {
    Config c;
    c.regions.emplace("temperate", Region{});
    c.regions.emplace("whiterun", Region{"Whiterun Hold", {-4, 9, 21, 9}, 6.0, 6.0, 0.0, 9.0, false});
    c.regions.emplace("falkreath", Region{"Falkreath Hold", {0, 10, 19, 9}, 4.0, 6.0, 0.0, 9.0, false});
    c.regions.emplace("rift", Region{"The Rift", {-3, 9, 20, 8}, 5.5, 6.0, 0.0, 8.0, false});
    c.regions.emplace("eastmarch", Region{"Eastmarch / Windhelm", {-14, -2, 7, -3}, 4.0, 6.0, -2.0, 2.0, false});
    c.regions.emplace("winterhold", Region{"Winterhold", {-16, -5, 5, -5}, 3.5, 6.0, -2.5, 1.0, true});
    c.regions.emplace("pale", Region{"The Pale", {-11, 0, 10, 0}, 4.0, 6.0, 0.0, 3.0, true});
    c.regions.emplace("reach", Region{"The Reach", {-5, 7, 18, 7}, 5.0, 6.0, 0.0, 7.0, false});
    c.regions.emplace("haafingar", Region{"Haafingar", {-2, 7, 16, 8}, 3.0, 6.0, 0.0, 8.0, true});
    c.regions.emplace("hjaalmarch", Region{"Hjaalmarch", {-3, 7, 17, 8}, 3.5, 6.0, 0.0, 8.0, false});
    // Experimental environmental baselines, not measured or lore-certified.
    c.biomeClimate.profiles = {
        {"pine-forest",{{0,10,19,9},4.0}}, {"fall-forest",{{-3,9,20,8},5.5}},
        {"tundra",{{-4,9,21,9},6.0}}, {"reach",{{-5,7,18,7},5.0}},
        {"marsh",{{-3,7,17,8},3.5}}, {"volcanic-tundra",{{-4,8,19,7},5.0}},
        {"coastal",{{-10,-1,9,0},3.0}}, {"snow",{{-16,-5,5,-5},3.5}},
        {"alpine",{{-19,-8,2,-8},4.0}}
    };
    c.biomeClimate.gradients.emplace("coastal", BiomeNorthSouthGradient{{7,7,7,7},{-4,-3,-3,-3}});
    // Initial research mappings verified as record identities in Alpha-5/6.
    // Climate association remains a shadow hypothesis, not hold geometry.
    c.nativeRegionProfiles = {{"Skyrim.esm",0x0010FDC6,"winterhold"},
        {"Skyrim.esm",0x0010474D,"falkreath"},{"Skyrim.esm",0x000C5858,"whiterun"}};
    // Coordinates come from the user-verified Alpha-3..Alpha-16 geography
    // snapshots. Each radius is deliberately only a local neighborhood,
    // NOT a claim about the actual hold border. Tamriel is vanilla 0x3C.
    c.geographicAnchors = {
        {"Winterhold field sample", "winterhold", 0x0000003C, 109542.4, 104511.7, 24576.0},
        {"Falkreath field sample", "falkreath", 0x0000003C, -32305.9, -88372.0, 24576.0},
        {"Whiterun tundra field sample", "whiterun", 0x0000003C, 3924.9, 84.9, 24576.0}
    };
    // The Tower Doomstone position was captured during Alpha-26's real
    // northern-coast expedition (rawXY=72751.8,105616.2, Tamriel=0x3C).
    // The 8,192-unit neighborhood is a provisional RESEARCH estimate, not a
    // verified hold boundary. Even at its center it remains untrusted.
    c.provisionalSurveySamples = {
        {"Tower Doomstone northern-coast observation", "winterhold", 0x3C,
         72751.8, 105616.2, 8192.0}
    };
    // Alpha-28 pilot: bounded *provisional* rectangle around the user-captured
    // Tower Doomstone reference XY. The 12,288 x 12,288 raw-unit square is an
    // explicitly experimental tile and NOT a surveyed hold boundary.
    // Keep disabled by default; named/verified geography always wins.
    c.provisionalAtlasZones = {
        {"Tower Doomstone pilot tile", "winterhold", 0x3C,
         66607.8, 78895.8, 99472.2, 111760.2}
    };
    // Source positions are user-observed and verified in Alpha-3..19 logs.
    // These are relative *profile* reference levels only, not sea level.
    c.elevation.references = {
        {"Winterhold reference", "winterhold", 0x3C, 109542.4, 104511.7, -8988.3, 49152.0},
        {"Falkreath reference", "falkreath", 0x3C, -32305.9, -88372.0, -3163.6, 49152.0},
        {"Whiterun tundra reference", "whiterun", 0x3C, 3924.9, 84.9, -5788.9, 49152.0},
        {"Markarth reference", "reach", 0x3C, -170997.1, 5964.2, -4001.2, 49152.0}
    };
    c.interior.locations = {
        {"The Frozen Hearth", "winterhold"},
        {"Dead Man's Drink", "falkreath"}
    };
    return c;
}

Climate::Climate(Config config) : config_(std::move(config)) {
    if (!ValidHudSettings(config_.hud)) throw std::invalid_argument("Invalid hud settings: see HUD_NOTIFICATIONS_ALPHA48.md");
    for (const auto season : config_.months) {
        if (static_cast<unsigned>(season)>3) throw std::invalid_argument("Invalid configured season; require winter, spring, summer or autumn");
    }
    for (int d : config_.daysInMonth) {
        if (d < 1 || d > 40) throw std::invalid_argument("daysInMonth must be 1..40");
    }
    if (config_.transitionDays < 0 || config_.transitionDays > 13) {
        throw std::invalid_argument("transitionDays must be 0..13");
    }
    for (int d : config_.daysInMonth) {
        if (d <= 2 * config_.transitionDays) {
            throw std::invalid_argument("Month too short for selected transitionDays; transition windows must not overlap");
        }
    }
    if (config_.anomalyAmplitudeC < 0.0 || config_.anomalyAmplitudeC > 25.0 ||
        !std::isfinite(config_.anomalyAmplitudeC)) {
        throw std::invalid_argument("anomalyAmplitudeC must be 0..25");
    }
    if (config_.geographicAnchors.size() > 64) {
        throw std::invalid_argument("geography.anchors exceeds the 64-anchor safety limit");
    }
    for (const auto& anchor : config_.geographicAnchors) {
        if (anchor.label.empty() || anchor.label.size() > 96 || anchor.regionKey.empty() ||
            anchor.regionKey.size() > 96 || anchor.worldspaceFormId == 0 ||
            !std::isfinite(anchor.xRaw) || !std::isfinite(anchor.yRaw) ||
            !std::isfinite(anchor.radiusRawUnits) ||
            std::abs(anchor.xRaw) > 10000000.0 ||
            std::abs(anchor.yRaw) > 10000000.0 ||
            anchor.radiusRawUnits <= 0.0 || anchor.radiusRawUnits > 65536.0) {
            throw std::invalid_argument("Invalid geography anchor: label, key, worldspace, XY and radius must be valid");
        }
    }
    if (config_.provisionalSurveySamples.size() > 16) {
        throw std::invalid_argument("geography.provisionalSamples exceeds the 16-sample safety limit");
    }
    for (const auto& point : config_.provisionalSurveySamples) {
        if (point.label.empty() || point.label.size() > 96 || point.regionKey.empty() ||
            !config_.regions.contains(point.regionKey) || point.worldspaceFormId == 0 ||
            !std::isfinite(point.xRaw) || !std::isfinite(point.yRaw) ||
            !std::isfinite(point.radiusRawUnits) ||
            std::abs(point.xRaw) > 10000000.0 ||
            std::abs(point.yRaw) > 10000000.0 ||
            point.radiusRawUnits <= 0.0 || point.radiusRawUnits > 12288.0) {
            throw std::invalid_argument("Invalid provisional geography sample");
        }
    }
    if (config_.provisionalAtlasZones.size() > 32) {
        throw std::invalid_argument("geography.atlasZones exceeds the 32-zone safety limit");
    }
    for (const auto& zone : config_.provisionalAtlasZones) {
        if (zone.label.empty() || zone.label.size() > 96 || zone.regionKey.empty() ||
            !config_.regions.contains(zone.regionKey) || zone.worldspaceFormId == 0 ||
            !std::isfinite(zone.minXRaw) || !std::isfinite(zone.maxXRaw) ||
            !std::isfinite(zone.minYRaw) || !std::isfinite(zone.maxYRaw) ||
            std::abs(zone.minXRaw) > 10000000.0 ||
            std::abs(zone.maxXRaw) > 10000000.0 ||
            std::abs(zone.minYRaw) > 10000000.0 ||
            std::abs(zone.maxYRaw) > 10000000.0 ||
            zone.minXRaw >= zone.maxXRaw || zone.minYRaw >= zone.maxYRaw ||
            zone.maxXRaw - zone.minXRaw > 12288.0 ||
            zone.maxYRaw - zone.minYRaw > 12288.0) {
            throw std::invalid_argument("Invalid provisional atlas zone; require a configured profile and bounded exterior XY rectangle");
        }
    }
    const auto& bc=config_.biomeClimate;
    if(!std::isfinite(bc.maxGapRawUnits)||bc.maxGapRawUnits<0||bc.maxGapRawUnits>8192||
       !std::isfinite(bc.gapSeparationRawUnits)||bc.gapSeparationRawUnits<1||bc.gapSeparationRawUnits>8192||bc.profiles.size()>32)
        throw std::invalid_argument("Invalid biome climate preview limits");
    for(const auto& [key,p]:bc.profiles) {
        if(key.empty()||key.size()>96||!std::isfinite(p.diurnalAmplitudeC)||p.diurnalAmplitudeC<0||p.diurnalAmplitudeC>20)
            throw std::invalid_argument("Invalid biome climate profile");
        for(double t:p.seasonalMeanC) if(!std::isfinite(t)||t < -80||t > 60)
            throw std::invalid_argument("Invalid biome seasonal baseline");
    }
    if(!std::isfinite(bc.gradientSouthYRaw)||!std::isfinite(bc.gradientNorthYRaw)||
       std::abs(bc.gradientSouthYRaw)>1e7||std::abs(bc.gradientNorthYRaw)>1e7||
       bc.gradientNorthYRaw-bc.gradientSouthYRaw<4096||bc.gradients.size()>32)
        throw std::invalid_argument("Invalid biome spatial gradient bounds");
    for(const auto& [key,g]:bc.gradients) {
        if(!bc.profiles.contains(key)) throw std::invalid_argument("Gradient requires a configured biome profile");
        for(const auto& values:{g.southOffsetC,g.northOffsetC})
            for(double x:values) if(!std::isfinite(x)||std::abs(x)>30)
                throw std::invalid_argument("Invalid biome spatial gradient offset");
    }
    if (config_.nativeRegionProfiles.size() > 128) throw std::invalid_argument("Too many native region mappings");
    for (const auto& m : config_.nativeRegionProfiles) {
        if (m.plugin.empty() || m.plugin.size()>128 || !m.localFormId || m.localFormId>0xFFFFFF ||
            !config_.regions.contains(m.regionKey)) throw std::invalid_argument("Invalid native region profile mapping");
    }
    const auto& el = config_.elevation;
    if (!std::isfinite(el.rawUnitsPerMeter) || el.rawUnitsPerMeter < 10.0 || el.rawUnitsPerMeter > 200.0 ||
        !std::isfinite(el.maxRiseMeters) || el.maxRiseMeters <= 0.0 || el.maxRiseMeters > 10000.0 ||
        !std::isfinite(el.maxCoolingC) || el.maxCoolingC <= 0.0 || el.maxCoolingC > 50.0 ||
        el.references.size() > 32) {
        throw std::invalid_argument("Invalid elevation scale, correction limits or reference count");
    }
    for (const auto& point : el.references) {
        if (point.label.empty() || point.label.size() > 96 || point.regionKey.empty() ||
            !config_.regions.contains(point.regionKey) || point.worldspaceFormId == 0 ||
            !std::isfinite(point.xRaw) || !std::isfinite(point.yRaw) ||
            !std::isfinite(point.zRaw) || !std::isfinite(point.radiusRawUnits) ||
            std::abs(point.xRaw) > 10000000.0 || std::abs(point.yRaw) > 10000000.0 ||
            std::abs(point.zRaw) > 10000000.0 ||
            point.radiusRawUnits <= 0.0 || point.radiusRawUnits > 262144.0) {
            throw std::invalid_argument("Invalid elevation reference");
        }
    }
    const auto& indoor = config_.interior;
    if (!std::isfinite(indoor.baselineC) || indoor.baselineC < -30.0 || indoor.baselineC > 50.0 ||
        !std::isfinite(indoor.outdoorCoupling) || indoor.outdoorCoupling < 0.0 || indoor.outdoorCoupling > 1.0 ||
        !std::isfinite(indoor.minAmbientC) || indoor.minAmbientC < -80.0 || indoor.minAmbientC > 50.0 ||
        !std::isfinite(indoor.maxAmbientC) || indoor.maxAmbientC < -30.0 || indoor.maxAmbientC > 80.0 ||
        indoor.minAmbientC >= indoor.maxAmbientC || indoor.locations.size() > 32) {
        throw std::invalid_argument("Invalid interior ambient model settings");
    }
    for (const auto& rule : indoor.locations) {
        if (rule.name.empty() || rule.name.size() > 120 ||
            !config_.regions.contains(rule.regionKey)) {
            throw std::invalid_argument("Invalid interior location rule");
        }
    }
    for (const auto& [key, region] : config_.regions) {
        // Match biome baseline/amplitude limits; reject invalid custom data at
        // config load rather than propagating NaN or infinity into the API.
        for (double t : region.seasonalMeanC) {
            if (!std::isfinite(t) || t < -80 || t > 60)
                throw std::invalid_argument("regions." + key + ".seasonalMeanC must be finite and -80..60 C");
        }
        if (!std::isfinite(region.diurnalAmplitudeC) || region.diurnalAmplitudeC<0 || region.diurnalAmplitudeC>20)
            throw std::invalid_argument("regions." + key + ".diurnalAmplitudeC must be finite and 0..20 C");
        if (!std::isfinite(region.permanentSnowBiasC) || std::abs(region.permanentSnowBiasC)>30)
            throw std::invalid_argument("regions." + key + ".permanentSnowBiasC must be finite and -30..30 C");
        if (!std::isfinite(region.waterMeanC) || region.waterMeanC < -30 || region.waterMeanC > 60)
            throw std::invalid_argument("regions." + key + ".waterMeanC must be finite and -30..60 C");
        if (!std::isfinite(region.altitudeLapseCPerKm) ||
            region.altitudeLapseCPerKm < 0.0 || region.altitudeLapseCPerKm > 20.0) {
            throw std::invalid_argument("Invalid regional altitude lapse rate");
        }
    }
    if (!config_.regions.contains(config_.fallbackRegion)) {
        throw std::invalid_argument("fallbackRegion is missing from regions");
    }
}

SeasonState Climate::GetSeason(const Date& date) const {
    const double today = AbsoluteDay(config_, date);
    const int yearDays = YearLength(config_);
    SeasonState state;
    state.official = config_.months[date.month];
    state.weights.fill(0.0);
    state.weights[static_cast<unsigned>(state.official)] = 1.0;

    int startMonth = date.month;
    int walkedBack = 1;
    while (walkedBack < 12) {
        const int prev = (startMonth + 11) % 12;
        if (config_.months[prev] != state.official) break;
        startMonth = prev;
        ++walkedBack;
    }
    int runMonths = 0;
    while (runMonths < 12 && config_.months[(startMonth + runMonths) % 12] == state.official) ++runMonths;
    int runDays = 0;
    for (int i = 0; i < runMonths; ++i) runDays += config_.daysInMonth[(startMonth + i) % 12];
    double elapsed = today - StartDay(config_, startMonth);
    if (elapsed < 0) elapsed += yearDays;
    state.progress = runMonths == 12 ? today / yearDays : std::clamp(elapsed / runDays, 0.0, 1.0);

    if (config_.transitionDays == 0) return state;
    // Blend near boundaries, not all month long. A 9-day setting spans
    // nine days BEFORE and nine days AFTER the official change.
    for (int month = 0; month < 12; ++month) {
        const int previous = (month + 11) % 12;
        if (config_.months[previous] == config_.months[month]) continue;
        double offset = today - StartDay(config_, month);
        if (offset > yearDays / 2.0) offset -= yearDays;
        if (offset < -yearDays / 2.0) offset += yearDays;
        if (std::abs(offset) > config_.transitionDays) continue;
        const double nextShare = Smooth((offset + config_.transitionDays) / (2.0 * config_.transitionDays));
        state.weights.fill(0.0);
        state.weights[static_cast<unsigned>(config_.months[previous])] = 1.0 - nextShare;
        state.weights[static_cast<unsigned>(config_.months[month])] += nextShare;
        break;
    }
    return state;
}

Reading Climate::Calculate(const Sample& sample) const {
    if (!std::isfinite(sample.elevationAboveReferenceM) || !std::isfinite(sample.windKmh) || !std::isfinite(sample.waterDepthM))
        throw std::invalid_argument("Climate sample elevation, wind and water depth must be finite");
    Reading result;
    result.season = GetSeason(sample.date);
    const auto iter = config_.regions.find(sample.region);
    const auto& region = iter != config_.regions.end() ? iter->second : config_.regions.at(config_.fallbackRegion);
    result.region = region.name;
    for (unsigned i = 0; i < 4; ++i) {
        result.seasonalBaselineC += region.seasonalMeanC[i] * result.season.weights[i];
    }
    result.seasonalBaselineC += region.permanentSnowBiasC;
    result.diurnalOffsetC = region.diurnalAmplitudeC * std::cos((sample.date.hour - 15.0) * kPi / 12.0);
    result.weatherOffsetC = WeatherDelta(sample.weather);
    result.altitudeOffsetC = -region.altitudeLapseCPerKm * std::max(0.0, sample.elevationAboveReferenceM) / 1000.0;
    const double absolute = AbsoluteDay(config_, sample.date) + static_cast<double>(sample.date.year) * YearLength(config_);
    const double threeDay = absolute / 3.0;
    const auto bucket = static_cast<std::int64_t>(std::floor(threeDay));
    const double fraction = Smooth(threeDay - std::floor(threeDay));
    const auto seed = config_.weatherSeed ^ RegionHash(sample.region);
    const auto n0 = SignedNoise(seed ^ static_cast<std::uint32_t>(bucket));
    const auto n1 = SignedNoise(seed ^ static_cast<std::uint32_t>(bucket + 1));
    result.anomalyOffsetC = config_.anomalyAmplitudeC * (n0 + (n1 - n0) * fraction);
    result.airC = result.seasonalBaselineC + result.diurnalOffsetC + result.weatherOffsetC +
                  result.altitudeOffsetC + result.anomalyOffsetC;
    result.feelsLikeC = result.airC;
    if (result.airC <= 10.0 && sample.windKmh > 4.8) {
        const double v = std::pow(sample.windKmh, 0.16);
        result.feelsLikeC = 13.12 + 0.6215 * result.airC - 11.37 * v + 0.3965 * result.airC * v;
    }
    result.underwater = sample.underwater;
    const double freeze = region.saltWater ? -1.8 : 0.0;
    result.waterC = std::clamp(region.waterMeanC + (result.seasonalBaselineC - 5.0) * 0.25 -
        std::max(0.0, sample.waterDepthM) * 0.035, freeze, 27.0);
    if (!std::isfinite(result.airC) || !std::isfinite(result.feelsLikeC) || !std::isfinite(result.waterC))
        throw std::invalid_argument("Climate sample exceeds finite model output limits");
    return result;
}

} // namespace aetherclime
