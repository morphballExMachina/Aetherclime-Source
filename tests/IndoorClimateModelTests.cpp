#include "Aetherclime/IndoorClimateModel.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <stdexcept>
#include <string_view>

using namespace aetherclime;
namespace {
void Require(bool result, std::string_view message) {
    if (!result) throw std::runtime_error(std::string(message));
}
bool Near(double x, double y) { return std::abs(x-y) < 1e-9; }
void RecognizedInnsAndOutdoorCoupling() {
    auto c = DefaultConfig();
    auto winter = ResolveInteriorClimateContext(c, true, true, "The Frozen Hearth", "Unrelated Cell",
                                                "winterhold", true);
    auto south = ResolveInteriorClimateContext(c, true, true, "", "dead man's drink", "falkreath", true);
    Require(winter.eligible && south.eligible, "two known inns must be recognized");
    Require(!winter.usedRuleRegion && !south.usedRuleRegion, "trusted region ancestry must be preserved");
    auto cold = AssessIndoorAmbient(c, winter, -12.0);
    auto warm = AssessIndoorAmbient(c, south, 10.0);
    Require(Near(cold.projectedAmbientC, 13.0), "cold inn at -12 outdoors predicts 13C sheltered");
    Require(Near(warm.projectedAmbientC, 18.5), "warm inn at 10 outdoors predicts 18.5C sheltered");
    Require(warm.projectedAmbientC > cold.projectedAmbientC,
            "equivalent inn heating must preserve outdoor temperature difference");
    Require(cold.available && warm.available, "Alpha-49 defaults must model known inns");
    c.interior.enabled = false;
    Require(!AssessIndoorAmbient(c,winter,-12.0).available,"disabled indoor model must abstain");
    c.interior.enabled = true;
    cold = AssessIndoorAmbient(c, winter, -12.0);
    warm = AssessIndoorAmbient(c, south, 10.0);
    Require(cold.available && warm.available, "enabled ambient readings must become available");
    Require(Near(cold.actualAmbientC, 13.0) && Near(warm.actualAmbientC, 18.5),
            "enabled actual reading must equal projected indoors");
}
void UnknownAndConflictingLocations() {
    auto c = DefaultConfig();
    auto noCell = ResolveInteriorClimateContext(c, false, false, "", "", "temperate", false);
    Require(noCell.status == InteriorClimateStatus::MissingCell, "no cell must stay unknown");
    auto outside = ResolveInteriorClimateContext(c, true, false, "The Frozen Hearth", "The Frozen Hearth",
                                                  "winterhold", true);
    Require(outside.status == InteriorClimateStatus::Exterior && !outside.eligible,
            "inn display names in exterior cells must never trigger indoor behavior");
    auto cave = ResolveInteriorClimateContext(c, true, true, "Forsaken Cave", "Forsaken Cave",
                                               "pale", true);
    Require(cave.status == InteriorClimateStatus::UnclassifiedInterior && !cave.eligible,
            "caves or unnamed cells must NOT be assumed heated");
    auto modeledCave=AssessIndoorAmbient(c,cave,-20.0,BuildingKind::CaveOrMine);
    Require(modeledCave.available && modeledCave.basis==IndoorEstimateBasis::BuildingClass &&
        Near(modeledCave.actualAmbientC,8),"a cave should receive conservative cold-cave model");
    auto unknown=AssessIndoorAmbient(c,cave,10.0,BuildingKind::UnknownInterior);
    Require(unknown.available && unknown.basis==IndoorEstimateBasis::Generic &&
        Near(unknown.actualAmbientC,17.0),"mod-added unknown interior gets broad estimate");
    c.interior.estimateUnclassified=false;
    Require(!AssessIndoorAmbient(c,cave,-20.0,BuildingKind::CaveOrMine).available,
        "opt-out preserves explicitly configured inn-only mode");
    c.interior.estimateUnclassified=true;
    auto conflict = ResolveInteriorClimateContext(c, true, true, "The Frozen Hearth", "",
                                                   "reach", true);
    Require(conflict.status == InteriorClimateStatus::ConflictingRegionEvidence && !conflict.eligible,
            "trusted contradictory ancestry must not be overwritten by a rule");
    auto ruleRecovered = ResolveInteriorClimateContext(c, true, true, "The Frozen Hearth", "",
                                                        "temperate", false);
    Require(ruleRecovered.eligible && ruleRecovered.usedRuleRegion && ruleRecovered.regionKey == "winterhold",
            "recognized inn may restore outdoor region when location ancestry is missing");
    c.interior.locations.push_back({"The Frozen Hearth", "falkreath"});
    auto ambiguous = ResolveInteriorClimateContext(c, true, true, "The Frozen Hearth", "",
                                                    "temperate", false);
    Require(ambiguous.status == InteriorClimateStatus::AmbiguousInnRule && !ambiguous.eligible,
            "conflicting configured inn rules must not choose arbitrarily");
}
void LimitsAndValidation() {
    auto c = DefaultConfig();
    auto inn = ResolveInteriorClimateContext(c, true, true, "The Frozen Hearth", "", "winterhold", true);
    c.interior.baselineC = 20.0;
    auto assessment = AssessIndoorAmbient(c, inn, -500.0);
    Require(assessment.clamped && Near(assessment.projectedAmbientC, -5.0),
            "extreme cold must be bounded by minimum ambient");
    assessment = AssessIndoorAmbient(c, inn, 500.0);
    Require(assessment.clamped && Near(assessment.projectedAmbientC, 30.0),
            "extreme heat must be bounded by maximum ambient");
    auto nonFinite = AssessIndoorAmbient(c, inn, std::numeric_limits<double>::infinity());
    Require(!nonFinite.available, "non-finite outdoor reading must not produce ambient");
    c.interior.outdoorCoupling = -0.1;
    bool invalid = false;
    try { Climate climate(c); (void)climate; } catch (const std::invalid_argument&) { invalid = true; }
    Require(invalid, "invalid coupling must be rejected");
    c.interior.outdoorCoupling = 0.25;
    c.interior.locations.push_back({"Daggerfall Tavern", "missing-region"});
    invalid = false;
    try { Climate climate(c); (void)climate; } catch (const std::invalid_argument&) { invalid = true; }
    Require(invalid, "invalid mapped region must be rejected");
}
}
int main() {
    try {
        RecognizedInnsAndOutdoorCoupling();
        UnknownAndConflictingLocations();
        LimitsAndValidation();
        std::cout << "Indoor climate model tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Indoor climate model tests FAILED: " << e.what() << "\n";
        return 1;
    }
}
