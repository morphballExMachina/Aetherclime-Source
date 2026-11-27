#pragma once
#include "Aetherclime/NativeRegionAtlas.h"
namespace aetherclime {
// Separate environmental identity; never modifies Sample, Reading, or geographic trust.
struct BiomeClimatePreview {
    std::string geographicProfile;
    std::string biomeA, biomeB;
    const char* status{"disabled"};
    bool available{};
    double weightA{}, weightB{}, gapDistanceRaw{-1}, gapRunnerUpRaw{-1};
    double legacyAirC{}, previewAirC{}, deltaC{}, baselineC{}, diurnalC{};
    double gradientOffsetC{}, northFraction{-1};
    bool gradientApplied{};
};
BiomeClimatePreview CompareBiomeClimate(const Config&,const Sample&,const Reading&,const NativeAtlasResult&,
    const std::optional<ClimateWorldPoint>& position=std::nullopt);
}
