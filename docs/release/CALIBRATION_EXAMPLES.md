# Optional calibration and research examples (not release defaults)

The RC1 shipped `config.json` deliberately omits or empties former experiments. The following data comes from the original prototype configuration and is preserved for developers, **not** as a recommended set of universally valid mappings.

- `geography.provisionalSamples` and `geography.atlasZones`: bounded Tower Doomstone survey experiments. Enabling their associated settings can affect location evidence; neither entry is a surveyed hold boundary.
- `geography.nativeRegionProfiles`: three *experimental* record-to-profile associations for native region-shadow work. This is **not** a complete region atlas; `nativeRegionShadowEnabled` remains off in RC1. A native biome preview, if enabled for research, has additional prerequisites.
- `elevation.references`: four local XYZ reference observations. The elevation correction is off in RC1; these are not province-wide altitude measurements.
- `interior.locations`: two earlier named inn overrides. Generic indoor estimation is now on by default; add a named override only to correct an exceptional interior and validate it in-game.
- `regions`: historical Whiterun and Winterhold parameter overrides. The same seasonal means and Winterhold snow bias are present in the compiled defaults, so these overrides are redundant for RC1.

**These are JSON fragments, not a complete drop-in configuration file.** Merge only the appropriate section or array into a copy of the current config; back it up first. Merely adding example entries does not necessarily enable the related feature, and toggling a feature on without validated inputs can change climate behavior. Do not replace the installed config with this document.

```json
{
  "geography": {
    "provisionalSamples": [
      {
        "label": "Tower Doomstone northern-coast observation",
        "regionKey": "winterhold",
        "worldspaceFormId": 60,
        "xRaw": 72751.8,
        "yRaw": 105616.2,
        "radiusRawUnits": 8192
      }
    ],
    "atlasZones": [
      {
        "label": "Tower Doomstone pilot tile",
        "regionKey": "winterhold",
        "worldspaceFormId": 60,
        "minXRaw": 66607.8,
        "maxXRaw": 78895.8,
        "minYRaw": 99472.2,
        "maxYRaw": 111760.2
      }
    ],
    "nativeRegionProfiles": [
      {
        "plugin": "Skyrim.esm",
        "localFormId": 1113542,
        "regionKey": "winterhold"
      },
      {
        "plugin": "Skyrim.esm",
        "localFormId": 1066829,
        "regionKey": "falkreath"
      },
      {
        "plugin": "Skyrim.esm",
        "localFormId": 809048,
        "regionKey": "whiterun"
      }
    ]
  },
  "elevation": {
    "enabled": false,
    "references": [
      {
        "label": "Winterhold reference",
        "regionKey": "winterhold",
        "worldspaceFormId": 60,
        "xRaw": 109542.4,
        "yRaw": 104511.7,
        "zRaw": -8988.3,
        "radiusRawUnits": 49152.0
      },
      {
        "label": "Falkreath reference",
        "regionKey": "falkreath",
        "worldspaceFormId": 60,
        "xRaw": -32305.9,
        "yRaw": -88372.0,
        "zRaw": -3163.6,
        "radiusRawUnits": 49152.0
      },
      {
        "label": "Whiterun tundra reference",
        "regionKey": "whiterun",
        "worldspaceFormId": 60,
        "xRaw": 3924.9,
        "yRaw": 84.9,
        "zRaw": -5788.9,
        "radiusRawUnits": 49152.0
      },
      {
        "label": "Markarth reference",
        "regionKey": "reach",
        "worldspaceFormId": 60,
        "xRaw": -170997.1,
        "yRaw": 5964.2,
        "zRaw": -4001.2,
        "radiusRawUnits": 49152.0
      }
    ]
  },
  "interior": {
    "locations": [
      {
        "name": "The Frozen Hearth",
        "regionKey": "winterhold"
      },
      {
        "name": "Dead Man's Drink",
        "regionKey": "falkreath"
      }
    ]
  },
  "regions": {
    "whiterun": {
      "seasonalMeanC": [
        -4,
        9,
        21,
        9
      ]
    },
    "winterhold": {
      "seasonalMeanC": [
        -16,
        -5,
        5,
        -5
      ],
      "permanentSnowBiasC": -2.5
    }
  }
}
```

See [Geography and elevation](GEOGRAPHY_AND_ELEVATION.md) and [Configuration](CONFIGURATION.md).
