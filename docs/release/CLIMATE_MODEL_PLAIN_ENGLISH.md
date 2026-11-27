# How Aetherclime estimates the environment

Aetherclime is **not** a weather replacement and **not** a thermometer. Its primary work is to interpret the current game context and offer a plausible, explainable environmental model.

## Seasonal climate

The game calendar gives Aetherclime a month and time. A configurable month mapping selects winter, spring, summer or autumn. Individual geographic profiles use different seasonal temperature expectations; Whiterun and Winterhold are not assigned identical climates. The model also considers time of day and available weather classification. Configured regional anomalies introduce bounded variation. The result is an **estimate**, not a forecast.

This calendar capability does not need Seasons of Skyrim. It also does not replace seasonal mesh swapping, plant swaps or distant LODs.

## When a region is uncertain

The existing regional resolver gives a named profile when location or trusted geographic survey evidence supports one. It may also produce a bounded provisional reading. Aetherclime keeps these separate from **secondary estimates** for locations where the exact hold is unclear.

Depending on enabled features and available evidence, a secondary outdoor estimate may use native biome geometry, nearby biomes, a nearby configured climate reference, a broad north–south Tamriel approximation, or a very rough custom-worldspace baseline. Lower-confidence temperature estimates do **not** confer trusted hold identity or verified weather. Geography is grounded in Skyrim's coordinates but is **not** a real-world calibrated map or a physical altitude measurement.

The three default geography anchors are surveyed **local neighborhoods**, not a complete list of mapped holds or their borders. For how XY coordinates relate to the optional, currently disabled Z-coordinate elevation model, see [Geography and elevation](GEOGRAPHY_AND_ELEVATION.md).

The low-confidence reference shoulder added in Alpha-49.1 was motivated by a real route near the College of Winterhold: an earlier broad estimate jumped from snowy 2°C to 16°C when a location label disappeared. Nearby surveyed climate references now soften that loss of context without extending trusted hold boundaries.

## Weather and travel

Aetherclime treats an exterior game-Sky observation differently from an unverified interior Sky. An exterior observation can be cached only under the established trust rules; moving indoors does not magically make an interior Sky pointer into current outdoor weather. A sufficiently large position/worldspace discontinuity clears stale geographic continuity and cached exterior weather. The destination must be observed anew.

There are normal circumstances in which weather or regional identity is *unavailable*. Consumers should inspect flags and estimate basis rather than assuming the provider knows everything.

## Inside a building

Aetherclime can recognize broad categories such as dwellings, halls, caves, mines and ruins using imperfect record and name hints. If no explicitly configured location applies, it provides a **generic modeled ambient temperature**. Underground places tend to be less sensitive to short-term outside changes; occupied shelters tend to be more temperate. The specific value can be wrong for a mod-added interior or unusual structure.

The model does **not** measure fire heat, insulation, occupancy, room-by-room airflow or thermal lag. An explicit named rule remains available for exceptional cases; there is no requirement to catalog every interior in Skyrim.

## Confidence matters

An API consumer might be comfortable using an inferred temperature for an NPC's cloak choice but require verified weather before removing exposed fire particles. The framework supports those different policies: **estimated value**, **reason for the estimate**, and **trust/availability flags** are intentionally separate.

The V1/V2 contracts retain their established published regional meanings; the additive V3 snapshot exposes secondary outdoor/indoor estimates. A condition such as `outdoorEstimateC<5` can return unknown if the estimate is absent; it does not imply that the reading is verified weather.

## What is not modeled yet?

Surface snow accumulation, shaders, tree foliage recoloring, object/LOD swaps, actual flame states, fire heat, NPC clothing changes, survival effects and wildlife behavior are outside the first release. Those are possible future consumers of the environment API, not implemented Aetherclime features.
