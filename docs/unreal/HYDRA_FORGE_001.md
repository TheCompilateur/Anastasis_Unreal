# HYDRA_FORGE_001 — grammaire hydrologique visuelle

Présentation seulement. Le simulateur (`AnastasisHydrology`, `FTile::FlowAmt`,
`Shore`, `Wetness`, `Alt`) reste la vérité. Unreal ne calcule pas un second
écoulement.

## Pourquoi pas le Water plugin

Le sol incarné est un `ProceduralMeshComponent` (TerrainSurface + TerrainForge),
pas un Landscape. Water Body River / Lake déforment un Landscape. Les activer
ici créerait un second système d'eau, divergent, collé à rien.

Outil retenu : trois sections supplémentaires du même ProceduralMesh, lues par
`M_AnastasisSlice` (couleur de sommet = sémantique, déjà en production).

## Archétypes (seed 12345, monde 96×96)

Classification d'une tuile d'eau, sans donnée nouvelle :

1. **Still** — `Type==Water` et `FlowAmt < 0.06` (seuil sim).
2. **Inflow** — eau courante 4-voisine d'un plan d'eau stagnant ≥ 8 tuiles.
3. **Torrent** — eau courante étroite (`≤ 4` voisins eau / 8) et berges hautes
   (`max Alt terre − SeaLevel ≥ 0.04`).
4. **Valley** — le reste de l'eau courante.

Mesure sur le canonique :

| archétype | tuiles | relief moyen | largeur 3×3 |
|---|---|---|---|
| torrent | 177 | 0.210 | 0.311 |
| valley | 99 | 0.076 | 0.787 |
| inflow | 113 | — | — |
| still | 809 | — | — |

## Géométrie

- Section 2 — rubans d'écoulement, Z = nappe de mer + 3.5/5 UU, A=1 (speculaire).
- Section 3 — bande de berge humide (terre → rive), A=0 (mate). Sommets terre
  recalés sur le sol forgé (`SampleActive`).
- Section 4 — roches de chenal, roseaux d'embouchure, écume aux ruptures
  (relief de berge ≥ 0.12).

CVar : `anastasis.Dressing.Hydrology` (0 = nappe plate seule).

## Tests

`Anastasis.Hydrology.*` — six tests, tous PASS. Le snapshot n'est pas muté.
Les tuiles courantes sont exactement celles du seuil sim 0.06.

## Captures

`tools/unreal/hydra-capture.ps1` → `docs/visual/hydra-001/`.
