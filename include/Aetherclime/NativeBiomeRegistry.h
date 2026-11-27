#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace aetherclime {
enum class NativeBiomeRole { Unclassified, Broad, NoPrecipVariant, LocalOverride };
constexpr const char* NativeBiomeRoleName(NativeBiomeRole role) noexcept {
    switch(role) {
    case NativeBiomeRole::Broad: return "broad";
    case NativeBiomeRole::NoPrecipVariant: return "no-precip-variant";
    case NativeBiomeRole::LocalOverride: return "local-override";
    default: return "unclassified";
    }
}
struct NativeBiomeDefinition { std::uint32_t localFormId; std::string_view editorId, biome; NativeBiomeRole role; };
// Skyrim.esm identities transcribed from SSEEdit, Alpha-37 research.
// Biome labels are research classifications, NOT climate profiles or weather instructions.
inline constexpr std::array<NativeBiomeDefinition,25> kVanillaBiomeRegistry{{
    {0x0002A72D,"WeatherPineForest","pine-forest",NativeBiomeRole::Broad},
    {0x0004D7FD,"WeatherMountains","alpine",NativeBiomeRole::Broad},
    {0x000C5852,"WeatherCoast","coastal",NativeBiomeRole::Broad},
    {0x000C5853,"WeatherSnow","snow",NativeBiomeRole::Broad},
    {0x000C5854,"WeatherVolcanicTundra","volcanic-tundra",NativeBiomeRole::Broad},
    {0x000C5855,"WeatherTundraMarsh","marsh",NativeBiomeRole::Broad},
    {0x000C5856,"WeatherReach","reach",NativeBiomeRole::Broad},
    {0x000C5857,"WeatherFallForest","fall-forest",NativeBiomeRole::Broad},
    {0x000C5858,"WeatherTundra","tundra",NativeBiomeRole::Broad},
    {0x001046C7,"WeatherVolcanicTundraNoPrecip","volcanic-tundra",NativeBiomeRole::NoPrecipVariant},
    {0x001046C8,"WeatherTundraMarshNoPrecip","marsh",NativeBiomeRole::NoPrecipVariant},
    {0x001046C9,"WeatherTundraNoPrecip","tundra",NativeBiomeRole::NoPrecipVariant},
    {0x001046CA,"WeatherSnowNoPrecip","snow",NativeBiomeRole::NoPrecipVariant},
    {0x001046CB,"WeatherMountainsNoPrecip","alpine",NativeBiomeRole::NoPrecipVariant},
    {0x001046CC,"WeatherFallForestNoPrecip","fall-forest",NativeBiomeRole::NoPrecipVariant},
    {0x0010474D,"WeatherFalkreath","",NativeBiomeRole::LocalOverride},
    {0x0010474E,"WeatherDarklightTower","",NativeBiomeRole::LocalOverride},
    {0x000EA25F,"WeatherThroatOfTheWorld","",NativeBiomeRole::LocalOverride},
    {0x000EA260,"WeatherHighHrothgar","",NativeBiomeRole::LocalOverride},
    {0x00105F3F,"WeatherDA02","",NativeBiomeRole::LocalOverride},
    {0x00106667,"WeatherLabyrinthian","",NativeBiomeRole::LocalOverride},
    {0x0010FDC5,"WeatherFFRiften","",NativeBiomeRole::LocalOverride},
    {0x0010FDC6,"WeatherWinterhold","",NativeBiomeRole::LocalOverride},
    {0x0010FE79,"WeatherCoastFog","",NativeBiomeRole::LocalOverride},
    {0x000703C2,"WeatherWindhelm","",NativeBiomeRole::LocalOverride}
}};
constexpr bool IsBroadBiome(NativeBiomeRole role) noexcept {
    return role==NativeBiomeRole::Broad || role==NativeBiomeRole::NoPrecipVariant;
}
}
