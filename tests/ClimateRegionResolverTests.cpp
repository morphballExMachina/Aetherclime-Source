#include "Aetherclime/ClimateRegionResolver.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace aetherclime;

namespace {
void Check(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}

void NamedHolds() {
    const auto config = DefaultConfig();
    for (const auto [name, expected] : std::array{
        std::pair{"Winterhold", "winterhold"}, std::pair{"Falkreath", "falkreath"},
        std::pair{"Whiterun", "whiterun"}, std::pair{"Riften", "rift"},
        std::pair{"The Rift", "rift"}, std::pair{"Windhelm", "eastmarch"},
        std::pair{"Dawnstar", "pale"}, std::pair{"Markarth", "reach"},
        std::pair{"Solitude", "haafingar"}, std::pair{"Morthal", "hjaalmarch"}
    }) {
        const ClimateLocationEvidence location{name, "", 123};
        const auto result = ResolveClimateRegion(config, {.locationChain = std::span(&location, 1), .hasCell = true});
        Check(result.regionKey == expected, "wrong named hold classification");
        Check(result.source == ClimateRegionSource::LocationDisplayName, "expected display evidence");
        Check(result.matchedLocationFormId == 123, "expected copied FormID");
        Check(result.matched(), "known hold must be matched");
    }
}

void ParentsAndEditorIds() {
    const auto config = DefaultConfig();
    const std::array chain{
        ClimateLocationEvidence{"Hall of Attainment", "HallOfAttainmentLocation", 11},
        ClimateLocationEvidence{"College of Winterhold", "CollegeOfWinterholdLocation", 22},
        ClimateLocationEvidence{"Winterhold", "WinterholdHoldLocation", 33}
    };
    const auto resolved = ResolveClimateRegion(config, {
        .locationChain = chain, .hasCell = true, .interior = true
    });
    Check(resolved.regionKey == "winterhold", "parent location should select winterhold");
    Check(resolved.chainDepth == 1, "nearest named location depth incorrect");
    Check(resolved.interior, "indoor source metadata missing");

    const ClimateLocationEvidence editorOnly{"(untranslated)", "WhiterunHoldLocation", 75};
    const auto editorResult = ResolveClimateRegion(config, {.locationChain = std::span(&editorOnly, 1), .hasCell = true});
    Check(editorResult.regionKey == "whiterun", "known editor ID shape should resolve");
    Check(editorResult.source == ClimateRegionSource::LocationEditorId, "expected editor ID evidence");
}

void FallBackHonestly() {
    const auto config = DefaultConfig();
    const auto empty = ResolveClimateRegion(config, {});
    Check(empty.regionKey == "temperate", "empty evidence fallback");
    Check(empty.source == ClimateRegionSource::FallbackNoEvidence, "no-evidence provenance");

    const ClimateLocationEvidence unknown{"Some Strange Modded Dungeon", "WeirdDungeonLoc", 7};
    const auto noMatch = ResolveClimateRegion(config, {.locationChain = std::span(&unknown, 1), .usedCellLocation = true});
    Check(noMatch.regionKey == "temperate", "unmatched fallback");
    Check(noMatch.source == ClimateRegionSource::FallbackUnmatched, "unmatched provenance");
    Check(noMatch.usedCellLocation, "cell-location source metadata lost");

    const ClimateLocationEvidence ambiguous{"Whiterun / Winterhold Border", "", 8};
    const auto ambiguity = ResolveClimateRegion(config, {.locationChain = std::span(&ambiguous, 1)});
    Check(ambiguity.regionKey == "temperate", "ambiguous climate should be fallback");
    Check(ambiguity.source == ClimateRegionSource::FallbackAmbiguous, "ambiguous provenance");

    Config incomplete = config;
    incomplete.regions.erase("winterhold");
    const ClimateLocationEvidence lost{"Winterhold", "", 9};
    const auto missing = ResolveClimateRegion(incomplete, {.locationChain = std::span(&lost, 1)});
    Check(missing.regionKey == "temperate", "unconfigured profile should fallback");
    Check(missing.source == ClimateRegionSource::FallbackUnconfigured, "unconfigured provenance");
}

void AvoidFalsePositives() {
    const auto config = DefaultConfig();
    for (const auto text : {"NotWinterhold", "WeatherWinterhold", "SnowyPaleontologist", "Reachable", "OldRiftenia"}) {
        const ClimateLocationEvidence loc{text, "", 7};
        const auto output = ResolveClimateRegion(config, {.locationChain = std::span(&loc, 1)});
        Check(output.source == ClimateRegionSource::FallbackUnmatched, "substring caused false hold classification");
    }
    const ClimateLocationEvidence loc{"", "LocWinterhold", 12};
    const auto output = ResolveClimateRegion(config, {.locationChain = std::span(&loc, 1)});
    Check(output.regionKey == "winterhold", "known editor prefix must resolve");
}

void SelectionFeedsClimate() {
    const Climate climate;
    const ClimateLocationEvidence cold{"Winterhold", "", 3};
    const ClimateLocationEvidence mild{"Falkreath", "", 4};
    const auto winterhold = ResolveClimateRegion(climate.Settings(), {.locationChain = std::span(&cold, 1)});
    const auto falkreath = ResolveClimateRegion(climate.Settings(), {.locationChain = std::span(&mild, 1)});
    Sample a{.date = {0, 15, 12.0, 201}, .region = winterhold.regionKey};
    Sample b = a;
    b.region = falkreath.regionKey;
    Check(climate.Calculate(a).region == "Winterhold", "resolved profile not applied to climate core");
    Check(climate.Calculate(b).region == "Falkreath Hold", "Falkreath routing wrong");
    Check(climate.Calculate(a).seasonalBaselineC < climate.Calculate(b).seasonalBaselineC,
        "configured climate baselines should differ");
}
}
int main() {
    NamedHolds();
    ParentsAndEditorIds();
    FallBackHonestly();
    AvoidFalsePositives();
    SelectionFeedsClimate();
    std::cout << "Climate region resolver: all 5 test groups passed\n";
}
