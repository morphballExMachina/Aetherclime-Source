// SDK-only example compiled by CMake without AetherclimeCore, CommonLib or SKSE.
// The calling mod must invoke this on Skyrim's game/main thread.
#include "Aetherclime/NativeClient.h"
#include <optional>

namespace aetherclime::example {
struct TemperatureReading {
    double degreesC{};
    std::uint32_t basis{};
    bool trustedRegion{};
};
std::optional<TemperatureReading> OutdoorTemperature(const client::Client& api,
                                                     bool acceptInferred) {
    const auto reply=api.SnapshotV3();
    if(reply.status!=client::CallStatus::Ok) return std::nullopt;
    const auto& snap=reply.value;
    if(!(snap.flags&AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE))return std::nullopt;
    // 1 is trusted regional, 2..7 are lower-confidence estimates. Inferred
    // estimates are not equivalent to verified outdoor weather or hold identity.
    if(!acceptInferred && snap.outdoorEstimateBasis!=1)return std::nullopt;
    return TemperatureReading{snap.outdoorEstimateC,snap.outdoorEstimateBasis,
        (snap.legacy.legacy.flags&AETHERCLIME_ENV_TRUSTED_REGION)!=0};
}
} // namespace aetherclime::example
