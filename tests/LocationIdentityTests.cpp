#include "Aetherclime/LocationIdentity.h"
#include <array>
#include <cassert>
#include <iostream>
using namespace aetherclime;
int main() {
    const std::array chain{ClimateLocationEvidence{"Whistling Mine", "EditorNameNotForUI", 0x123},
                           ClimateLocationEvidence{"Winterhold", "WholdEditor", 0x456}};
    auto here = ResolveLocalIdentity({true, false, 0x3C, "", chain, "Winterhold", "winterhold"});
    assert(here.name == "Whistling Mine");
    assert(here.source == LocalNameSource::NearestLocation);
    assert(here.certainty == LocalNameCertainty::NamedAreaRecord);
    assert(here.namedLocationFormId == 0x123);
    assert(!here.exactInteriorCell); // location record does NOT prove exact position
    const std::array noNearest{ClimateLocationEvidence{"", "LocEditorOnly", 0x222},
                               ClimateLocationEvidence{"The Pale", "", 0x333}};
    here = ResolveLocalIdentity({true, false, 0x7, "", noNearest, "The Pale", "pale"});
    assert(here.name == "The Pale");
    assert(here.source == LocalNameSource::AncestorLocation && here.locationDepth == 1);
    assert(here.namedLocationFormId == 0x333);
    here = ResolveLocalIdentity({true, false, 0x8, "", {}, "Winterhold", "winterhold"});
    assert(here.name == "Winterhold (local area unknown)");
    assert(here.certainty == LocalNameCertainty::RegionalOnly);
    assert(here.source == LocalNameSource::RegionFallback);
    here = ResolveLocalIdentity({true, false, 0x9, "", {}, "", ""});
    assert(here.name == "Skyrim (local area unknown)");
    here = ResolveLocalIdentity({true, true, 0xAA, " The Frozen Hearth ", chain, "Winterhold", "winterhold"});
    assert(here.name == "The Frozen Hearth");
    assert(here.source == LocalNameSource::InteriorCellName);
    assert(here.exactInteriorCell && here.certainty == LocalNameCertainty::ActiveInteriorCell);
    assert(here.namedLocationFormId == 0);
    here = ResolveLocalIdentity({true, true, 0xAA, "", chain, "Winterhold", "winterhold"});
    assert(here.name == "Whistling Mine" && !here.exactInteriorCell);
    here = ResolveLocalIdentity({true, true, 0xAA, "", {}, "Winterhold", "winterhold"});
    assert(here.name == "Unidentified interior");
    assert(here.certainty == LocalNameCertainty::Unavailable);
    here = ResolveLocalIdentity({false, false, 0, "", chain, "Winterhold", "winterhold"});
    assert(here.name.empty() && here.source == LocalNameSource::MissingCell);
    assert(LocalNameCertaintyName(LocalNameCertainty::NamedAreaRecord) == "named-area-not-geographic-proof");
    std::cout << "Location identity: all assertions passed\n";
}
