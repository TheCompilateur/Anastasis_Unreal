# P1.6 — Visual reference R1

## Identity

`REFERENCE_VISUAL_R1::PONTIC_LAKESHORE_SURVIVAL_CAMP`

Source file supplied by the owner:

`C:/Users/alex_/OneDrive/Documents/Unreal Projects/Anastasis_UnrealV2 5.8/Intermediate/Build/Win64/UnrealEditor/Inc/Anastasis_UnrealV2/ChatGPT Image 12 sept. 2026, 00_43_52.png`

The image is an artistic reference, not historical proof, simulation data, or
an implementation instruction. It defines atmosphere, composition, material
readability and social density.

## Observed visual grammar

- humid forested lake or marsh edge;
- irregular shoreline, reeds, mud, puddles and saturated ground;
- steep forest background with distant misted mountains;
- partially cleared woodland, stumps, fallen timber and brush piles;
- temporary shelters and work structures rather than a finished town;
- fishing, drying, cooking, woodworking and storage as visible activities;
- restrained religious presence through a small icon/shrine;
- cold diffuse overcast light balanced by warm firelight;
- dense material detail without fantasy ornament or low-poly geometry.

## Canonical reading

```text
Pontic ecology
  -> displacement / survival
  -> local resource use
  -> communal work
  -> devotional continuity
  -> gradual reconstruction
```

The reference expands the world target from “natural Pontic terrain” to
“natural Pontic terrain inhabited by a materially constrained community.”

## Technical consequences

The visual translator should eventually expose presentation-only fields for:

- forest edge and clearing gradient;
- wetness and shore proximity;
- fallen timber / disturbed-ground opportunity;
- work-zone and settlement-influence masks;
- visual access to water and bank;
- atmosphere depth and fire-versus-daylight contrast.

These fields must remain derived from simulation truth or explicit future visual
configuration. They must not silently become new simulation authority.

## Slice acceptance criteria

The first inhabited visual slice may be considered directionally aligned only if
it can demonstrate:

1. continuous terrain with no visible simulation grid;
2. a readable wet forest-to-shore transition;
3. clearings, stumps or disturbed ground as structured variation;
4. clustered vegetation rather than procedural wallpaper;
5. material separation between mud, wood, stone, cloth and water;
6. cold humid daylight with restrained warm fire contrast;
7. social occupation suggested by a bounded camp/work area, without requiring a
   full settlement or NPC simulation;
8. no generic Western-medieval or fantasy visual language.

## Boundary

R1 does not authorize mass building generation, NPC production, a new biome
system, or changes to `AnastasisSim`. It is a visual target for later proof.
