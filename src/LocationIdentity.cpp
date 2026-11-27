#include "Aetherclime/LocationIdentity.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace aetherclime {
namespace {
std::string CleanDisplayName(std::string_view name) {
    // Data may originate in modded records. Bound the copied text; avoid
    // non-printing/control characters in log and C ABI consumers.
    constexpr std::size_t kMaxChars = 120;
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.remove_prefix(1);
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.remove_suffix(1);
    std::string out;
    out.reserve(std::min(name.size(), kMaxChars));
    for (unsigned char c : name) {
        if (out.size() >= kMaxChars) break;
        out.push_back((c < 32 || c == 127) ? ' ' : static_cast<char>(c));
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}
}

std::string_view LocalNameSourceName(LocalNameSource s) noexcept {
    switch (s) {
    case LocalNameSource::MissingCell: return "missing-cell";
    case LocalNameSource::InteriorCellName: return "active-interior-cell-name";
    case LocalNameSource::NearestLocation: return "nearest-location-record";
    case LocalNameSource::AncestorLocation: return "ancestor-location-record";
    case LocalNameSource::RegionFallback: return "regional-fallback-not-local";
    case LocalNameSource::UnnamedInterior: return "unnamed-interior";
    }
    return "unknown";
}
std::string_view LocalNameCertaintyName(LocalNameCertainty c) noexcept {
    switch (c) {
    case LocalNameCertainty::Unavailable: return "unavailable";
    case LocalNameCertainty::RegionalOnly: return "regional-only";
    case LocalNameCertainty::NamedAreaRecord: return "named-area-not-geographic-proof";
    case LocalNameCertainty::ActiveInteriorCell: return "active-interior-cell";
    }
    return "unavailable";
}

LocalIdentity ResolveLocalIdentity(const LocalIdentityInput& input) {
    LocalIdentity out;
    out.cellFormId = input.cellFormId;
    out.interior = input.interior;
    if (!input.hasCell) return out;

    if (input.interior) {
        out.name = CleanDisplayName(input.activeCellName);
        if (!out.name.empty()) {
            out.source = LocalNameSource::InteriorCellName;
            out.certainty = LocalNameCertainty::ActiveInteriorCell;
            out.exactInteriorCell = true;
            return out;
        }
    }

    // Never publish editor IDs as human-readable display names. An exterior
    // location record may remain constant over numerous cell boundaries.
    for (std::size_t i = 0; i < input.locationChain.size(); ++i) {
        const auto display = CleanDisplayName(input.locationChain[i].displayName);
        if (display.empty()) continue;
        out.name = display;
        out.namedLocationFormId = input.locationChain[i].formId;
        out.locationDepth = static_cast<int>(i);
        out.source = (i == 0) ? LocalNameSource::NearestLocation : LocalNameSource::AncestorLocation;
        out.certainty = LocalNameCertainty::NamedAreaRecord;
        return out;
    }

    if (input.interior) {
        out.name = "Unidentified interior";
        out.source = LocalNameSource::UnnamedInterior;
        return out;
    }
    auto region = CleanDisplayName(input.regionDisplayName);
    if (region.empty()) region = CleanDisplayName(input.regionKey);
    if (region.empty()) region = "Skyrim";
    // Only normalize our synthesized fallback label. Do not alter the exact
    // spelling/capitalization of BGSLocation or interior CELL record names.
    const std::size_t first = region.starts_with("Near ") ? 5u : 0u;
    if (first < region.size() && region[first] >= 'a' && region[first] <= 'z') {
        region[first] = static_cast<char>(region[first] - 'a' + 'A');
    }
    out.name = region + " (local area unknown)";
    out.source = LocalNameSource::RegionFallback;
    out.certainty = LocalNameCertainty::RegionalOnly;
    return out;
}
}  // namespace aetherclime
