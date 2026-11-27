# Aetherclime - Environmental Framework (SKSE)

**Aetherclime understands the environment. Other mods bring it to life.**

> **Short About text:** A read-only SKSE environmental framework for Skyrim: calendar seasons, regional climate, indoor temperature estimates, geography and a native API. No ESP/ESL/ESM required.

## Description

Skyrim's weather, calendar, mountains, coastlines, homes and caves are rich in environmental context. **Aetherclime** makes that context available through a shared, versioned native interface so other mods can respond to it without reproducing the same climate modeling.

It is an **environmental data framework**, **not a weather overhaul**. Aetherclime observes available game conditions and produces modeled temperatures and seasonal context. Its fallback estimates identify their own evidence and uncertainty instead of presenting guesses as verified measurements.

Temperatures account for seasonal regional trends, time of day, available weather evidence and appropriate geographic information. An unknown region can receive a broad or nearby climate-reference estimate; an ordinary home, ruin, cavern, or mine can receive a distinct modeled indoor ambient estimate without hand-authoring every location. These are *estimates*, not instruments measuring actual fires or rooms.

## Installation instructions

1. Install the archive through MO2 or Vortex. Launch the game through the matching Skyrim/SKSE64 runtime.
2. Choose **Framework Only (Recommended)** for no in-game HUD messages, or **Minimal HUD** for short optional notifications with a 90-second cooldown.
3. You can change your choice later by editing `SKSE\Plugins\Aetherclime\config.json` (`hud.mode`: `off`, `minimal`, `detailed`, `debug`).
4. **Back up your config before reinstalling.** Re-running the FOMOD may replace your customized JSON. See the included `Documentation/CONFIGURATION.md`.

## Main features

- **Native SKSE framework; no ESP/ESL/ESM slot required for the core.**
- **Configurable Tamriel-calendar seasons** independent of Seasons of Skyrim. This does *not* perform season-based asset or LOD swapping.
- **Regional, weather-aware and time-aware climate modeling** with provenance and evidence-sensitive estimates.
- **Generic indoor ambient estimates** for common classes of interior, with exceptional location overrides where needed.
- **Geographic fallback estimates** for areas where the exact hold or biome can't be confidently identified.
- **Public C API and C++ client helper**, including snapshot V1/V2/V3 and a condition query language.
- **Read-only by design:** no weather writes, NPC changes, seasonal texture/mesh swaps, or gameplay effects.
- **On-demand/event-driven design**, with the diagnostic HUD off by default; measured FPS-performance claims remain pending validation.

**Potential uses by future companion mods:** regional clothing distribution, survival conditions, fires and lighting, vegetation behavior, wildlife ecology and more. These are *examples*, not integrations currently shipped with Aetherclime.

## Requirements

- **Tested:** Skyrim Special Edition / Anniversary Edition **1.6.1170** with **SKSE64 2.2.6**, on Windows x64 (HexRim 1.5.0 and Journals of Jyggalag 8.2.0). Other Skyrim runtime builds and Skyrim VR have not been qualified.
- **Microsoft Visual C++ Redistributable v14 (x64):** install the [latest supported Microsoft package](https://aka.ms/vc14/vc_redist.x64.exe). The DLL dynamically imports Microsoft C/C++ runtime components. The developer's installed build is not an established minimum.
- A mod manager such as MO2 or Vortex is recommended for the FOMOD installer. **Both FOMOD choices were tested in MO2; Vortex has not been independently tested.**
- No Seasons of Skyrim installation required for Aetherclime's own season calculations.

**Compatibility note:** An SKSE DLL can be sensitive to runtime/dependency versions; no claim of universal compatibility with arbitrary modlists, weather systems, or unusual worldspaces is made. Report problems with your game/SKSE version, modlist, and relevant `Aetherclime.log` excerpts.

## For mod developers

See `Documentation/API_QUICK_START.md` and `Documentation/API_REFERENCE.md`. The versioned native API provides climate snapshots and clear availability/estimate-quality indicators; the condition language supports composable queries such as `season=summer & outdoorEstimateC>20`. Query outcomes distinguish true, false, unknown, and invalid. Native calls must run on Skyrim's game thread.

## Shout outs, credits and transparency

Aetherclime is designed, directed and extensively in-game tested by its maintainer, with substantial generative-AI assistance for source code, tests and documentation. **AI-Generated Content** accurately describes the code contribution; **AI Media** is applicable to AI-generated promotional page media or description. Source and test code are published for review. Constructive technical reports and contributions are welcome.

The source repository is MIT-licensed. Dependencies and any externally sourced creative assets retain their own licenses. No third-party weather, outfit or shader author is represented as having endorsed or integrated Aetherclime.

## How do I know it is working?

With **Framework Only** selected, Aetherclime intentionally makes no visible gameplay or weather changes. The optional Minimal HUD defaults to Celsius; change `hud.temperatureUnit` to `F` or `both` in `SKSE\Plugins\Aetherclime\config.json` if preferred.

Initialization is logged in `Aetherclime.log`, under the game's SKSE log directory within Documents / My Games. In tested installations, these included `Skyrim Special Edition\SKSE` and `Skyrim.INI\SKSE`. See [Troubleshooting](TROUBLESHOOTING.md) for discovery commands and sample log filters; see [Bug Reporting](BUG_REPORTING.md) for a concise report format.

## What is not included

Aetherclime does not automatically apply snow cover, replace weather or seasonal landscape assets, recolor grass, distribute outfits, manage fires, or supply visual seasonal LOD variants. Future companion mods may implement such behavior by consuming the API.

---

**Publishing checklist:** Runtime versions and Microsoft VC++ x64 requirements are now documented. Confirm final author/credit details, the appropriate Nexus permissions and AI tags, and at least one accurate in-game screenshot before publishing. Avoid graphics suggesting Aetherclime currently changes visual weather or season assets.
