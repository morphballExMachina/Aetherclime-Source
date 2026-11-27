#pragma once

// CommonLibSSE-NG's umbrella headers supply the standard-library aliases,
// REL types, Windows API declarations, and SKSE interfaces used by both our
// adapter and its CMake-generated plugin metadata translation unit.
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

// CommonLibSSE-NG 3.7 generates plugin metadata with "..."sv literals.
// Its generated translation unit receives this header via CMake's PCH.
#include <string_view>
using namespace std::literals::string_view_literals;
