#include "Aetherclime/EnvironmentalSnapshot.h"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace aetherclime {
std::string_view OutdoorWeatherProvenanceName(OutdoorWeatherProvenance source) noexcept {
    switch (source) {
    case OutdoorWeatherProvenance::Unavailable: return "unavailable";
    case OutdoorWeatherProvenance::DirectExteriorSky: return "direct-exterior-sky";
    case OutdoorWeatherProvenance::CachedVerifiedExterior: return "cached-verified-exterior";
    case OutdoorWeatherProvenance::InteriorSkyUnverified: return "interior-sky-unverified";
    }
    return "unavailable";
}

EnvironmentSnapshot AssembleEnvironmentSnapshot(const EnvironmentSnapshotInput& in) {
    EnvironmentSnapshot out;
    if (!in.hasCell || in.local.cellFormId == 0 ||
        !std::isfinite(in.outdoorAirC) || !std::isfinite(in.outdoorFeelsLikeC)) return out;
    out.available = true;
    out.interior = in.interior;
    out.trustedRegion = in.trustedRegion;
    out.provisionalRegion = !in.trustedRegion && in.provisionalRegion;
    out.regionKey = in.regionKey;
    out.regionName = in.regionName;
    out.regionSource = in.regionSource;
    out.local = in.local;
    out.building = in.building;
    out.date = in.date;
    out.weather = in.weather;
    out.outdoorAirC = in.outdoorAirC;
    out.outdoorFeelsLikeC = in.outdoorFeelsLikeC;
    if (in.estimatedOutdoorAirC && std::isfinite(*in.estimatedOutdoorAirC) &&
        *in.estimatedOutdoorAirC>=-100 && *in.estimatedOutdoorAirC<=100) {
        out.estimatedOutdoorAirC = *in.estimatedOutdoorAirC;
        out.outdoorEstimateBasis = in.outdoorEstimateBasis;
        out.outdoorReferenceLabel = in.outdoorReferenceLabel;
        out.outdoorReferenceDistanceRaw = in.outdoorReferenceDistanceRaw;
        out.outdoorReferenceInfluence = in.outdoorReferenceInfluence;
        out.outdoorReferenceCount = in.outdoorReferenceCount;
    }
    if (in.interior && in.estimatedIndoorAmbientC && std::isfinite(*in.estimatedIndoorAmbientC) &&
        *in.estimatedIndoorAmbientC>=-100 && *in.estimatedIndoorAmbientC<=100) {
        out.estimatedIndoorAmbientC = *in.estimatedIndoorAmbientC;
        out.indoorEstimateBasis = in.indoorEstimateBasis;
    }
    // The origin must match the cell type. A caller's mistaken label cannot
    // trick us into marking an interior Sky reading as outdoor-verified.
    if (in.interior) {
        out.weatherProvenance = in.weatherProvenance == OutdoorWeatherProvenance::CachedVerifiedExterior ?
            OutdoorWeatherProvenance::CachedVerifiedExterior : OutdoorWeatherProvenance::InteriorSkyUnverified;
    } else {
        out.weatherProvenance = in.weatherProvenance == OutdoorWeatherProvenance::DirectExteriorSky ?
            OutdoorWeatherProvenance::DirectExteriorSky : OutdoorWeatherProvenance::Unavailable;
    }
    if (in.trustedRegion && in.weatherFormId &&
        (out.weatherProvenance == OutdoorWeatherProvenance::DirectExteriorSky ||
         out.weatherProvenance == OutdoorWeatherProvenance::CachedVerifiedExterior)) {
        out.verifiedOutdoorWeatherFormId = in.weatherFormId;
    }
    if (!in.interior && in.trustedRegion) {
        out.localAmbientC = in.outdoorAirC; // only a TRUSTED outdoor-air proxy
    } else if (in.interior && in.trustedRegion && in.building.knownHeatedInnRule && in.modeledIndoorAmbientC &&
               std::isfinite(*in.modeledIndoorAmbientC)) {
        out.localAmbientC = *in.modeledIndoorAmbientC;
        out.localAmbientIsModeledIndoor = true;
    }
    return out;
}
namespace {
template <std::size_t N>
void CopyText(char (&out)[N], std::string_view text) noexcept {
    static_assert(N > 0);
    const auto size = std::min(text.size(), N - 1);
    if (size) std::memcpy(out, text.data(), size);
    out[size] = '\0';
}
}
static_assert(std::is_standard_layout_v<AetherclimeEnvironmentSnapshotV1>);
static_assert(std::is_trivially_copyable_v<AetherclimeEnvironmentSnapshotV1>);

