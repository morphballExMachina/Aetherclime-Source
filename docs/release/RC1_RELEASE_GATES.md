# Aetherclime 0.5.0 RC1 — release gates and evidence

**Status:** The compiled 0.5.0 RC1 FOMOD has been uploaded to the unpublished Nexus page. Public release approval and a source tag remain separate actions. The curated GitHub repository contains source and documentation, not the runtime DLL.

## Verified native and regression results

- **Independent DLL consumer:** 26 passed, 0 failed, 0 skipped; `exportsMask=63`, `externalDll=true`, `coreLinked=false`, `readOnly=true` in the Alpha-50/RC1 test campaign.
- **Portable C++ regression tests:** 56/56 in GCC Release and 56/56 in Clang Debug, including the configuration-polish verification run. These are not a substitute for the Windows SKSE smoke test.
- **Windows SKSE observations:** the validated Alpha-50 release-readiness marker loaded, published V1/V2 snapshots were unchanged in audits, and load/save context diagnostics passed in tested settings.
- **Default behavior:** the framework is read-only, with HUD off, no default per-frame player-update hook, and modeled confidence/provenance reported separately from verified weather.

## Installed FOMOD and user-facing checks

- [x] Windows FOMOD package generated with a validated Windows x64 DLL and six audited third-party copyright/license files.
- [x] Two installer presets demonstrated in MO2 during RC1 preparation: Framework Only (`hud.mode=off`) and Minimal HUD (`hud.mode=minimal`, 90-second cooldown, no heartbeat).
- [x] Cleaned release JSON verified: three bounded geographic anchors, empty optional pilot arrays, elevation off, experimental biome off, generic interiors on.
- [x] Final cleaned-config Framework Only smoke test: Riverwood trusted exterior (`outdoorEstimateAvailable=true`) to Riverwood Trader (`indoorBasis=building-class-estimate`, `indoorEstimateAvailable=true`), with `readOnly=true` and `perFramePolling=false`.
- [x] Updated RC1 FOMOD uploaded to the *unpublished* Nexus page.
- [x] Rebuilt RC1 archive inspected: SHA-256 matched, DLL hash matched the validated build, both HUD presets and JSON paths verified, release docs matched the then-current source, six dependency notices present, and no unexpected plugin/probe DLLs.
- [x] Both selections of that rebuilt FOMOD were separately installed and inspected in a temporary MO2 test mod. **Vortex has not been independently verified**.
- [ ] Regenerate and recheck the archive once more **after this final runtime-documentation update**, then replace the unpublished Nexus upload with that exact checked archive.
- [x] Record the tested game runtime **Skyrim 1.6.1170** and the MO2-reported **SKSE64 2.2.6** in HexRim 1.5.0 and JOJ 8.2.0.
- [x] Loaded HexRim `skse64.log` confirms **SKSE64 2.2.6** initializing and discovering `Aetherclime.dll`. Earlier successful game and climate audits establish plugin operation; plugin discovery alone is not sufficient evidence of successful initialization.
- [x] Inspect release DLL imports with `dumpbin /dependents`: Microsoft C++ v14 and Universal C Runtime imports, plus Windows system DLLs. Record MSVC toolset path `14.51.36231`, compiler `19.51.36260.0`, and developer x64 v14 Redistributable `14.51.36247.00`. This observed installed version is not a proven minimum for end users.
- [ ] Include Skyrim **1.6.1170**, SKSE64 **2.2.6**, and Microsoft's **latest supported x64 Visual C++ v14 Redistributable** in Nexus requirements, with a note that other runtimes/VR are unqualified.
- [ ] Final Nexus copy, screenshots, permissions/credits, AI disclosure, and download version check.
- [ ] Create an annotated source tag only after the final curated GitHub revision is pushed and reviewed.

## Performance and compatibility evidence

- **HexRim:** repeatable Helgen cave exit to Riverwood walk/sprint, comparing disabled, Framework Only and Minimal HUD. An initial enemy-confounded baseline was replaced with a cleaner repeat. Whole-process CPU and memory did not reveal an obvious regression. This test **did not capture frame times**, so do not advertise zero/neutral FPS impact.
- **JOJ:** Whiterun exterior and the mod-added Dancing Horse inn loaded and saved successfully. Its unclassified interior received a **generic**, not inn-specific, indoor estimate (`indoorEstimateAvailable=true`).
- **Final package:** cleaned config loaded at Riverwood and modeled Riverwood Trader without a hand-authored interior rule.
- **Limitations:** these are short smoke tests, not comprehensive compatibility certification. Skyrim VR, arbitrary alternate runtimes, custom weather frameworks, Vortex installation and all mod-added worldspaces remain unqualified unless separately tested.

## Packaging on the Windows development machine

Build from the current Windows x64 RC1 source and use the compiled DLL; the curated GitHub source does not contain the binary:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Build-Aetherclime.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Package-AetherclimeRC1.ps1
```

Output: `release-output/Aetherclime-0.5.0-RC1-FOMOD.zip` plus its `.sha256` sidecar. The packaging script verifies the x64 PE marker, versioned vcpkg copyright notices and required FOMOD members, and refuses to overwrite an existing archive. **Do not upload a source ZIP as a Nexus runtime file.**

The native climate engine remains at the validated Alpha-50 release-readiness marker; the final configuration polish modifies JSON, documentation and packaging metadata, not native climate algorithms.
