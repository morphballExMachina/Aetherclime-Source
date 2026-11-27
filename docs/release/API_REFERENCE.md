# Aetherclime Native API Reference

**Framework:** Aetherclime 0.5.0 RC1  
**Audience:** SKSE/C++ plugin developers  
**Interface:** Synchronous, versioned native DLL exports (Windows x64)

*Aetherclime understands the environment. Other mods bring it to life.*

Aetherclime provides calendar seasons, regional climate, exterior and interior temperature estimates, location/weather evidence, and a condition-query engine. **It does not modify the weather, actors, landscapes, or game state.** Your plugin decides whether and how to act on its results.

> **Important:** The native API is not a Papyrus interface. Call it from Skyrim's **main/game thread**, after `Aetherclime.dll` has loaded. Values are modeled estimates, not physical measurements; test availability flags before reading them.
---

## 1. Getting started

The developer SDK includes two public headers:
- `include/Aetherclime/PluginApi.h` — exact C ABI declarations, structures, flags and export names.
- `include/Aetherclime/NativeClient.h` — optional C++17 client/helper supporting version discovery, snapshots and condition calls.

The client header and its examples **do not link against AetherclimeCore, CommonLibSSE-NG, or SKSE**. Aetherclime's DLL must be installed and loaded independently.

### Discover the provider

```cpp
#include <Windows.h>
#include "Aetherclime/NativeClient.h"

// Invoke on Skyrim's game thread, after SKSE has loaded plugins.
const HMODULE module = GetModuleHandleW(L"Aetherclime.dll");
const auto api = aetherclime::client::Client::Discover(
    [module](const char* symbol) {
        return module ? GetProcAddress(module, symbol) : nullptr;
    });

if (api.Status() != aetherclime::client::CallStatus::Ok) {
    // Provider absent, initialization incomplete, or incompatible.
    return;
}
```

`Aetherclime_ApiVersion()` deliberately returns **1** for legacy compatibility. It is **not** the framework release number. Snapshot V2/V3 and condition queries are separate optional exports: feature-detect them rather than assuming they're present.

### Read an outdoor temperature safely

```cpp
const auto reply = api.SnapshotV3();
if (reply.status == aetherclime::client::CallStatus::Ok) {
    const auto& s = reply.value;
    if (s.flags & AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE) {
        const double temperatureC = s.outdoorEstimateC;
        const bool trustedRegion = (s.outdoorEstimateBasis == 1);
        // Your mod decides whether to accept lower-confidence estimates.
        (void)temperatureC;
        (void)trustedRegion;
    }
}
```

If the player context has not settled, `SnapshotV3()` may return `Unavailable`. If the installed DLL lacks the optional export, it may return `MissingExport`. **Zero Celsius is a valid temperature, never an unavailable sentinel.**
---

## 2. Available exports

| Native export | What it returns |
| --- | --- |
| `Aetherclime_ApiVersion()` | Legacy ABI version, currently 1 |
| `Aetherclime_GetSeason()` | 0 winter; 1 spring; 2 summer; 3 autumn |
| `Aetherclime_GetSeasonProgress()` | Progress through the current official season |
| `Aetherclime_GetAmbientC()` | Legacy regional modeled air temperature, **not V3 indoor ambient** |
| `Aetherclime_GetFeelsLikeC()` | Legacy modeled feels-like temperature |
| `Aetherclime_GetEnvironmentSnapshotV1(...)` | Established regional, location, date and weather information |
| `Aetherclime_GetEnvironmentSnapshotV2(...)` | Nested V1, official season weights, biome and experimental preview information |
| `Aetherclime_GetEnvironmentSnapshotV3(...)` | Nested V2, plus explicitly labeled outdoor and indoor temperature estimates |
| `Aetherclime_EvaluateConditionV1(...)` | One condition rule with True / False / Unknown / Invalid |
| `Aetherclime_EvaluateConditionsV1(...)` | 1–32 conditions evaluated against one captured context |

Use `GetProcAddress` or the C++ helper to discover optional exports. Never assume that a mod with `Aetherclime_ApiVersion() == 1` necessarily exposes V3.

## 3. Versioned environmental snapshots

The snapshots use **exact-size, versioned structures**. Callers provide the output pointer plus its `sizeof(...)` in bytes; the function returns `1` when a snapshot was written and `0` for an unavailable context, invalid output or handled error. Do not probe with a wrong structure size or change packing.
- `AetherclimeEnvironmentSnapshotV1` — legacy data and trust flags.
- `AetherclimeEnvironmentSnapshotV2` — includes full `V1` as `.legacy`.
- `AetherclimeEnvironmentSnapshotV3` — includes full `V2` as `.legacy` , and therefore `V1` as `.legacy.legacy`.

**V1** retains its established regional temperature semantics. `AETHERCLIME_ENV_TRUSTED_REGION` means the geographic profile is recognized; it does **not** mean exterior weather has been verified. That is a separate flag, `AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER`. The legacy local ambient value is valid only when `AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE` is set.