bool ToPublicSnapshotV1(const EnvironmentSnapshot& state,
                        AetherclimeEnvironmentSnapshotV1& output) noexcept {
    std::memset(&output, 0, sizeof(output));
    if (!state.available) return false;
    output.structSize = static_cast<std::uint32_t>(sizeof(output));
    output.schemaVersion = 1;
    output.cellFormId = state.local.cellFormId;
    output.namedLocationFormId = state.local.namedLocationFormId;
    output.verifiedOutdoorWeatherFormId = state.verifiedOutdoorWeatherFormId;
    output.buildingKind = static_cast<std::uint32_t>(state.building.kind);
    output.localNameSource = static_cast<std::uint32_t>(state.local.source);
    output.localNameCertainty = static_cast<std::uint32_t>(state.local.certainty);
    output.weatherProvenance = static_cast<std::uint32_t>(state.weatherProvenance);
    output.weatherKind = static_cast<std::uint32_t>(state.weather);
    output.year = state.date.year;
    output.month = state.date.month;
    output.day = state.date.day;
    output.hour = state.date.hour;
    output.outdoorAirC = state.outdoorAirC;
    output.outdoorFeelsLikeC = state.outdoorFeelsLikeC;
    if (state.interior) output.flags |= AETHERCLIME_ENV_INTERIOR;
    if (state.trustedRegion) output.flags |= AETHERCLIME_ENV_TRUSTED_REGION;
    if (state.provisionalRegion) output.flags |= AETHERCLIME_ENV_PROVISIONAL_REGION;
    if (!state.trustedRegion) output.flags |= AETHERCLIME_ENV_OUTDOOR_AIR_UNVERIFIED;
    if (state.verifiedOutdoorWeatherFormId)
        output.flags |= AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER;
    if (state.localAmbientC) {
        output.flags |= AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE;
        output.localAmbientC = *state.localAmbientC;
    }
    if (state.localAmbientIsModeledIndoor)
        output.flags |= AETHERCLIME_ENV_LOCAL_AMBIENT_MODELED_INDOOR;
    if (state.local.exactInteriorCell)
        output.flags |= AETHERCLIME_ENV_EXACT_INTERIOR_CELL_NAME;
    CopyText(output.localName, state.local.name);
    CopyText(output.regionKey, state.regionKey);
    CopyText(output.regionDisplayName, state.regionName);
    return true;
}

