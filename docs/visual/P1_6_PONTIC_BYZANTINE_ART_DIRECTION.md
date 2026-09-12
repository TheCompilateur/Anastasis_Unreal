# P1.6 — Pontic / Byzantine art direction

This document is a visual constraint system for a believable post-1204
Rhomaioi settlement world. It is not a building asset brief and does not
authorize mass content production.

## Visual pillars

1. Humid Pontic ecology: steep green valleys, forested ridges, wet soils,
   broken shorelines, stream cuts and mist depth.
2. Material truth: stone, timber, lime/plaster, clay tile, earth, moss and
   leaf litter read through scale, roughness and weathering.
3. Society in the land: paths, clearings, fields and future structures emerge
   from use, repair and terrain constraints.
4. Cinematic restraint: natural contrast, grounded exposure and atmosphere that
   reveals scale rather than hiding weak assets.

## Forbidden languages

No low-poly/faceted terrain, tile checkerboard, cube landscape, generic fantasy
castle pack, generic Western medieval village, Mediterranean dryness, tropical
jungle, Skyrim-like heroic mountains, decorative Byzantine fantasy, blue water
tiles, uniform procedural wallpaper, or LUT/bloom used to counterfeit material
quality.

## Terrain identity

Continuous terrain must interpolate simulation elevation without exposing tile
edges. Use slope, curvature, moisture, shore proximity and seed to derive
macroform and restrained microrelief. Valleys are humid and irregular; ridges
are forested; rock exposures follow slope and erosion logic; clearings and
worked ground interrupt vegetation density.

## Vegetation identity

Canopy, secondary trees, shrubs, young growth, grasses, ferns, deadwood, stones
and litter form clustered ecologies with edges, gaps and succession-like
variation. Forest is not a repeated tree stamp. Density responds to semantic
forest, slope, moisture, altitude, water proximity and seed.

## Water identity

Water is a body with shoreline, bank transition, wetness and riparian life, not
a terrain color. Rivers/streams should cut and gather; shorelines should be
irregular. Reflection and fog must support water/terrain continuity.

## Material identity

Terrain uses layered soil, grass, wet grass, mud, rock, moss, forest litter,
gravel, shore and cultivated earth. Variation comes from macro scale, normal
detail, roughness and wetness; color alone is insufficient. The material system
must remain cheap enough to profile.

## Atmosphere identity

Temperate Black Sea air: soft haze, humid depth, mist pockets, credible daylight,
complex forest shade, restrained local exposure. Avoid universal god rays,
crushed blacks, oversaturation and bloom spectacle.

## Architectural identity (future boundary)

Future structures must support timber/stone domestic construction, lime/plaster,
regional roof families, ecclesiastical forms, workshops, agricultural
infrastructure, retaining walls, terraces, courtyards, wells, storage and
defensive elements when socially justified. Buildings are deferred in P1.6;
the pipeline must represent material culture, not place generic assets.

## Scale and realism rules

Read scale through human movement, slope, tree height, bank width, mist falloff
and material texel density. Every visual enhancement must answer a perceptual
question. AAA means coherent information density, not maximum feature count.

## Technical consequences

The player renderer consumes a versioned semantic snapshot. Visual derivations
are deterministic and read-only. The debug view remains available and is never
used as the player surface. Fixed seed/camera/time captures are mandatory for
comparison. Runtime proof is required; package or headless success is not a
visual verdict.

