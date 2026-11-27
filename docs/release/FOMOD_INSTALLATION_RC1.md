# RC1 FOMOD installation and reinstallation

The FOMOD has exactly one mutually exclusive choice: **Framework Only** (recommended, silent HUD) or **Minimal HUD** (90-second cooldown, heartbeat off). No second installation and no plugin slot are required.

The text in each option shows the actual configuration path: `SKSE\Plugins\Aetherclime\config.json` and explains that changing `hud.mode` later is supported. **Rerunning the FOMOD may overwrite the entire config.** It does not merge existing JSON, even though the separate developer deploy script does.

## First installation

In MO2 or Vortex, install `Aetherclime-0.5.0-RC1-FOMOD.zip` using the mod manager. Confirm the FOMOD presents precisely two HUD options and select one. The resulting **Data-relative** files should be exactly:

```
SKSE/Plugins/Aetherclime.dll
SKSE/Plugins/Aetherclime/config.json
```

No `AetherclimeApiProbe.dll`, `.esp`, `.esl`, `.esm`, or source/build files are part of the selected runtime payload. Startup diagnostics are written to the normal SKSE log directory.

## Changing HUD later

Edit the `hud.mode` field in the active mod's `SKSE\Plugins\Aetherclime\config.json`: `off`, `minimal`, `detailed`, `debug`. The installer doesn't need to be run again. The native HUD message duration remains game-managed.

## Upgrade and reinstall safety

Back up your personalized `config.json` before selecting another FOMOD preset or replacing the mod in MO2/Vortex. A new preset **replaces**, not merges, the JSON file. That is useful if you deliberately want a full reset. To keep settings, install the new DLL carefully and restore/merge the old config after reviewing new keys in `Documentation/CONFIGURATION.md`.

**Verification status:** Both HUD options were installed and checked in **Mod Organizer 2** during RC1 preparation. The final cleaned-configuration FOMOD has also been smoke-tested with Framework Only in MO2. **Vortex installation and a second install of the final archive using Minimal HUD have not been independently rechecked.**