bool ValidSeasonState(const SeasonState& season) noexcept {
    if(static_cast<unsigned>(season.official)>3) return false;
    double sum=0;
    for(double w:season.weights) {if(!std::isfinite(w)||w<0||w>1) return false;sum+=w;}
    return std::abs(sum-1)<1e-9;
}
void AttachEnvironmentDetails(EnvironmentSnapshot& s,const SeasonState& season,
    const BiomeIdentity& biome,std::optional<double> preview) {
    s.season.reset();s.biome={};s.previewAirC.reset();
    if(!s.available) return;
    if(ValidSeasonState(season)) s.season=season;
    if(s.interior) {s.biome.status="interior-no-exterior-biome";return;}
    if(ValidBiomeIdentity(biome)) s.biome=biome;
    else s.biome.status=biome.status ? biome.status : "unavailable";
    if(ValidBiomeIdentity(s.biome) && preview && std::isfinite(*preview)) s.previewAirC=preview;
}
static_assert(std::is_standard_layout_v<AetherclimeEnvironmentSnapshotV2>);
static_assert(std::is_trivially_copyable_v<AetherclimeEnvironmentSnapshotV2>);
static_assert(offsetof(AetherclimeEnvironmentSnapshotV2, legacy)==8);
static_assert(sizeof(AetherclimeEnvironmentSnapshotV2)<1024);
bool ToPublicSnapshotV2(const EnvironmentSnapshot& s,AetherclimeEnvironmentSnapshotV2& out) noexcept {
    std::memset(&out,0,sizeof(out));
    if(!ToPublicSnapshotV1(s,out.legacy)) return false;
    out.structSize=static_cast<std::uint32_t>(sizeof(out));out.schemaVersion=2;
    if(s.season && ValidSeasonState(*s.season)) {
        out.flags|=AETHERCLIME_ENV2_SEASON_AVAILABLE;
        out.officialSeason=static_cast<std::uint32_t>(s.season->official);
        std::copy(s.season->weights.begin(),s.season->weights.end(),out.seasonWeights);
    }
    CopyText(out.biomeStatus,s.interior ? "interior-no-exterior-biome" : s.biome.status ? s.biome.status : "unavailable");
    if(!s.interior && ValidBiomeIdentity(s.biome)) {
        out.flags|=AETHERCLIME_ENV2_BIOME_AVAILABLE;
        out.biomeEvidence=static_cast<std::uint32_t>(s.biome.evidence);
        if(s.biome.evidence==BiomeEvidence::Nearby) out.flags|=AETHERCLIME_ENV2_BIOME_NEARBY;
        CopyText(out.biomeA,s.biome.biomeA);CopyText(out.biomeB,s.biome.biomeB);
        out.biomeWeightA=s.biome.weightA;out.biomeWeightB=s.biome.weightB;
        if(s.previewAirC && std::isfinite(*s.previewAirC)) {
            out.flags|=AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE;out.previewAirC=*s.previewAirC;
        }
    }
    return true;
}

bool ToPublicSnapshotV3(const EnvironmentSnapshot& state,
    AetherclimeEnvironmentSnapshotV3& output) noexcept {
    std::memset(&output,0,sizeof(output));
    if(!state.available || !ToPublicSnapshotV2(state,output.legacy)) {
        std::memset(&output,0,sizeof(output));return false;
    }
    output.structSize=static_cast<std::uint32_t>(sizeof(output));
    output.schemaVersion=3;
    if(state.estimatedOutdoorAirC && std::isfinite(*state.estimatedOutdoorAirC) &&
       *state.estimatedOutdoorAirC>=-100 && *state.estimatedOutdoorAirC<=100 &&
       state.outdoorEstimateBasis!=OutdoorEstimateBasis::Unavailable) {
        output.flags|=AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE;
        output.outdoorEstimateC=*state.estimatedOutdoorAirC;
        output.outdoorEstimateBasis=static_cast<std::uint32_t>(state.outdoorEstimateBasis);
    }
    if(state.interior && state.estimatedIndoorAmbientC &&
       std::isfinite(*state.estimatedIndoorAmbientC) &&
       *state.estimatedIndoorAmbientC>=-100 && *state.estimatedIndoorAmbientC<=100 &&
       state.indoorEstimateBasis!=IndoorEstimateBasis::Unavailable) {
        output.flags|=AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE;
        output.indoorEstimateC=*state.estimatedIndoorAmbientC;
        output.indoorEstimateBasis=static_cast<std::uint32_t>(state.indoorEstimateBasis);
    }
    return true;
}
static_assert(std::is_standard_layout_v<AetherclimeEnvironmentSnapshotV3>);
static_assert(std::is_trivially_copyable_v<AetherclimeEnvironmentSnapshotV3>);

} // namespace aetherclime
