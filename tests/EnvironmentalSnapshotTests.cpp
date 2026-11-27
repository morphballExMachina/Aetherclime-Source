#include "Aetherclime/EnvironmentalSnapshot.h"
#include "Aetherclime/PluginApi.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <type_traits>
using namespace aetherclime;
int main() {
    static_assert(std::is_standard_layout_v<AetherclimeEnvironmentSnapshotV1>);
    static_assert(std::is_trivially_copyable_v<AetherclimeEnvironmentSnapshotV1>);
    static_assert(sizeof(AetherclimeEnvironmentSnapshotV1) < 512);
    EnvironmentSnapshotInput in;
    in.hasCell = true;
    in.regionKey = "winterhold";
    in.regionName = "Winterhold";
    in.trustedRegion = true;
    in.local = ResolveLocalIdentity({true, false, 0x123, "", {}, "Winterhold", "winterhold"});
    in.outdoorAirC = -12.0;
    in.outdoorFeelsLikeC = -15.0;
    in.weather = Weather::Snow;
    in.weatherFormId = 0xCAFE;
    in.weatherProvenance = OutdoorWeatherProvenance::DirectExteriorSky;
    auto out = AssembleEnvironmentSnapshot(in);
    assert(out.available && !out.interior);
    assert(out.localAmbientC && *out.localAmbientC == -12.0);
    assert(!out.localAmbientIsModeledIndoor);
    assert(out.verifiedOutdoorWeatherFormId == 0xCAFE);
    assert(out.weatherProvenance == OutdoorWeatherProvenance::DirectExteriorSky);
    AetherclimeEnvironmentSnapshotV1 exported{};
    assert(ToPublicSnapshotV1(out, exported));
    assert(exported.schemaVersion == 1 && exported.structSize == sizeof(exported));
    assert(exported.flags & AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER);
    assert(exported.flags & AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE);
    assert(!(exported.flags & AETHERCLIME_ENV_INTERIOR));
    assert(exported.localAmbientC == -12.0);
    assert(std::string(exported.localName) == "Winterhold (local area unknown)");
    in.interior = true;
    in.local = ResolveLocalIdentity({true, true, 0x124, "The Frozen Hearth", {}, "Winterhold", "winterhold"});
    in.building = {BuildingKind::Inn, BuildingEvidence::ConfiguredInn, false, true};
    in.weatherProvenance = OutdoorWeatherProvenance::CachedVerifiedExterior;
    in.modeledIndoorAmbientC = 13.0;
    out = AssembleEnvironmentSnapshot(in);
    assert(out.available && out.interior);
    assert(out.localAmbientC && *out.localAmbientC == 13.0);
    assert(out.localAmbientIsModeledIndoor);
    assert(out.verifiedOutdoorWeatherFormId == 0xCAFE);
    assert(ToPublicSnapshotV1(out, exported));
    assert(exported.flags & AETHERCLIME_ENV_INTERIOR);
    assert(exported.flags & AETHERCLIME_ENV_LOCAL_AMBIENT_MODELED_INDOOR);
    assert(exported.flags & AETHERCLIME_ENV_EXACT_INTERIOR_CELL_NAME);
    assert(std::string(exported.localName) == "The Frozen Hearth");
    in.building = {BuildingKind::PalaceOrCourt, BuildingEvidence::KnownName, false, false};
    out = AssembleEnvironmentSnapshot(in);
    assert(!out.localAmbientC); // palace is NOT assumed heated
    assert(ToPublicSnapshotV1(out, exported));
    assert(!(exported.flags & AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE));
    assert(exported.localAmbientC == 0.0);
    in.weatherProvenance = OutdoorWeatherProvenance::InteriorSkyUnverified;
    out = AssembleEnvironmentSnapshot(in);
    assert(out.verifiedOutdoorWeatherFormId == 0);
    assert(out.weatherProvenance == OutdoorWeatherProvenance::InteriorSkyUnverified);
    assert(ToPublicSnapshotV1(out, exported));
    assert(!(exported.flags & AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER));
    assert(exported.verifiedOutdoorWeatherFormId == 0);
    in.weatherProvenance = OutdoorWeatherProvenance::DirectExteriorSky; // dishonest caller
    out = AssembleEnvironmentSnapshot(in);
    assert(out.verifiedOutdoorWeatherFormId == 0);
    in.interior = false;
    in.weatherProvenance = OutdoorWeatherProvenance::CachedVerifiedExterior; // dishonest caller
    out = AssembleEnvironmentSnapshot(in);
    assert(out.weatherProvenance == OutdoorWeatherProvenance::Unavailable);
    assert(out.verifiedOutdoorWeatherFormId == 0);
    // The historic 17.81 C temperate fallback must no longer masquerade as
    // a trustworthy local temperature when north-of-Skyrim evidence runs out.
    in.interior = false;
    in.building = {BuildingKind::Exterior, BuildingEvidence::Exterior, false, false};
    in.trustedRegion = false;
    in.provisionalRegion = false;
    in.outdoorAirC = 17.81;
    in.outdoorFeelsLikeC = 17.81;
    in.weatherProvenance = OutdoorWeatherProvenance::Unavailable;
    out = AssembleEnvironmentSnapshot(in);
    assert(out.available && !out.localAmbientC);
    assert(ToPublicSnapshotV1(out, exported));
    assert(exported.flags & AETHERCLIME_ENV_OUTDOOR_AIR_UNVERIFIED);
    assert(!(exported.flags & AETHERCLIME_ENV_TRUSTED_REGION));
    assert(!(exported.flags & AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE));
    in.provisionalRegion = true;
    out = AssembleEnvironmentSnapshot(in);
    assert(out.provisionalRegion && !out.trustedRegion && !out.localAmbientC);
    assert(ToPublicSnapshotV1(out, exported));
    assert(exported.flags & AETHERCLIME_ENV_PROVISIONAL_REGION);
    assert(exported.flags & AETHERCLIME_ENV_OUTDOOR_AIR_UNVERIFIED);
    in.provisionalRegion = false;
    in.trustedRegion = true;
    in.hasCell = false;
    assert(!AssembleEnvironmentSnapshot(in).available);
    in.hasCell = true;
    in.local.cellFormId = 0;
    assert(!AssembleEnvironmentSnapshot(in).available);
    in.local.cellFormId = 0x100;
    in.outdoorAirC = std::nan("");
    assert(!AssembleEnvironmentSnapshot(in).available);
    assert(!ToPublicSnapshotV1(AssembleEnvironmentSnapshot(in), exported));
    assert(exported.schemaVersion == 0 && exported.localName[0] == '\0');
    assert(OutdoorWeatherProvenanceName(OutdoorWeatherProvenance::CachedVerifiedExterior) == "cached-verified-exterior");
    std::cout << "Environmental snapshot: all assertions passed\n";
}
