# Geography, Skyrim coordinates, and optional elevation correction

Aetherclime uses Skyrim's **worldspace-relative game coordinates** as supporting evidence for climate. This is intentionally different from claiming to know the elevation, latitude, or exact climate-hold boundary of every point in Skyrim.

## Why does the default config mention only three geographic anchors?

The `geography.anchors` entries for Winterhold, Falkreath, and Whiterun are **locally observed calibration neighborhoods**, not a list of every place Aetherclime can recognize. They supplement the built-in location/hold resolver. Other holds have built-in seasonal regional profiles and can be resolved through location evidence. When the precise region is uncertain, V3 can provide a lower-confidence geographic/worldspace estimate with an explicit provenance basis.

An anchor has `worldspaceFormId`, `xRaw`, `yRaw`, `radiusRawUnits`, `regionKey`, and a human-readable `label`. It represents only a bounded XY neighborhood. **Its radius is not a hold border**, and an extrapolated outdoor estimate must not be mistaken for verified weather or trusted hold identity. Removing the release's three anchors could alter geographic fallback results, so RC1 keeps them.

The separate `provisionalSamples` and `atlasZones` arrays were used during research around the Tower Doomstone; both corresponding research systems are **disabled by default**. The RC1 template explicitly sets them to `[]`. Earlier sample values are preserved in [Calibration examples](CALIBRATION_EXAMPLES.md).

## X, Y, and Z in the game

- `xRaw` / `yRaw`: horizontal position in Skyrim's game units, used for distances and choosing a suitable nearby reference.
- `zRaw`: vertical position in game units. **Not meters above sea level.** Negative Z is possible and perfectly normal.
- `worldspaceFormId`: identifies the worldspace; a Tamriel reference must not be applied to a different worldspace. Vanilla Tamriel uses form ID `0x0000003C` (decimal `60`).

Exterior Z is meaningful here only **relative to a surveyed reference Z in the same worldspace and resolved climate profile**. Indoor cell coordinates are excluded; they are not assumed to be exterior heights.

## The optional reference-relative elevation model

When `elevation.enabled` is `true` and a matching nearby reference is available, the model chooses the nearest valid XY reference that matches the resolved climate `regionKey` and worldspace, and falls within that reference's configured radius. It declines to make a correction when data is missing, outside the calibration neighborhood, invalid, or ambiguous.

For a valid reference:

```text
relativeRiseMeters = (playerZRaw - referenceZRaw) / rawUnitsPerMeter
coolingC = -altitudeLapseCPerKm * max(0, relativeRiseMeters) / 1000
```

The model bounds the rise with `maxRiseMeters` and caps cooling at `maxCoolingC`. The default `rawUnitsPerMeter = 70` is an approximate game-units-to-meters scale, **not an absolute altitude datum**. Built-in regional climate profiles currently use a lapse rate of `6 °C/km` for this model. A location *below* its reference is not automatically warmed.

**Worked example (illustrative, not a measured mountain):** If the player's Z is 7,000 game units above a valid local reference, at 70 units/meter that is a 100-meter relative climb. At 6 °C/km the projected temperature adjustment is **−0.6 °C**. The model's default bounds are 2,000 meters of effective rise and 12 °C of cooling.

### Release defaults and safety

The RC1 package ships with **`elevation.enabled = false` and `elevation.references = []`**. Therefore no elevation correction is applied. The four historical Winterhold/Falkreath/Whiterun/Markarth references are kept as [optional developer examples](CALIBRATION_EXAMPLES.md), not silently enabled for users.

To experiment, a developer must deliberately add validated references, enable the model, and test climate readings on actual exterior terrain. References should not be copied indiscriminately to fill out the nine holds; a mismatched reference could introduce temperature discontinuities. The feature does not continuously poll the player or modify the game world by itself.

## Inspecting the model

The SKSE log contains `Elevation audit` records at supported diagnostic triggers, including `status`, `enabled`, `eligible`, `applied`, `profile`, `rawZ`, `referenceRawZ`, `riseRelativeM`, `projectedOffsetC`, and `actualOffsetC`. `projectedOffsetC` describes an eligible proposed adjustment; **`actualOffsetC` is the correction applied**, which remains zero when elevation is disabled. With the RC1 empty reference list, `no-calibration-for-profile-worldspace` is an expected status, not an error.

This feature is separate from Aetherclime's broad north–south geographic approximation, the V3 nearby-climate fallback, and the experimental biome preview. None of those should be described as a complete province-wide altimeter.

See [Configuration](CONFIGURATION.md) and [Climate model in plain English](CLIMATE_MODEL_PLAIN_ENGLISH.md).
