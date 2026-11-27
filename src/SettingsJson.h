#pragma once

#include "Aetherclime/Climate.h"
#include <filesystem>

namespace aetherclime {
// Installed with the SKSE adapter; uses nlohmann-json at build time.
// Never overwrites a user's existing file. Invalid files throw instead of being reset.
Config LoadOrCreateConfig(const std::filesystem::path& path);
}
