# Aetherclime 0.5.0 RC1 — troubleshooting and log commands

**Audience:** Players and mod authors. You do not need PowerShell to use Aetherclime. Its default **Framework Only** installation is intentionally silent: it changes no visible weather, objects, NPCs, or UI. Another mod must consume the native API for gameplay effects.

## 1. Check that the plugin is installed

Use your mod manager's **Data** tab (MO2) to find the winning file at `SKSE\Plugins\Aetherclime.dll`. There should also be `SKSE\Plugins\Aetherclime\config.json`. Do not mistake the source ZIP for an installable runtime mod. Launch Skyrim through the matching SKSE64 loader. Check that your installed Skyrim/SKSE versions meet the release's tested compatibility list (to be finalized before publication).

The two FOMOD selections install an identical DLL. `Framework Only` sets `hud.mode` to `off`, and `Minimal HUD` sets it to `minimal` with Celsius (`temperatureUnit: "C"`), a 90-second notification cooldown and heartbeat disabled. You can edit `SKSE\Plugins\Aetherclime\config.json` later. JSON must be valid and must not contain comments. **Back it up before reinstalling:** FOMOD reinstallation may overwrite it.

## 2. Find `Aetherclime.log`

The active SKSE log directory varies by installation. In local testing the log was found at:

- `Documents\My Games\Skyrim.INI\SKSE\Aetherclime.log`
- `Documents\My Games\Skyrim Special Edition\SKSE\Aetherclime.log`

These are observations, not guaranteed paths. The Documents folder may be redirected. To locate the newest file automatically, open **PowerShell** and paste:

```powershell
$myGames = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games'
$log = Get-ChildItem -LiteralPath $myGames -Filter Aetherclime.log -File -Recurse -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($null -eq $log) { Write-Warning 'Aetherclime.log was not found under Documents\My Games.' }
else { $log.FullName }
```

The following commands assume `$log` contains the file returned above. If no log was found, first confirm that the **active** MO2 mod provides the DLL and the game launched with SKSE.

## 3. Verify initialization

```powershell
Select-String -LiteralPath $log.FullName -Pattern 'Aetherclime milestone:' | Select-Object -Last 1
```

For **0.5.0 RC1**, the engine marker is still `AETHERCLIME_BUILD_ALPHA50_RELEASE_READINESS`. The marker names a validated engine milestone, **not** the user-facing release version; this is expected. Repeated older markers may indicate a conflicting DLL.

## 4. Find configuration and runtime problems

```powershell
Select-String -LiteralPath $log.FullName -Pattern 'Config rejected','Context contract failure','status=failed','\[error\]','\[warn\]' |
    Select-Object -Last 40
```

A search with no matches is **not** proof of universal compatibility. Many modding issues are not logged as errors by Aetherclime.

## 5. Inspect environmental observations

```powershell
Select-String -LiteralPath $log.FullName -Pattern 'Climate estimate audit:','Indoor climate audit:','Climate fallback transition:','Travel context reset:' |
    Select-Object -Last 40
```

- `Climate estimate audit:` reports available V3 estimates and their provenance, typically on load/save.
- `Indoor climate audit:` can show both legacy inn-model eligibility and a newer generic indoor estimate. `eligible=false` in the legacy fields is not automatically a failure if `indoorEstimateAvailable=true`.
- `Climate fallback transition:` captures select low-confidence changes only when optional HUD or climate event subscriptions are enabled; the event audit is bounded per session.
- `Travel context reset:` indicates that Aetherclime discarded stale continuity information after a large geographical jump.

The HUD may not show every cell change because its notification cooldown and event triggers are independent of climate availability. `heartbeatSeconds=0` disables periodic re-announcements. With the default HUD off and event subscriptions off, **absence of event-by-event entries is expected**; load/save audits are still useful.

For a compact HUD-only inspection (if Minimal HUD is enabled):

```powershell
Select-String -LiteralPath $log.FullName -Pattern 'HUD notification:' | Select-Object -Last 20
```

## 6. Share only relevant excerpts

```powershell
Select-String -LiteralPath $log.FullName -Pattern 'Aetherclime milestone:','Climate estimate audit:','Indoor climate audit:','Climate fallback transition:','Travel context reset:','status=failed' |
    Select-Object -Last 60 |
    Out-File -FilePath (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Aetherclime-diagnostics.txt') -Encoding utf8
```

You may instead attach the complete `Aetherclime.log` when asked. Review logs before uploading them publicly: filenames, local paths, character names, mod details, and other data may be present. **Do not upload savegames, private system paths or a full modlist unless they are relevant and you are comfortable sharing them.**

## 7. When a temperature seems wrong

See [BUG_REPORTING.md](BUG_REPORTING.md). Include the location, in-game season and weather, the displayed estimate and whether you walked, loaded directly, fast-traveled or used a carriage. A save near the location can produce useful audits, but never overwrite your only important save for testing.

**Limitations:** These logs are diagnostics, not performance benchmarks. A read-only design and the absence of default per-frame polling do not prove zero FPS impact. Experimental settings can change overhead and log volume.
