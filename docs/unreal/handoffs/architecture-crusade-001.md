# HANDOFF: architecture-crusade-001

## MISSION

Grande croisade architecturale (mandat d'Alexandre, 2026-10-07) : refaire le langage bati du village a
l'echelle humaine. Audit, convention d'echelle (`docs/unreal/ARCHITECTURE_SCALE_001.md`), maison de
reference, kit modulaire, grammaire de typologies, implantation terrassee, catalogue fonctionnel separe
du visuel, preuve PIE avant / apres.

Cause systemique trouvee : les meshes du village etaient modeles pour une tuile de 4 m alors qu'une tuile
de simulation fait 20 m a l'ecran (`TileWorldSize` 400 x `anastasis.WorldView.Scale` 5). Maison de 3,6 m
au milieu d'une parcelle de 20 m, porte de 146 cm libres pour des habitants de 158-168 cm, toit a 45 deg,
pignons ouverts, collision convexe. Corrige a la source (generateur), pas par une echelle.

## FILES_OWNED

- `tools/unreal/create-village-architecture.py` + `.ps1` (generateur, kit, grammaire, validation ARCH-01..10, materiau)
- `tools/unreal/architecture-preview.py` (banc hors Unreal, z-buffer logiciel, silhouettes 170 cm)
- `tools/unreal/architecture-pie.py` + `.ps1` (preuve PIE, registre `architecture-pie`)
- `Content/Anastasis/VillageArchitecture/**` (7 maisonnees + 7 assises, 23 pieces de kit, `M_AnastasisArchitecture`) -- genere
- `Source/Anastasis_UnrealV2/Village/AnastasisArchitecture.{h,cpp}` (catalogue fonctionnel, choix de typologie, fiche vivante)
- `Source/Anastasis_UnrealV2/Village/AnastasisArchitectureTests.cpp`
- `Source/Anastasis_UnrealV2/Village/AnastasisVillageBuilding.{h,cpp}` (assise, archetype, terrasse, foyer dans l'atre, Neglect du materiau)
- `Source/Anastasis_UnrealV2/Village/AnastasisVillagePresentation.{h,cpp}` (TraceGround, SettleArchitecture, CVars)
- `Source/Anastasis_UnrealV2/Sim/AnastasisSimulationSubsystem.{h,cpp}` (scenario `Anastasis.Village.Hamlet`, `Anastasis.Village.ArchitectureReport`)
- `docs/unreal/ARCHITECTURE_SCALE_001.md`, `docs/unreal/architecture/architecture-kit-001.json`
- `AGENTS.md` (index), `tools/unreal/proofs.txt` (une ligne)

## COMMIT

PENDING

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree)
- TESTS: `Automation RunTests Anastasis.Village` (worktree, 2026-10-07) : 17 PASS, 0 KNOWN_EXPECTED_FAILURE, 0 FAIL,
  dont les 4 nouveaux `Anastasis.Village.Architecture.{Echelle,Rapport,Variante,Terrasse}`. Suite complete : au lot.
- ASSETS: `create-village-architecture.ps1 -Rebuild` -> `ARCH::PASS` (7 corps avec 3 LOD, 7 assises, 23 pieces de kit, materiau sans `Failed to compile` apres le marqueur)
- ECHELLE (hors Unreal) : `python tools/unreal/create-village-architecture.py` -> `ARCH_GEOMETRY PASS buildings=7 kit=23 failures=0`

| Maisonnee | Corps (cm) | Triangles | Porte principale (l x h libre) | Faitage |
|---|---|---|---|---|
| maison pauvre | 986 x 1161 x 455 | 22 k | 96 x 192 | 4,3 m |
| maison moyenne (reference) | 1278 x 1689 x 845 | 74 k | 140 x 212 (rez), 100 x 196 (etage) | 8,0 m |
| ferme | 1755 x 1749 x 839 | 89 k | 260 x 215 (portail), 240 x 255 (grange) | 8,1 m |
| grenier communautaire | 1654 x 1441 x 800 | 60 k | 240 x 260 | 7,6 m |
| puits | 491 x 672 x 402 | 3 k | -- | 2,8 m |
| atelier (labo) | 871 x 726 x 475 | 26 k | baie 680 x 290 | 4,6 m |
| chapelle (labo) | 1358 x 786 x 1055 | 44 k | 140 x 280 | 10,4 m (clocher) |

Triangles apres chanfrein des moellons : pauvre 24 k, moyenne 89 k, ferme 109 k, grenier 67 k (3 LOD par corps : 100 / 45 / 18 %).

- PREUVE PIE : `tools\unreal\architecture-pie.ps1` -> `ARCH_PIE::PASS` (2026-10-07, worktree), 13 prises dans
  `Saved/ArchitectureEvidence/pie/` (2 AVANT aux memes cameras, 11 APRES). Hameau `Anastasis.Village.Hamlet 6 10` :
  puits, 3 maisons pauvres, 2 moyennes, 1 ferme, 1 grenier ; chaque corps `SM_Arch_*` + `_Footing`.

| Batiment | Archetype | Terrain sous l'emprise (cm) | Cour posee a | Decalage | Instances ecartees |
|---|---|---|---|---|---|
| building-0 | well | 1155..1167 | 1159 | +1 | 54 |
| building-1 | house_farm | 1211..1397 | 1264 | 0 | 106 |
| building-2 | house_poor | 1035..1127 | 1086 | -5 | 83 |
| building-3 | house_medium | 1140..1194 | 1157 | +1 | 185 |
| building-4 | house_poor | 1242..1289 | 1263 | +4 | 53 |
| building-5 | house_medium | 1223..1358 | 1278 | -9 | 96 |
| building-6 | house_poor | 1161..1314 | 1237 | +8 | 67 |
| building-7 | storehouse | 1379..1596 | 1488 | -63 | 137 |

  Denivele sous emprise jusqu'a 2,2 m (grenier) : terrasse a la mediane, soutenement de 4,8 m suffisant.
  Interieur (03) pris a +3 EV (`r.ExposureOffset 3`, cette prise seulement) : l'exposition du projet est fixe.

## PROOFS

PROOFS: architecture-pie

## SCN

`Anastasis.Village.Hamlet [Houses=6] [NpcCount=10] [X Y]` (nouveau, scenario de preuve : puits, maisons en
grappe sur parcelles voisines, grenier rempli de 40, habitants proprietaires). `Anastasis.Village.ArchitectureReport`
journalise une ligne `ANASTASIS_ARCH record` par batiment.

## PLY

Inchange. La collision du corps est complexe (on passe les portes) ; le joueur minimal peut entrer.

## ECARTS

AUCUN -- `Source/AnastasisSim/` n'est pas touche. Le choix de typologie (phase + graine de l'identifiant)
vit dans la presentation, n'ecrit rien dans la simulation et ne tire rien de `sim.rng`.

