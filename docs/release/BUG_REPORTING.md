# Aetherclime — bug reporting guide (0.5.0 RC1)

Thank you for helping improve Aetherclime. A concise, reproducible report is more useful than a long unrelated log. Please review [TROUBLESHOOTING.md](TROUBLESHOOTING.md) before reporting a startup or missing-HUD issue.

## Copy-and-paste issue template

```text
Title: [e.g. Temperature jumps near Winterhold when walking outdoors]
Aetherclime version: 0.5.0 RC1 (or exact version installed)
Skyrim runtime / edition:
SKSE64 version:
Mod manager / modlist: (name is enough initially)
FOMOD option: Framework Only / Minimal HUD / customized JSON
Location / nearest landmark:
Cell type: Interior / exterior / unknown
In-game month, season, time, weather:
Previous location and travel method: walked / carriage / fast travel / loaded save
Expected result:
Actual result:
Steps to reproduce:
Relevant Aetherclime.log excerpts (include timestamps):
Other climate / weather / interior mods, if relevant:
```

## What makes a useful report?

- **Context:** Distinguish the native game temperature estimate from a visible weather effect produced by a different mod. Aetherclime itself does not apply snow shaders or alter weather.
- **Evidence:** Include `outdoorBasis`, `indoorBasis`, availability flags and `referenceInfluence` when present. An estimate may legitimately be low confidence rather than a confirmed region observation.
- **Transitions:** Say whether the anomaly followed walking, loading, fast travel, carriage, entering a building, or returning outdoors. This helps distinguish geographic evidence gaps from stale travel state.
- **Version:** The engine log marker for 0.5.0 RC1 remains `AETHERCLIME_BUILD_ALPHA50_RELEASE_READINESS`; it does not mean you installed the older public release version.
- **Reproduction:** Two or three clear steps and a small timestamped excerpt usually suffice. For a crash, include the relevant crash log if available, but do not assume Aetherclime caused it without evidence.

**Privacy:** Review logs for user-specific paths, usernames and unrelated data before posting. Do not share a full modlist or saves unless needed to reproduce the issue.

## What happens next?

Reports are assessed by reproducibility and impact. We may request an outdoor save-time `Climate estimate audit` or a short route test, but do not need exhaustive exploratory walking from every user. Some estimated values are intentionally approximate; clear provenance is part of the API contract, not a guarantee that every modeled temperature matches a player's expectation.
