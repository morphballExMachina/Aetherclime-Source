# Installation and compatibility — 0.5.0 RC1

**Status:** Aetherclime 0.5.0 RC1. The GitHub repository contains source and developer files, not an installable DLL. The runtime FOMOD is distributed separately through Nexus. The RC1 binary has been packaged and smoke-tested in Mod Organizer 2; public publication is a separate release decision.

## Requirements

- **Tested game/SKSE combination:** Skyrim Special Edition / Anniversary Edition **1.6.1170** with **SKSE64 2.2.6** (the version reported by MO2 in **HexRim 1.5.0** and **Journals of Jyggalag 8.2.0**). The HexRim `skse64.log` independently reports `SKSE64 runtime: initialize (version = 2.2.6 ...)` and discovers `Aetherclime.dll`; successful operation is additionally supported by prior Aetherclime initialization and in-game smoke tests. Other Skyrim runtimes and Skyrim VR have not been qualified.
- A supported Windows/x64 Skyrim installation and a mod manager such as MO2, or a manual SKSE plugin installation.
- **Microsoft Visual C++ runtime:** the latest supported **Visual C++ v14 Redistributable (x64)** from [Microsoft](https://aka.ms/vc14/vc_redist.x64.exe). The compiled DLL imports `MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` and Universal C Runtime components, alongside Windows system DLLs. The dependency list was inspected with `dumpbin /dependents`; this is not a claim that every target machine has those DLLs installed.
- **No ESP/ESL/ESM slot** is used by the Aetherclime core package. The repository's default runtime payload is `SKSE/Plugins/Aetherclime.dll` and `SKSE/Plugins/Aetherclime/config.json`.
- Developer diagnostic `AetherclimeApiProbe.dll` is optional at runtime. The build/deploy workflow requires it for testing, and stores it at `SKSE/Plugins/Aetherclime/Diagnostics/` rather than among game-loadable SKSE plugins.

## Installing the RC1 runtime FOMOD

1. Exit Skyrim.
2. Install the release archive as one mod, preserving `SKSE/Plugins/` paths. **Do not install a source-preview ZIP as if it were a compiled game mod.**
3. Enable the mod in your manager and launch via SKSE.
4. Confirm `Aetherclime.log` contains the release marker and successful initialization. Logs are written to the SKSE log directory returned by your Skyrim runtime (typically the Skyrim user documents SKSE folder).
5. Leave the HUD off for regular use. Enable `hud.mode="minimal"` only if you want short status notifications.

## Configuration

`SKSE/Plugins/Aetherclime/config.json` controls seasons, climate profiles and opt-in diagnostics. This is **JSON, not JSON-with-comments**; validate syntax after editing. Existing configurations are preserved by the included developer deployment tool, which merges missing defaults without replacing current values. Back up your config before manual upgrades.

Important defaults:

```json
{
  "hud": { "mode": "off", "heartbeatSeconds": 0.0 },
  "settings": {
    "eventClimateEnabled": false,
    "apiConsumerProbeEnabled": false,
    "experimentalLiveMonitor": false
  },
  "interior": { "enabled": true, "estimateUnclassified": true }
}
```

These are **fragments** from a full config; do not replace the entire template with this excerpt. If upgrading an old installation with `interior.enabled=false`, explicitly change that value to `true` to enable generic indoor estimates.

## Recorded build and runtime dependencies

- Windows release DLL built with **Visual Studio 2026**, using the MSVC **14.51** toolset at `VC/Tools/MSVC/14.51.36231` and CMake-reported C++ compiler `19.51.36260.0`.
- On the tested developer machine, the installed x64 Redistributable was `14.51.36247.00` and the plugin ran successfully in Skyrim. This is **an observed installed version, not an independently established minimum Redistributable version**. Microsoft recommends the latest supported x64 v14 package, at least as recent as the applicable build tools.
- The vcpkg build triplet is `x64-windows-static-md`; the six audited third-party copyright/license notices are included in the FOMOD. Microsoft C/C++ runtime libraries are dynamically linked.

## Recorded RC1 smoke tests

- **HexRim:** enabled/disabled whole-process CPU and memory samples on the Helgen-to-Riverwood route; no obvious whole-process CPU regression was observed. This is **not** a frame-time or FPS neutrality claim.
- **JOJ:** load/save in a Whiterun exterior and a mod-added interior, The Dancing Horse. The exterior resolved a trusted regional temperature and the interior returned a labeled generic indoor estimate.
- **Final cleaned-config installation:** Framework Only at Riverwood and Riverwood Trader produced a trusted exterior estimate and a building-class indoor estimate with `perFramePolling=false` and `readOnly=true`.
- **Mod managers:** the two FOMOD choices have been tested in MO2 during RC1 preparation. Vortex installation has **not** been independently tested.

## What works together?

- **Weather overhauls:** Aetherclime reads available live weather information; it does not overwrite weather records. **Specific overhaul compatibility remains to be tested.** Modded weather categories or special scripts may produce unknown or estimated values.
- **Seasons of Skyrim:** Not required for Aetherclime's own calendar-based season labels and climate estimates. Aetherclime does **not** replace SoS asset/LOD swaps or automatically synchronize unusual external season configurations; test combined use separately.
- **Custom interiors and worldspaces:** Generic ambient modeling usually supplies indoor estimates based on available classification hints. Mod-added worldspaces without calibrated Tamriel geography receive explicitly rough generic estimates. Misclassification can occur.
- **ENB, Community Shaders, Bottled Shaders, grass/LOD mods:** Aetherclime does not install a shader pipeline or modify assets; it currently **does not automatically turn their seasonal graphics on or off**. No official integration is implied.
- **Outfit, survival and other gameplay mods:** No automatic interaction or new ESP records. Another mod must explicitly consume Aetherclime's native API or conditions.

## Troubleshooting

| Symptom | First check |
|---|---|
| No Aetherclime log | SKSE plugin path, correct DLL and runtime, required dependencies, SKSE log directory |
| HUD silent | Default is Off; diagnostics still run at load/save; opt into HUD if wanted |
| Old milestone reported | An earlier DLL is winning the MO2 file conflict; inspect MO2 **Data** tab |
| Interior unavailable | Interior enabled? Settled parent cell? Check `Climate estimate audit` for `indoorEstimateAvailable` |
| Sudden warm northern fallback | Check `Climate fallback transition:` and `Climate estimate audit:` logs for basis, raw XY, reference influence |
| No automatic fallback transition logs | Set either the HUD on or `settings.eventClimateEnabled=true` to register event sampling; load/save audits work without either |
| API consumer probe absent | Probe defaults off. For testing, enable `settings.apiConsumerProbeEnabled=true`, restart and make one manual save |

## Performance and VR limitations

No per-frame player-update hook is installed by the **default** configuration, but event samples, on-demand API requests, enabled biome previews and save-time debug diagnostics still have computational cost. No numeric FPS or overhead guarantee has yet been established. Experimental monitor options can add load. Skyrim VR cell-source support has specific unverified offsets; **do not claim VR compatibility** before validation.

## Permissions and attribution

This repository carries the MIT license. Third-party integrations must respect the applicable licenses and permissions of all mods involved. See the AI-development disclosure in the repository README and on the Nexus mod page.


## RC1 FOMOD packaging

The two HUD choices and reinstall overwrite warning are documented in [FOMOD_INSTALLATION_RC1.md](FOMOD_INSTALLATION_RC1.md) and [CONFIGURATION.md](CONFIGURATION.md). A source ZIP is not an installable runtime archive: Windows source builders must compile the DLL and then run `tools/Package-AetherclimeRC1.ps1`.

**Detailed diagnostics:** [TROUBLESHOOTING.md](TROUBLESHOOTING.md) and [BUG_REPORTING.md](BUG_REPORTING.md).
