#include "Aetherclime/ClimateEstimates.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace aetherclime {
std::string_view OutdoorEstimateBasisName(OutdoorEstimateBasis basis) noexcept {
    switch(basis) {
    case OutdoorEstimateBasis::TrustedRegional:return "trusted-regional";
    case OutdoorEstimateBasis::ProvisionalRegional:return "provisional-regional";
    case OutdoorEstimateBasis::NativeBiome:return "native-biome-estimate";
    case OutdoorEstimateBasis::NearbyBiome:return "nearby-biome-estimate";
    case OutdoorEstimateBasis::BroadTamriel:return "broad-tamriel-estimate";
    case OutdoorEstimateBasis::GenericWorldspace:return "generic-worldspace-estimate";
    case OutdoorEstimateBasis::NearbyClimateReference:return "nearby-climate-reference-estimate";
    default:return "unavailable";
    }
}
OutdoorEstimate EstimateOutdoorClimate(const Config& config, const Sample& sample,
    const Reading& legacy, bool trustedRegion, bool provisionalRegion,
    std::optional<double> biomePreviewC, bool biomeNearby,
    const std::optional<ClimateWorldPoint>& point) {
    OutdoorEstimate out;
    if(!std::isfinite(legacy.airC) || legacy.airC < -100 || legacy.airC > 100) return out;
    // An explicit named region always retains its established published model.
    if(trustedRegion) {
        out.basis=OutdoorEstimateBasis::TrustedRegional;
        out.airC=legacy.airC;
        out.regionTrusted=true;
        return out;
    }
    // A verified native biome is more location-specific than a generic hold
    // fallback. A NEARBY extrapolation is allowed only within atlas gap limits.
    if(config.biomeClimate.enabled && biomePreviewC &&
       std::isfinite(*biomePreviewC) && *biomePreviewC>=-100 && *biomePreviewC<=100 &&
       point && point->worldspaceFormId==0x3C) {
        out.basis=biomeNearby?OutdoorEstimateBasis::NearbyBiome:OutdoorEstimateBasis::NativeBiome;
        out.airC=*biomePreviewC;
        return out;
    }
    if(provisionalRegion) {
        out.basis=OutdoorEstimateBasis::ProvisionalRegional;
        out.airC=legacy.airC;
        return out;
    }
    // Broad calibration is ONLY for vanilla Tamriel. A latitude proxy on the
    // generic temperate region alone caused a documented 2C -> 16C step while
    // walking between College of Winterhold and Whistling Mine (Alpha-49).
    // First calculate that low-confidence broad field, then allow previously
    // surveyed geographic anchors to gently influence it beyond their STRICT
    // region-resolution radii. This is NOT a verified hold boundary.
    if(point && point->worldspaceFormId==0x3C &&
       std::isfinite(point->xRaw) && std::isfinite(point->yRaw)) {
        const double north=std::clamp((point->yRaw+100000.0)/270000.0,0.0,1.0);
        const double latitudeOffsetC=std::lerp(2.0,-12.0,north);
        const double broad=legacy.airC+latitudeOffsetC;
        if(std::isfinite(broad) && broad>=-100 && broad<=100) {
            out.basis=OutdoorEstimateBasis::BroadTamriel;
            out.airC=broad;
            // Use only existing explicitly surveyed Tamriel reference points.
            // Their original configured radius remains the ONLY radius used
            // by the trusted region resolver. The additional 8-cell shoulder
            // influences the secondary estimate only, with a smooth fade.
            // Independent location/biome evidence always won above.
            if(config.geographicFallbackEnabled) {
                double sumWeightedC=0.0, sumWeights=0.0, strongest=0.0;
                double bestDistance=std::numeric_limits<double>::infinity();
                const GeographicClimateAnchor* closest=nullptr;
                for(const auto& anchor:config.geographicAnchors) {
                    if(anchor.worldspaceFormId!=point->worldspaceFormId ||
                       !config.regions.contains(anchor.regionKey) ||
                       !std::isfinite(anchor.xRaw)||!std::isfinite(anchor.yRaw)||
                       !std::isfinite(anchor.radiusRawUnits)||anchor.radiusRawUnits<=0.0)
                        continue;
                    const double distance=std::hypot(point->xRaw-anchor.xRaw,
                                                     point->yRaw-anchor.yRaw);
                    const double plateau=anchor.radiusRawUnits+8192.0; // 2 cells
                    const double limit=std::min(anchor.radiusRawUnits+32768.0,65536.0); // <=16 cells
                    if(!std::isfinite(distance)||distance>limit||limit<=plateau)continue;
                    const double t=std::clamp((distance-plateau)/(limit-plateau),0.0,1.0);
                    const double weight=1.0-t*t*(3.0-2.0*t); // smoothstep fade
                    if(weight<=0.0)continue;
                    auto referenceSample=sample;
                    referenceSample.region=anchor.regionKey;
                    const double referenceAirC=Climate(config).Calculate(referenceSample).airC;
                    if(!std::isfinite(referenceAirC)||referenceAirC< -100||referenceAirC>100)continue;
                    sumWeightedC+=weight*referenceAirC;
                    sumWeights+=weight;
                    strongest=std::max(strongest,weight);
                    ++out.contributingReferences;
                    if(distance<bestDistance) {bestDistance=distance;closest=&anchor;}
                }
                if(sumWeights>0.0 && closest) {
                    const double referenceMean=sumWeightedC/sumWeights;
                    const double result=std::lerp(broad,referenceMean,strongest);
                    if(std::isfinite(result) && result>=-100 && result<=100) {
                        out.basis=OutdoorEstimateBasis::NearbyClimateReference;
                        out.airC=result;
                        out.referenceLabel=closest->label;
                        out.referenceDistanceRawUnits=bestDistance;
                        out.referenceInfluence=strongest;
                    }
                }
            }
            return out;
        }
    }
    // Custom worldspaces with no user-configured region do NOT inherit
    // Tamriel's latitude formula. Generic fallback is explicitly lowest tier.
    out.basis=OutdoorEstimateBasis::GenericWorldspace;
    out.airC=legacy.airC;
    return out;
}
} // namespace aetherclime
