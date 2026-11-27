#include "Aetherclime/ElevationModel.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
using namespace aetherclime;
namespace {
void Require(bool v, std::string_view why) { if (!v) throw std::runtime_error(std::string(why)); }
ClimateRegionContext At(double x, double y, std::optional<double> z,
                        std::uint32_t worldspace = 0x3C) {
    ClimateRegionContext c; c.hasCell = true;
    c.exteriorPosition = ClimateWorldPoint{worldspace, x, y, z};
    return c;
}
void CenterAndRise() {
    auto c = DefaultConfig();
    auto a = c.elevation.references[1]; // Falkreath
    const auto center = AssessElevation(c, At(a.xRaw, a.yRaw, a.zRaw), "falkreath");
    Require(center.eligible && !center.applied && center.projectedOffsetC == 0.0,
            "reference center should not change baseline");
    auto up = AssessElevation(c, At(a.xRaw + 512, a.yRaw, a.zRaw + 7000), "falkreath");
    Require(up.eligible && !up.applied && std::abs(up.riseMeters - 100.0) < 1e-6 &&
            std::abs(up.projectedOffsetC + 0.6) < 1e-6, "preview must calculate 100m climb 0.6C cooling");
    c.elevation.enabled = true;
    up = AssessElevation(c, At(a.xRaw + 512, a.yRaw, a.zRaw + 7000), "falkreath");
    Require(up.applied && std::abs(up.actualOffsetC + 0.6) < 1e-6,
            "enabled correction not applied");
    Sample s; s.region = "falkreath"; s.date = {7, 17, 12.0, 201};
    const auto baseline = Climate(c).Calculate(s).airC;
    s.elevationAboveReferenceM = up.effectiveRiseMeters;
    Require(std::abs(Climate(c).Calculate(s).airC - baseline + 0.6) < 1e-6,
            "actual climate offset must match preview");
    c.elevation.enabled = false;
    const auto off = AssessElevation(c, At(a.xRaw + 512, a.yRaw, a.zRaw + 7000), "falkreath");
    Require(!off.applied && off.actualOffsetC == 0, "disabled mode must preserve normal climate");
}
void ExclusionsAndLimits() {
    auto c = DefaultConfig();
    auto a = c.elevation.references[1];
    const auto low = AssessElevation(c, At(a.xRaw, a.yRaw, a.zRaw-10000), "falkreath");
    Require(low.eligible && low.riseMeters < 0 && low.effectiveRiseMeters == 0 &&
        low.projectedOffsetC == 0, "below reference should not be warmed");
    const auto high = AssessElevation(c, At(a.xRaw, a.yRaw, a.zRaw+700000), "falkreath");
    Require(high.clamped && high.effectiveRiseMeters <= 2000 &&
        std::abs(high.projectedOffsetC+12.0) < 1e-6, "extreme high altitude must cap cooling");
    auto ctx = At(a.xRaw, a.yRaw, a.zRaw);
    ctx.interior = true;
    Require(AssessElevation(c, ctx, "falkreath").status == ElevationStatus::ExcludedInterior,
        "interior local coords are not exterior Z");
    ctx.interior = false;
    ctx.exteriorPosition.reset();
    Require(AssessElevation(c, ctx, "falkreath").status == ElevationStatus::MissingExteriorCoordinates,
        "no exterior coordinates must be excluded");
    ctx = At(a.xRaw, a.yRaw, std::nullopt);
    Require(AssessElevation(c, ctx, "falkreath").status == ElevationStatus::MissingExteriorCoordinates,
        "missing Z must not be synthesized");
    ctx = At(a.xRaw, a.yRaw, a.zRaw, 0x999);
    Require(AssessElevation(c, ctx, "falkreath").status == ElevationStatus::MissingCalibration,
        "wrong worldspace must not use Tamriel datum");
    ctx = At(a.xRaw + a.radiusRawUnits + 1, a.yRaw, a.zRaw);
    Require(AssessElevation(c, ctx, "falkreath").status == ElevationStatus::OutsideCalibrationNeighborhood,
        "outside radius must not extrapolate");
    ctx = At(a.xRaw, a.yRaw, std::numeric_limits<double>::infinity());
    Require(AssessElevation(c, ctx, "falkreath").status == ElevationStatus::InvalidCoordinates,
        "invalid Z must be excluded");
    Require(AssessElevation(c, At(a.xRaw, a.yRaw, a.zRaw), "rift").status == ElevationStatus::MissingCalibration,
        "region-mismatched reference must not affect Rift");
    Require(AssessElevation(c, At(a.xRaw, a.yRaw, a.zRaw), "madeup").status == ElevationStatus::UnknownProfile,
        "unrecognized region must decline correction");
}
void AmbiguityAndConfiguration() {
    auto c = DefaultConfig();
    auto a = c.elevation.references[1];
    c.elevation.references.push_back({"ambiguous", "falkreath", 0x3C,
                                      a.xRaw, a.yRaw, a.zRaw + 1000, a.radiusRawUnits});
    Require(AssessElevation(c, At(a.xRaw, a.yRaw, a.zRaw+100), "falkreath").status ==
        ElevationStatus::ConflictingCalibrations, "contradictory tie must not guess");
    c.elevation.references.back().zRaw = a.zRaw;
    Require(AssessElevation(c, At(a.xRaw, a.yRaw, a.zRaw+100), "falkreath").eligible,
        "identical duplicate should not become ambiguous");
    c.elevation.rawUnitsPerMeter = 0;
    bool failed = false;
    try { Climate invalid(c); (void)invalid; } catch (const std::invalid_argument&) { failed=true; }
    Require(failed, "bad meter calibration must fail config validation");
    c.elevation.rawUnitsPerMeter = 70;
    c.elevation.references.front().zRaw = std::numeric_limits<double>::quiet_NaN();
    failed = false;
    try { Climate invalid(c); (void)invalid; } catch (const std::invalid_argument&) { failed=true; }
    Require(failed, "nonfinite references must fail validation");
}
}
int main() {
    try { CenterAndRise(); ExclusionsAndLimits(); AmbiguityAndConfiguration();
          std::cout << "ElevationModel tests passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << "ElevationModel: " << e.what() << "\n"; return 1; }
}