## INTEGRATION_RISK

- `AnastasisVillageBuilding.cpp` / `AnastasisVillagePresentation.cpp` sont chauds (iceberg-001, abandon-001
  y ont ecrit). Conflit probable seulement si une mission en cours retouche `SetNeglect` ou la boucle de spawn de `Sync`.
- `SetNeglect` : avec un archetype, le MID est cree sur `M_AnastasisArchitecture` (qui porte `Neglect`), plus
  sur `M_VillageBuilding_Aged`. `abandon-pie` lit toujours un MID en slot 0 avec `Neglect` : contrat tenu, a
  rejouer au lot.
- `metabolism-pie` : le foyer est desormais pose dans l'atre de l'archetype (etage de la maison moyenne), plus a
  1,2 m du centre ; la lumiere sort par les fenetres et la galerie. La preuve lit le composant, pas sa position.
- Le defrichement passe les instances d'herbe / sous-bois / arbres de l'emprise a l'echelle zero (indices
  inchanges). Un systeme qui REECRIT ces transforms plus tard les fera repousser (aucun vu a ce jour).
- `anastasis.Village.Architecture 0` rend l'ancien rendu (temoin).

## STOP

- Ne revendique pas un rendu « AAA » : materiaux procéduraux HLSL (pas de textures photo), pas de decalques,
  pas de mobilier fin. Le bati porte desormais l'echelle, la structure et la fonction ; la matiere reste le
  chantier suivant (voir la conclusion du rapport).
- `HousePhase` reste a 1 dans le port : la variete des maisons vient de la graine, pas encore de l'histoire.
- Atelier et chapelle n'ont pas de type de simulation : ils existent en kit et en laboratoire, pas au village.
- Les PNJ n'entrent pas visuellement (la carte se cache « dedans ») : l'interieur est pret, pas habite.
