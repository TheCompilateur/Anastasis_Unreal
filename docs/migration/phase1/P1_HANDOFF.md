# P1 Handoff — génération du monde + parité

Mission: `Ω::ANASTASIS_UNREAL_PHASE_1`. Mode A. Nav / life / ai non commencés.

## 1. Mission

Porter `generateWorld` (archetype knobs sim, hydrologie, plafond forêt, cultures) dans `AnastasisSim`, avec vecteurs JS et tests `Anastasis.Sim.Parite.*`.

## 2. Constat mesuré (2026-09-11, UnrealEditor-Cmd, `-nullrhi`)

| Test | Résultat |
| --- | --- |
| Archetype | PASS |
| Culture | PASS |
| **Monde** | **PASS** (8 cartes 32×32 + 96×96 graine 12345 + 24 tuiles échantillon, alt/shade/type/crop) |
| Libm (sin/log/pow/tanh, échantillons) | PASS |
| Fbm | FAIL 1/5 : `fbm(0.1, 0.2, 12345)` ~2.5 ulp (0x3fd25f9cb5efe582 vs 0x3fd25f9cb5eff8a2) |
| Rng, RngHelpers, Hash, Math, Horloge, GrilleSpatiale | PASS |
| SemantiqueJs | FAIL `ToUint32(1e21)` — test couche 0, hors worldgen |

`Anastasis.Sim.Parite.Monde` vert = critère de fin de `ACTIVE_MISSION.md`. Les altitudes tuile sont du f32 arrondi `round3` : l’écart fbm ci-dessus ne déplace pas les cartes vecteurs.

CRT `sin` = `FMath::Sin` = échantillons V8. L’accumulation fbm sur d’autres arguments n’est pas bit-identique. Ne pas « corriger » les vecteurs. Si le bit-exact fbm double devient un invariant, remplacer `AnastasisJs::Sin` par fdlibm (piste PORTAGE), pas le `.inl`.

## 3. Fichiers

Unreal (`AnastasisSim`) :

- `Public/World/AnastasisWorldNoise.h`
- `Public/World/AnastasisWorldArchetype.h` + `Private/World/AnastasisWorldArchetype.cpp`
- `Public/World/AnastasisHydrology.h` + `Private/World/AnastasisHydrology.cpp`
- `Public/World/AnastasisWorld.h` + `Private/World/AnastasisWorld.cpp`
- `Public/Core/AnastasisJsNumeric.h` (Round / Floor / StoreF32 / Sin / Log / Pow / Tanh)
- tests + `AnastasisParityVectors.inl` régénéré

JS (seul fichier touché, hors travail preexistant) :

- `tools/unreal/gen-parity-vectors.mjs`

Build : `Anastasis_UnrealV2Editor` Win64 Development, `-NoHotReloadFromIDE` (Live Coding de **Final_AnastasisUR** tenait le mutex de `UnrealEditor.exe`). Final n’a pas été fermé ni modifié.

## 4. Interdit / non fait

- Pas de nav, LOD, save, `simulation.js`, life/ai.
- Pas de Three.js / `src/render3d`.
- Pas de carte Unreal / visualisation (MCP absent de cette session).
- `.cursorignore` JS `src/render3d/` toujours non persisté (écriture refusée plus tôt).

## 5. Suite

Phase 2 (nav) seulement sur ordre explicite. Option micro : fdlibm pour verdir `Parite.Fbm`.
