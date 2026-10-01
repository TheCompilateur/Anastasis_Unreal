# HANDOFF: hydro-network-claude-01

## MISSION

HYDRO_NETWORK_001 : faire du réseau d'eau un bassin versant cohérent (sources → affluents →
rivière principale → lac / exutoire), intégré au relief rendu, sans refaire la carte ni
toucher la génération de terrain. Voir `docs/unreal/HYDRO_NETWORK_001.md`.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.h` / `.cpp` (nouveaux)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisDrainageTests.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp` — appel après TERRAIN_FORGE (mode 2), journal, hook ripisylve
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.h` / `.cpp` — expose `RecomputeNormals` (enveloppe, aucun changement de comportement)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeography.h` / `.cpp` — expose `AuthoredRivers()` (lecture des courbes existantes, aucun changement de comportement)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.h` / `.cpp` — `FRenderedHabitat::SampleRiparian` optionnel (sans lui : comportement d'avant)
- `tools/unreal/hydro-network-capture.ps1` / `.py` (nouveaux), index dans `AGENTS.md`
- `docs/unreal/HYDRO_NETWORK_001.md`, cette fiche, `docs/visual/hydro-network-001/`

## COMMIT

BRANCH_HEAD

## MEC

- BUILD: `BUILD::PASS` (`tools\unreal\anastasis-unreal.ps1 build`, worktree, code final)
- UNITY: prouvé après commit — `Build.bat` direct, fichiers touchés sans changement de contenu,
  aucune exclusion adaptative, `Module.Anastasis_UnrealV2.1.cpp` recompilé, `Result: Succeeded`.
  Les helpers vivent dans `AnastasisDrainage::Detail` (pas d'espace anonyme : pas de collision
  de `SmoothStep` avec TERRAIN_FORGE dans un même lot unity).
- TESTS: `TESTS::PASS` — 135 PASS / 4 KNOWN_EXPECTED_FAILURE / 0 FAIL, 139 annoncés
  (`tools\unreal\report-tests.ps1`), dont les nouveaux :
  - PASS `Anastasis.Terrain.Drainage.Network` (HG actif : tous les contrôles à 0, containment ≥ 0.9, clairsemé, Strahler ≥ 2, largeur et eau décroissantes de la source à l'embouchure)
  - PASS `Anastasis.Terrain.Drainage.OriginalForms` (mêmes contrôles, HG coupé)
  - PASS `Anastasis.Terrain.Drainage.KeepsAuthoredRivers` (≥ 95 % du tracé écrit reste de l'eau)
  - PASS `Anastasis.Terrain.Drainage.DeterminismAndSnapshot` (bit à bit, snapshot de simulation intact, grille incohérente refusée sans écriture)
- Non-régression : `Anastasis.Terrain.*`, `Anastasis.Ecology.*`, `Anastasis.Places.*`, `Anastasis.Sim.Parite.*` sont dans ce même run.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\hydro-network-capture.ps1 -Label final -States "0,1"`

## SCN

PASS (éditeur dédié, `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, capture HighResShot) :

```
ANASTASIS_DRAINAGE enabled=1 rivers=14 heads=9 confluences=8 max_order=3 length_m=5694
  width_m=[4.5,42.0] mouths(river/lake/border)=8/5/1 lakes=7 interior_lakes=5 wetlands=1
ANASTASIS_DRAINAGE check uphill=0 narrowing=0 confluence_narrower=0 dangling_mouths=0
  isolated_water=0 lakes_without_role=0 bank_containment=0.947
```

Captures avant / après, gros plans, lignes de debug largeur / vitesse, HG coupé :
`docs/visual/hydro-network-001/`. Jugement visuel : le réseau se lit d'en haut comme un bassin
versant ; défaut restant visible de près : la rive en escalier (voir Limites dans
`HYDRO_NETWORK_001.md`).

Passe 2 (lacs et mers arrondis, cuvettes closes, initiation aire x pente^2) :

```
ANASTASIS_DRAINAGE enabled=1 rivers=19 heads=14 confluences=9 max_order=3 length_m=6668
  lakes=8 interior_lakes=6 wetlands=2 basin_lakes=1 filled_pits=277
ANASTASIS_DRAINAGE check uphill=0 narrowing=0 confluence_narrower=0 dangling_mouths=0
  isolated_water=0 lakes_without_role=0 bank_containment=0.942   (HG coupe : 0.929)
```

Captures `docs/visual/hydro-network-001/pass2_*`.

## PLY

UNKNOWN — aucune session joueur ; rien n'a été parcouru à pied.

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp` est un fichier chaud (village, dressing, places). L'ajout
  est un bloc contigu après `AnastasisTerrainForge::Apply`, plus une ligne dans le bloc
  écologie.
- Tout consommateur de `SampleActive` / `SampleActiveWater` (dressing, places, village,
  caméras) voit désormais le relief drainé : les tranchées réparées deviennent de la terre,
  les rivières nouvelles de l'eau. Places, village et point haut sont posés après la couche ;
  aucune plaine d'inondation à moins de 220 m du bassin ni du point haut.
- `Type = Water` de simulation ≠ eau rendue sur les tranchées réparées (gameplay inchangé).
- `anastasis.Terrain.Drainage 0` rend l'eau d'avant : c'est l'interrupteur de repli.
- Mode 1 (tranche scellée WORLD_SLICE_006) : non touché.
- `agent/hydra-forge-001` (non commité, 14/09) ajoute des rubans sur la nappe **plate** ; il
  est incompatible avec cette couche tel quel et n'a pas été repris.

## STOP

- Pas le Water plugin, pas de Landscape, pas de Landscape Patch : aucun n'existe ici (audit).
- Pas de modification de la simulation, de la parité JS, ni de `hydrology.js`.
- Pas d'esthétique d'eau (shader, écume, cascades, Niagara, reflets, son).
- Pas d'intégration dans `main` : décision d'Alexandre.
