#include "Aetherclime/BiomeClimatePreview.h"
#include "Aetherclime/BiomeIdentity.h"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace aetherclime {
BiomeClimatePreview CompareBiomeClimate(const Config& config,const Sample& sample,const Reading& legacy,const NativeAtlasResult& atlas,const std::optional<ClimateWorldPoint>& position) {
    BiomeClimatePreview out;
    out.geographicProfile=sample.region; out.legacyAirC=out.previewAirC=legacy.airC;
    const auto& cfg=config.biomeClimate;
    if(!cfg.enabled) return out;
    const auto identity=ResolveBiomeIdentity(config,atlas);
    out.status=identity.status;
    out.biomeA=identity.biomeA;out.biomeB=identity.biomeB;
    out.weightA=identity.weightA;out.weightB=identity.weightB;
    out.gapDistanceRaw=identity.gapDistanceRaw;out.gapRunnerUpRaw=identity.gapRunnerUpRaw;
    if(!identity.available) return out;
    const auto a=cfg.profiles.find(out.biomeA),b=cfg.profiles.find(out.biomeB);
    if(a==cfg.profiles.end()||(!out.biomeB.empty()&&b==cfg.profiles.end())) {out.status="missing-biome-profile";return out;}
    if(!std::isfinite(legacy.airC)||!std::isfinite(legacy.seasonalBaselineC)||!std::isfinite(legacy.diurnalOffsetC)||!std::isfinite(sample.date.hour)) {out.status="invalid-reference-reading";return out;}
    double amplitude=out.weightA*a->second.diurnalAmplitudeC;
    for(unsigned i=0;i<4;++i) out.baselineC+=out.weightA*a->second.seasonalMeanC[i]*legacy.season.weights[i];
    if(out.weightB>0) {
        amplitude+=out.weightB*b->second.diurnalAmplitudeC;
        for(unsigned i=0;i<4;++i) out.baselineC+=out.weightB*b->second.seasonalMeanC[i]*legacy.season.weights[i];
    }
    // Do not infer a real-world latitude or apply Tamriel's calibration to other worlds.
    if(cfg.spatialGradientEnabled && position && position->worldspaceFormId==0x3C &&
       std::isfinite(position->yRaw) && std::isfinite(cfg.gradientSouthYRaw) && std::isfinite(cfg.gradientNorthYRaw) && cfg.gradientNorthYRaw>cfg.gradientSouthYRaw) {
        const double t=std::clamp((position->yRaw-cfg.gradientSouthYRaw)/(cfg.gradientNorthYRaw-cfg.gradientSouthYRaw),0.0,1.0);
        auto accumulate=[&](const std::string& key,double weight) {
            const auto g=cfg.gradients.find(key);
            if(weight<=0||g==cfg.gradients.end()) return;
            for(unsigned i=0;i<4;++i) out.gradientOffsetC+=weight*legacy.season.weights[i]*std::lerp(g->second.southOffsetC[i],g->second.northOffsetC[i],t);
            out.gradientApplied=true;
        };
        accumulate(out.biomeA,out.weightA);accumulate(out.biomeB,out.weightB);
        if(out.gradientApplied) {out.northFraction=t;out.baselineC+=out.gradientOffsetC;}
    }
    out.diurnalC=amplitude*std::cos((sample.date.hour-15)*std::numbers::pi/12);
    // Controlled A/B: swap only seasonal baseline + diurnal amplitude. Retain legacy
    // anomaly, weather and calibrated elevation corrections; their hold dependence is explicit.
    out.deltaC=out.baselineC+out.diurnalC-legacy.seasonalBaselineC-legacy.diurnalOffsetC;
    out.previewAirC=legacy.airC+out.deltaC;
    out.available=std::isfinite(out.previewAirC);
    if(!out.available) {out.status="invalid-preview";out.previewAirC=legacy.airC;out.deltaC=0;}
    return out;
}
}
