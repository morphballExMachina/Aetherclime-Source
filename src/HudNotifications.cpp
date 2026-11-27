#include "Aetherclime/HudNotifications.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace aetherclime {
std::string_view HudModeName(HudMode m) noexcept {
    switch(m) { case HudMode::Off:return "off"; case HudMode::Minimal:return "minimal";
    case HudMode::Detailed:return "detailed"; case HudMode::Debug:return "debug"; }
    return "invalid";
}
std::optional<HudMode> ParseHudMode(std::string_view s) noexcept {
    for (auto mode : {HudMode::Off,HudMode::Minimal,HudMode::Detailed,HudMode::Debug})
        if (HudModeName(mode)==s) return mode;
    return {};
}
std::string_view HudTemperatureUnitName(HudTemperatureUnit u) noexcept {
    switch(u) { case HudTemperatureUnit::Celsius:return "C"; case HudTemperatureUnit::Fahrenheit:return "F";
    case HudTemperatureUnit::Both:return "both"; } return "invalid";
}
std::optional<HudTemperatureUnit> ParseHudTemperatureUnit(std::string_view s) noexcept {
    for (auto unit : {HudTemperatureUnit::Celsius,HudTemperatureUnit::Fahrenheit,HudTemperatureUnit::Both})
        if (HudTemperatureUnitName(unit)==s) return unit;
    return {};
}
namespace {
bool InRange(double v,double lo,double hi) noexcept {return std::isfinite(v)&&v>=lo&&v<=hi;}
bool MajorWeather(std::optional<Weather> w) noexcept {
    return w && (*w==Weather::Rain||*w==Weather::Storm||*w==Weather::Snow||*w==Weather::Blizzard);
}
std::string_view WeatherText(Weather w) noexcept {
    switch(w) { case Weather::Clear:return "Clear";case Weather::Cloudy:return "Cloudy";
    case Weather::Rain:return "Rain";case Weather::Storm:return "Storm";case Weather::Snow:return "Snow";
    case Weather::Blizzard:return "Blizzard";default:return "Unknown weather"; }
}
std::string RegionLabel(const EnvironmentSnapshot& e,const std::string& key) {
    // Do not alter user-provided/localized display names. Only repair the
    // known lowercase internal fallback keys reported during Alpha-48 testing.
    if(!e.regionName.empty() && e.regionName!=key) return e.regionName;
    if(key=="whiterun") return "Whiterun";
    if(key=="winterhold") return "Winterhold";
    if(key=="falkreath") return "Falkreath";
    if(key=="rift") return "The Rift";
    if(key=="pale") return "The Pale";
    if(key=="reach") return "The Reach";
    if(key=="eastmarch") return "Eastmarch";
    if(key=="haafingar") return "Haafingar";
    if(key=="hjaalmarch") return "Hjaalmarch";
    return e.regionName.empty()?key:e.regionName;
}
std::string_view SeasonText(Season s) noexcept {
    switch(s) {case Season::Winter:return "Winter";case Season::Spring:return "Spring";
    case Season::Summer:return "Summer";case Season::Autumn:return "Autumn";}return "Unknown season";
}
// Preserve localized bytes; truncate only before a UTF-8 continuation byte.
// Replace ASCII controls (incl. newline) to keep one native notification/log line.
std::string ShortText(std::string_view in,std::size_t max) {
    std::size_t n=std::min(in.size(),max);
    while(n<in.size()&&n>0&&(static_cast<unsigned char>(in[n])&0xc0)==0x80) --n;
    std::string out(in.substr(0,n));
    for(auto& c:out) if(static_cast<unsigned char>(c)<32||c==127) c=' ';
    if(n<in.size()) out+="...";
    return out;
}
std::string Temperature(double c,HudTemperatureUnit unit) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(0);
    // Do not put a misleading -0 on screen.
    const auto rounded=[](double x){const auto r=std::round(x);return r==0?0.0:r;};
    if(unit!=HudTemperatureUnit::Fahrenheit) out<<rounded(c)<<" C";
    if(unit==HudTemperatureUnit::Both) out<<" / ";
    if(unit!=HudTemperatureUnit::Celsius) out<<rounded(Climate::ToFahrenheit(c))<<" F";
    return out.str();
}
// A generous display guard also prevents extreme finite fault injection from
// producing a hundreds-of-digits native notification. Not a model clamp.
bool Displayable(double v) noexcept {return InRange(v,-1000,1000);}
}
bool ValidHudSettings(const HudSettings& s) noexcept {
    return HudModeName(s.mode)!="invalid"&&HudTemperatureUnitName(s.temperatureUnit)!="invalid"&&
        InRange(s.cooldownSeconds,5,3600)&&InRange(s.loadGraceSeconds,0,120)&&
        InRange(s.sampleIntervalSeconds,1,60)&&InRange(s.temperatureDeltaC,0.5,30)&&
        (s.heartbeatSeconds==0||InRange(s.heartbeatSeconds,s.cooldownSeconds,86400));
}
std::string_view HudReasonName(HudReason r) noexcept {
    switch(r) {case HudReason::Initial:return "initial";case HudReason::Season:return "season";
    case HudReason::Region:return "region";case HudReason::Context:return "context";case HudReason::Weather:return "weather";
    case HudReason::Temperature:return "temperature";case HudReason::Location:return "location";
    case HudReason::Evidence:return "evidence";case HudReason::Heartbeat:return "heartbeat";
    case HudReason::DebugInitialization:return "debug-initialization";}return "unknown";
}
std::string_view HudMenuCloseTrigger(std::string_view name) noexcept {
    if(name=="Sleep/Wait Menu")return "wait-sleep-close-deferred";
    if(name=="Journal Menu")return "journal-close-deferred";
    if(name=="Loading Menu")return "loading-close-deferred";
    if(name=="MapMenu")return "map-close-deferred";
    if(name=="Dialogue Menu")return "dialogue-close-deferred";
    if(name=="InventoryMenu"||name=="MagicMenu"||name=="TweenMenu"||name=="ContainerMenu")return "inventory-close-deferred";
    return {};
}
void HudNotifications::Reset() noexcept {
    previous_.reset();lastSample_.reset();lastSent_.reset();lastReference_.reset();reservation_.reset();lastText_.clear();jokeSent_=false;stats_={};
}
bool HudNotifications::BeginSample(const HudSettings& s,double now,bool ready) noexcept {
    reservation_.reset();
    if(s.mode==HudMode::Off||!ValidHudSettings(s)) return false;
    if(!std::isfinite(now)||now<0||(lastSample_&&now<*lastSample_)) {++stats_.invalidClock;return false;}
    if(!ready) {++stats_.blockedUi;return false;}
    if(now<s.loadGraceSeconds) {++stats_.blockedGrace;return false;}
    if(lastSent_&&now-*lastSent_<s.cooldownSeconds) {++stats_.blockedCooldown;return false;}
    if(lastSample_&&now-*lastSample_<s.sampleIntervalSeconds) {++stats_.blockedSampleInterval;return false;}
    lastSample_=now;reservation_=now;++stats_.eligibleSamples;return true;
}
HudNotifications::State HudNotifications::Classify(const HudSettings& s,const EnvironmentSnapshot& e) {
    State v;
    v.interior=e.interior;v.trusted=e.trustedRegion&&!e.provisionalRegion;v.provisional=!v.trusted&&e.provisionalRegion;
    if(v.trusted||(s.showProvisionalEstimates&&v.provisional&&!e.interior)) v.region=e.regionKey;
    // Suppress misleading region-fallback labels and ambiguous exact location claims.
    if(e.local.certainty==LocalNameCertainty::NamedAreaRecord||e.local.certainty==LocalNameCertainty::ActiveInteriorCell)
        v.name=ShortText(e.local.name,42);
    if(e.season&&ValidSeasonState(*e.season)) v.season=e.season->official;
    const bool verified=!e.interior&&v.trusted&&e.verifiedOutdoorWeatherFormId!=0&&
        e.weatherProvenance==OutdoorWeatherProvenance::DirectExteriorSky&&static_cast<unsigned>(e.weather)<static_cast<unsigned>(Weather::Unknown);
    if(verified) v.weather=e.weather;
    if(e.interior) {
        if(v.trusted&&e.localAmbientIsModeledIndoor&&e.localAmbientC&&Displayable(*e.localAmbientC)&&e.building.knownHeatedInnRule) {
            v.temperatureC=*e.localAmbientC;v.modeledIndoor=true;v.temperatureBasis="Indoor model";
        } else if(e.estimatedIndoorAmbientC&&Displayable(*e.estimatedIndoorAmbientC)) {
            v.temperatureC=*e.estimatedIndoorAmbientC;v.modeledIndoor=true;
            v.temperatureBasis=e.indoorEstimateBasis==IndoorEstimateBasis::BuildingClass?
                "Indoor model (building type)":"Indoor model (generic)";
        } else v.temperatureBasis="Indoor estimate unavailable";
    } else if(v.trusted&&!e.localAmbientIsModeledIndoor&&e.localAmbientC&&Displayable(*e.localAmbientC)) {
        v.temperatureC=*e.localAmbientC;v.temperatureBasis=verified?"Outdoor estimate":"Outdoor estimate (weather unverified)";
    } else if(e.estimatedOutdoorAirC && Displayable(*e.estimatedOutdoorAirC)) {
        // The new read-only estimate quality never becomes verified weather.
        // Existing showProvisionalEstimates continues to gate *regional*
        // continuity guesses; independent biome/geography estimates remain
        // available regardless of that regional preference.
        if(e.outdoorEstimateBasis!=OutdoorEstimateBasis::ProvisionalRegional || s.showProvisionalEstimates) {
            v.temperatureC=*e.estimatedOutdoorAirC;
            switch(e.outdoorEstimateBasis) {
            case OutdoorEstimateBasis::ProvisionalRegional:v.temperatureBasis="Provisional regional estimate";break;
            case OutdoorEstimateBasis::NativeBiome:v.temperatureBasis="Outdoor biome estimate";break;
            case OutdoorEstimateBasis::NearbyBiome:v.temperatureBasis="Outdoor nearby-biome estimate";break;
            case OutdoorEstimateBasis::BroadTamriel:v.temperatureBasis="Outdoor geographic estimate";break;
            case OutdoorEstimateBasis::NearbyClimateReference:v.temperatureBasis="Outdoor climate-reference estimate";break;
            case OutdoorEstimateBasis::GenericWorldspace:v.temperatureBasis="Outdoor rough estimate";break;
            default:v.temperatureBasis="Outdoor estimate";break;
            }
        } else v.temperatureBasis="Outdoor estimate unavailable";
    } else if(s.showProvisionalEstimates&&v.provisional&&Displayable(e.outdoorAirC)) {
        // Backward-compatible Alpha-48 independently built snapshots.
        v.temperatureC=e.outdoorAirC;v.temperatureBasis="Provisional regional estimate";
    } else v.temperatureBasis="Outdoor estimate unavailable";
    v.evidence=ShortText(e.regionSource,36);
    if(!e.interior&&e.biome.available&&ValidBiomeIdentity(e.biome)) {
        v.evidence+="/"+ShortText(e.biome.biomeA,24);
        if(!e.biome.biomeB.empty()) v.evidence+="+"+ShortText(e.biome.biomeB,24);
        v.evidence+="/"+std::string(BiomeEvidenceName(e.biome.evidence));
    }
    return v;
}
std::string HudNotifications::Format(const HudSettings& s,const EnvironmentSnapshot& e,const State& v) {
    std::string text="Aetherclime: ";
    if(v.season) {text+=SeasonText(*v.season);text+=" | ";}
    // Profile display names are regional labels, not exact hold boundaries.
    if(!v.region.empty()) {if(v.provisional) text+="Near ";text+=ShortText(RegionLabel(e,v.region),38);text+=" | ";}
    if(s.mode!=HudMode::Minimal&&!v.name.empty()&&
        (v.region.empty()||v.name!=RegionLabel(e,v.region))) {
        text+=v.interior?"Interior: ":"Area: ";text+=v.name;text+=" | ";
    }
    text+=v.temperatureBasis;
    if(v.temperatureC) text+=" "+Temperature(*v.temperatureC,s.temperatureUnit);
    if(v.weather&&(s.mode!=HudMode::Minimal||MajorWeather(v.weather))) {text+=" | ";text+=WeatherText(*v.weather);}
    if(s.mode==HudMode::Debug) {
        text+=" | "+v.evidence;
        if(!e.interior&&e.previewAirC&&Displayable(*e.previewAirC))
            text+=" | Experimental preview "+Temperature(*e.previewAirC,s.temperatureUnit);
    }
    // The prefix and temperature stay ahead of optional debug details.
    return ShortText(text,240);
}
std::optional<HudNotice> HudNotifications::Observe(const HudSettings& s,const EnvironmentSnapshot& e) {
    if(!reservation_) return {};
    const auto now=*reservation_;reservation_.reset();
    if(s.mode==HudMode::Off||!ValidHudSettings(s))return {};
    if(!e.available||!e.local.cellFormId) {++stats_.unavailable;return {};}
    auto next=Classify(s,e);
    if(s.mode==HudMode::Debug&&s.debugInitializationMessage&&!jokeSent_) {
        jokeSent_=true;lastSent_=now;++stats_.sent;
        lastText_="Aetherclime initialized. Marshmallow reserves critically low.";
        return HudNotice{HudReason::DebugInitialization,lastText_};
    }
    const bool heartbeatDue=s.heartbeatSeconds>0&&lastReference_&&now-*lastReference_>=s.heartbeatSeconds;
    std::optional<HudReason> reason;
    if(!previous_) {
        if(s.showOnFirstObservation) reason=HudReason::Initial;
        else {previous_=std::move(next);lastReference_=now;++stats_.baselineOnly;return {};}
    } else {
        const auto& old=*previous_;
        if(old.interior!=next.interior||old.modeledIndoor!=next.modeledIndoor||old.temperatureC.has_value()!=next.temperatureC.has_value()||old.temperatureBasis!=next.temperatureBasis) reason=HudReason::Context;
        else if(old.season!=next.season&&next.season) reason=HudReason::Season;
        else if(old.region!=next.region||old.trusted!=next.trusted||old.provisional!=next.provisional) reason=HudReason::Region;
        else if(old.weather!=next.weather&&(s.mode!=HudMode::Minimal||MajorWeather(old.weather)||MajorWeather(next.weather))) reason=HudReason::Weather;
        else if(s.mode!=HudMode::Minimal&&old.temperatureC&&next.temperatureC&&std::abs(*old.temperatureC-*next.temperatureC)>=s.temperatureDeltaC) reason=HudReason::Temperature;
        else if(s.mode!=HudMode::Minimal&&old.name!=next.name) reason=HudReason::Location;
        else if(s.mode==HudMode::Debug&&old.evidence!=next.evidence) reason=HudReason::Evidence;
        else if(heartbeatDue) reason=HudReason::Heartbeat;
    }
    if(!reason) {++stats_.unchanged;return {};}
    auto text=Format(s,e,next);
    if(text==lastText_&&*reason!=HudReason::Heartbeat) {
        // A semantic change hidden by formatting must not starve an enabled heartbeat.
        if(heartbeatDue)reason=HudReason::Heartbeat;
        else {++stats_.unchanged;return {};}
    }
    if(*reason==HudReason::Heartbeat && !next.temperatureC && text==lastText_) {
        // Alpha-49: the reason may have converted from an invisible context
        // change to heartbeat. Suppress identical unavailable text either way.
        previous_=std::move(next);++stats_.unchanged;return {};
    }
    previous_=std::move(next);lastText_=text;lastSent_=now;lastReference_=now;++stats_.sent;
    return HudNotice{*reason,std::move(text)};
}
} // namespace aetherclime
