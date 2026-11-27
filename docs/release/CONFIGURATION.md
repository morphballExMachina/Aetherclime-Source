# Aetherclime configuration guide — RC1

**Actual runtime path:** `SKSE\Plugins\Aetherclime\config.json` (relative to Skyrim's `Data` directory). The file must remain **strict, valid JSON**. JSON comments (`//`, `/* */`) are not supported and should not be added. The reference below explains options without risking the game parser.

## FOMOD choice and later changes

The two FOMOD choices install the **same Aetherclime.dll** and differ only in `hud.mode`:

- **Framework Only (recommended):** `hud.mode = "off"` — no on-screen notices; full API functionality.
- **Minimal HUD:** `hud.mode = "minimal"`, `hud.cooldownSeconds = 90`, `hud.heartbeatSeconds = 0` — short status messages only on eligible observation events.

After installing, open `SKSE\Plugins\Aetherclime\config.json` and edit the `hud.mode` string to `off`, `minimal`, `detailed`, or `debug`. Restart Skyrim for predictable configuration pickup. Re-running the FOMOD can **overwrite the existing customized JSON**; back it up first. A reinstall can also be used deliberately as a settings reset.

The developer PowerShell deploy script takes precautions to preserve existing JSON keys, but **that does not apply to mod-manager FOMOD reinstallations**. You should not assume a FOMOD merges settings.

## Primary HUD settings

| Key, inside `hud` | Default | Meaning |
|---|---|---|
| `mode` | `off` | No HUD (`off`), short notices (`minimal`), fuller (`detailed`), or developer (`debug`). |
| `temperatureUnit` | `C` | Display units: `C`, `F`, or `both`. This does not change internal modeled °C values. |
| `cooldownSeconds` | `90` | Minimum real-time seconds between eligible notifications. **Not** on-screen duration. |
| `loadGraceSeconds` | `8` | Avoids immediately presenting messages while a save is settling. |
| `sampleIntervalSeconds` | `2` | Minimum interval between eligible samples, not a per-frame polling schedule. |
| `temperatureDeltaC` | `3` | Sensitivity threshold for temperature-based HUD change decisions. |
| `heartbeatSeconds` | `0` | `0` disables repeated unchanged status reminders. |
| `sampleOnMenuClose` | `true` | Allows deferred sampling after supported menus close. |
| `showOnFirstObservation` | `true` | First valid eligible observation may display after grace/cooldown. |
| `showProvisionalEstimates` | `false` | Controls some low-confidence HUD messages; does not remove estimates from the API. |
| `debugInitializationMessage` | `false` | Applies only to `debug` HUD mode; disabled in the shipped RC1 presets. |

**Skyrim controls native notification visibility duration and font styling**. Aetherclime's `cooldownSeconds` doesn't extend them.

## Safe framework settings

| Key, inside `settings` | Default | Notes |
|---|---|---|
| `geographicFallbackEnabled` | `true` | Retain lower-confidence outdoor estimates where region evidence is incomplete. |
| `eventClimateEnabled` | `false` | Optional cell/location event observations and bounded fallback-transition logging; not needed for normal on-demand API use. |
| `apiConsumerProbeEnabled` | `false` | Developer-only independent DLL probe; test builds only. Disable after native API checks. |
| `experimentalLiveMonitor` | `false` | Developer monitoring research; do not enable casually. |
| `experimentalMovementProbe` | `false` | Developer-only movement diagnostics; can add work/log traffic. |
| `geographicContinuityEnabled` | `false` | Research gate for legacy regional continuity; **not** the same as the independent V3 fallback mechanism. |
| `geographicSurveyEnabled` / `geographicAtlasEnabled` | `false` | Opt-in geographic surveying/atlas experiments, not needed for trusted V1 published region resolution. |
| `nativeRegionShadowEnabled` / `nativeRegionGeometryExportEnabled` | `false` | Experimental geometry/shadow paths. |

Other settings such as `experimentalBoundaryProbe`, `experimentalWeatherCorrelationProbe`, `temperatureContinuityPreviewEnabled`, and movement/report limits are diagnostics or research controls; keep their supplied defaults unless you are developing or troubleshooting the framework.

## Indoor ambient models

`interior.enabled = true` and `interior.estimateUnclassified = true` are recommended: Aetherclime can infer an ambient estimate for most identified interior cells even without a hand-authored rule. The `baselineC`, `outdoorCoupling`, `minAmbientC`, and `maxAmbientC` properties describe the **generic physical model**; the default package uses conservative values. Climate values are estimates, not measured fire heat. The shipped RC1 template sets `interior.locations = []`: earlier named inn overrides have been moved to developer examples. You can add a named override for an exceptional case; **you do not need to register every building**.

## Seasons, geography, and experimental model inputs

- `months` maps Tamriel's month names to `WIN`, `SPR`, `SUM`, `AUT`. This is calendar-based and does not require Seasons of Skyrim.
- `regions` is an optional override table (empty in the RC1 template). Built-in profiles cover all nine holds plus a temperate fallback; an empty table does **not** disable regional climate. Edit calibrated parameters only if you understand the modeling implications.
- `geography.anchors` retains three **active, local** calibration neighborhoods, not a province-wide atlas or a list of the only modeled holds. `provisionalSamples` and `atlasZones` are empty in the RC1 template because their research features are disabled. **A geographic estimate is not automatically a trusted hold reading.**
- `biomeClimate.enabled = false` preserves the conservative release default. Other `biomeClimate` profiles and gradients are prepared for experimental/opt-in modeling; their existence does not mean the biome system has been promoted to the published region model.
- `elevation.enabled = false` and `elevation.references = []` mean no altitude cooling is applied in the RC1 preset. The optional Z-coordinate correction is reference-relative, not a guaranteed world altitude model. See [Geography and elevation](GEOGRAPHY_AND_ELEVATION.md) and [Calibration examples](CALIBRATION_EXAMPLES.md).
- `geography.nativeRegionProfiles = []` excludes partial developer-only native-region mappings from the release preset. Populate it deliberately if testing the opt-in native region atlas; no complete atlas is implied.

## Why do some locations appear only in the examples?

RC1 keeps the three local geography anchors because they still contribute to actual climate resolution. It deliberately excludes the Doomstone pilot, research-only region mappings, four inactive altitude calibration points and old inn exceptions. Their historical values remain in [Calibration examples](CALIBRATION_EXAMPLES.md). The model still handles other locations using trusted record evidence, built-in hold profiles, and labeled V3 fallbacks.

**Loader detail:** the C++ loader starts with built-in defaults. For these list fields, *omitting* the key restores the compiled samples, while an explicit empty array (`[]`) clears them. Do not delete the empty arrays if you want the release preset to remain free of those prototypes.

## Editing safely

1. Back up the active `config.json` (especially before reinstalling or using a new preset).
2. Use a text editor, not a word processor. Keep double quotes and comma placement intact.
3. Validate JSON, for example in PowerShell: `Get-Content -LiteralPath '.\config.json' -Raw | ConvertFrom-Json | Out-Null`.
4. Restart the game and check the `Aetherclime.log` initialization report. A malformed config should never be treated as a valid customization.

The full initial config is included in the mod archive. Avoid replacing it with short documentation fragments.