**V2** adds official season and optional biome information. `previewAirC` is experimental and does not replace the legacy regional air temperature. Check its availability flag.

**V3** adds accessible temperatures in cases where the earlier regional or explicit-interior model may be uncertain. It does not silently promote the nested V1/V2 results to trusted or verified status.

### Outdoor estimate provenance (V3)
Read `outdoorEstimateC` **only if** `AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE` is set.

| `outdoorEstimateBasis` | Source | Interpretation |
| --- | --- | --- |
| 0 | Unavailable | Do not read temperature |
| 1 | Trusted regional | Recognized regional climate **model** |
| 2 | Provisional regional | Inferred from incomplete nearby geographic evidence |
| 3 | Native biome | Optional polygon/biome evidence |
| 4 | Nearby biome | Nearby biome extrapolation |
| 5 | Broad Tamriel | Large-scale geographic approximation |
| 6 | Generic worldspace | Lowest-confidence worldspace fallback |
| 7 | Nearby climate reference | Surveyed climate influence blended into a broad estimate; **not a proven hold boundary** |

### Indoor estimate provenance (V3)
Read `indoorEstimateC` **only if** `AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE` is set. Confirm the interior flag in `s.legacy.legacy.flags` when using the result for indoor behavior.

| `indoorEstimateBasis` | Source |
| --- | --- |
| 0 | Unavailable |
| 1 | Explicit configured inn model |
| 2 | Inferred building classification |
| 3 | Generic interior model |

Indoor values are **ambient estimates**. They do not detect individual flame effects, fireplaces, body temperature or an independently simulated heat field.
---

## 4. Condition queries

The condition language lets consumer mods express common environmental requirements without reproducing Aetherclime's calculations.

```text
season=summer & outdoorEstimateC>20
interior=true & indoorEstimateC<12
weather=snow & trustedRegion=true
month=6 & (time>=18:30 | time<06:00)
biome=coastal & biomeWeight(coastal)>=0.5
```

Supported operators include `!` (NOT), `&` (AND), `|` (OR), parentheses and comparisons `=`, `!=`, `<`, `<=`, `>` and `>=`, according to field type. Field names are **case-sensitive**; time literals use the documented clock format. Rules are limited to 1–1024 bytes, excluding the final NUL.

```cpp
const auto result = api.Query("season=summer & outdoorEstimateC>20");
if (result.status == aetherclime::client::CallStatus::Ok &&
    result.State() == aetherclime::client::RuleState::True) {
    // Apply your own mod's behavior.
}
```

The evaluated condition has four possible outcomes:

| Status | Meaning |
| --- | --- |
| 0 — False | Valid rule, did not match |
| 1 — True | Valid rule, matched |
| 2 — Unknown | Required evidence unavailable or insufficiently trusted |
| 3 — Invalid | Syntax, field, literal, argument or internal error; inspect `error` and `errorOffset` |

A successful *API call* can still yield **Unknown** or **Invalid**. Don't assume those mean False. Aetherclime's query language is intentionally bounded; it is not a scripting engine.

### Batch evaluation

`Aetherclime_EvaluateConditionsV1(...)` accepts **1–32** rule descriptors and evaluates them against the same captured environment context. The returned batch contains a **V2** snapshot and individual rule results. The batch record itself does not contain V3, even though the condition evaluator supports estimate fields.
---

## 5. Producer and consumer responsibilities

Aetherclime supplies **observations and inferences**, not orders to change the world. Consumer plugins should:
- Perform all calls on Skyrim's game thread and after the provider loads.
- Check export presence, call status, availability flags and provenance separately.
- Treat trusted regional climate differently from verified exterior weather.
- Apply their own confidence policy for clothing, fires, survival, wildlife or visual effects.
- Handle initial loads, carriage travel, cell changes and weather changes without assuming cached values remain valid.
- Never assume a snapshot is continuously updated or subscriptions exist; the current public interface is on-demand.

An absent provider should be handled gracefully. The core requires **no ESP, ESL or ESM**, but a consumer mod may have its own requirements.

## 6. Developer files and support

The **Aetherclime 0.5.0 RC1 — Source & Developer SDK** contains the authoritative `PluginApi.h` and `NativeClient.h`, C++ example consumers, current reference documentation and portable tests.

Start with `docs/release/API_QUICK_START.md`; use the header declarations for definitive structure sizes, flags and call signatures.
For troubleshooting, see the **Aetherclime Troubleshooting** article and include game/SKSE versions, the location and weather, travel method and relevant lines from `Aetherclime.log` when submitting API or climate bugs.

**Validation:** The current development build completed 26/26 independent native API consumer checks in Skyrim, with 56/56 portable regression tests passing on both GCC Release and Clang Debug. Exact game/runtime compatibility and broader performance measurements remain release acceptance checks.
---

*Aetherclime understands the environment. Other mods bring it to life.*
