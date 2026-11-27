# Aetherclime native API — quick start

**Framework release:** Aetherclime 0.5.0 RC1. API struct/export versions are independent of the mod version.

**Audience:** SKSE C++ plugin authors. The API is a **synchronous native DLL interface**, not a Papyrus API, magic console command or automatic integration. All calls must occur on Skyrim's **main/game thread** and only after Aetherclime is loaded.

## 1. Include the SDK

Copy these headers from the source archive into your include path:

```text
include/Aetherclime/PluginApi.h
include/Aetherclime/NativeClient.h
```

The optional helper uses C++17 and does **not** link against AetherclimeCore, SKSE or CommonLib. Its `Client` borrows function pointers, not ownership of the DLL.

## 2. Discover Aetherclime

On Windows, from your plugin's game-thread callback:

```cpp
#include <Windows.h>
#include "Aetherclime/NativeClient.h"

// Do not call this from DllMain or a worker thread.
const HMODULE module = GetModuleHandleW(L"Aetherclime.dll");
const auto client = aetherclime::client::Client::Discover(
    [module](const char* symbol) {
        return module ? GetProcAddress(module, symbol) : nullptr;
    });
if (client.Status() != aetherclime::client::CallStatus::Ok) {
    // Provider missing, incompatible or discovery failed.
    return;
}
```

`Status()==Ok` checks the legacy API version, not the presence of every optional export. Use `SnapshotV3()` and its returned `CallStatus`, or inspect `ExportMask()` bit `32`, before relying on V3. **ApiVersion remains 1**, because the optional exports are versioned individually.

## 3. Read a temperature *and its provenance*

```cpp
const auto reply = client.SnapshotV3();
if (reply.status == aetherclime::client::CallStatus::Ok) {
    const auto& s = reply.value;
    if (s.flags & AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE) {
        const double degreesC = s.outdoorEstimateC;
        const bool trusted = s.outdoorEstimateBasis == 1; // trusted regional
        // 2..7 are inferred / lower-confidence tiers. Apply your own policy.
        (void)degreesC; (void)trusted;
    }
    if ((s.flags & AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE) &&
        (s.legacy.legacy.flags & AETHERCLIME_ENV_INTERIOR)) {
        const double ambientC = s.indoorEstimateC;
        // This is a modeled estimate, not sampled fireplace heat.
        (void)ambientC;
    }
}
```

For older providers, `SnapshotV3()` returns `MissingExport`. For an unsettled player context, it can return `Unavailable`. Do not treat **0°C as the unavailable sentinel**: the corresponding availability flag is authoritative.

V3 contains `legacy` (a full V2 record), which itself contains `legacy` (V1). Its `outdoorEstimateBasis` values are:

| Code | Basis | Policy hint |
|---:|---|---|
| 0 | Unavailable | Do not read value |
| 1 | Trusted regional | Established region model, **not** a weather measurement |
| 2 | Provisional region | Lower-confidence region interpolation |
| 3 | Native biome | Enabled experimental polygon evidence |
| 4 | Nearby biome | Nearby extrapolation, lower confidence |
| 5 | Broad Tamriel | Rough latitude-like approximation |
| 6 | Generic worldspace | Lowest-confidence fallback |
| 7 | Nearby climate reference | Surveyed reference blended into broad fallback; not a hold boundary |

Indoor basis: `0` unavailable, `1` explicit configured inn, `2` inferred building class, `3` generic interior. More detail is in [API_REFERENCE.md](API_REFERENCE.md).

## 4. Evaluate a condition

```cpp
const auto rule = client.Query("season=summer & outdoorEstimateC>20");
if (rule.status == aetherclime::client::CallStatus::Ok &&
    rule.State() == aetherclime::client::RuleState::True) {
    // Your mod can act on this. Aetherclime itself changes no game state.
}
```

`RuleState`: False `0`, True `1`, Unknown `2`, Invalid `3`. **Only True should cause an action.** A successful API call can still produce an Unknown or Invalid rule result. `QueryBatch()` can evaluate up to 32 rules against one captured V2 context; its nested snapshot does **not** include V3, although newer estimate fields are supported by the query evaluator.

## 5. Production guidance

- Use the **availability flags and basis codes**, not string labels, for decisions.
- Prefer a single capture for decisions that must be simultaneous. Separate snapshots/queries may see different game moments.
- Cache UI text or results only with a clear invalidation policy; a prior weather value is not guaranteed current.
- Do not call the APIs on a background thread, from DllMain, or before plugin initialization.
- The provider is read-only; **all gameplay effects and safety checks belong to your plugin**.
- The source includes a compiled, SDK-only example: `examples/EnvironmentalSnapshotV3Example.cpp`.
