# HANDOFF: pontic-ground-context-002

## MISSION

Donner une matiere photo distincte au passage humain existant et aux berges de
gravier en courant vif. La planche d'Alexandre sert de reference visuelle, pas
de carte PBR exploitable ni de preuve historique.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurface.h`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeography.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeographyTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisRiverbank.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisRiverbankTests.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp`
- `tools/unreal/ground-textures.py`, `ground-material.py`, `ground-cover-capture.py`, `proofs.txt`
- `Content/Anastasis/Materials/GroundTextures/T_Ground_{Path,Gravel}_{AH,NR}.uasset`
- `Content/Anastasis/Materials/M_AnastasisGround.uasset`, `MI_AnastasisGround.uasset`
- `.claude/skills/anastasis-realisme/fiches/sol.md`, `AGENTS.md`

## COMMIT

Ce commit de passation ; SHA exact dans le marqueur `ANASTASIS_WORKTREES/.handoff/pontic-ground-context-002.txt` apres `finish`.

## MEC

- Build initial depuis `main 4ff9c34a`: PASS. Build des changements C++ : `BUILD::PASS`, 13 actions, 138.60 s.
- UV2.x recoit le `RoadWeight` calcule par `HumanGeography` ; UV2.y recoit le gravier de `Riverbank::PaintBanks` sur sol sec, proche de l'eau et en courant vif. Les deux restent des signaux de presentation ; aucune simulation ne change.
- Tests de portee C++ ajoutes : passage sur le noeud du chemin et absence hors route ; gravier sur la bande vive et absence sur la bande calme.
- Sources CC0 : [Stony Dirt Path](https://polyhaven.com/a/stony_dirt_path), [River Small Rocks](https://polyhaven.com/a/river_small_rocks) (Poly Haven). Diffuse, normale DirectX, rugosite, hauteur et AO en 2K ; `Saved/GroundTextures/manifest.json` contient les URL et SHA256 de chaque carte brute. Ces sources ne sont pas archivees dans Git ; les textures Unreal empaquetees le sont.
- SHA256 bruts, dans l'ordre diffuse/normale DX/rugosite/hauteur/AO :
  - Path : `9c35b98628c17382b3a494ae15f17f95410edcaaac2e5922e50e5878f98d4a20`, `93f5cb15037e7241c8e62451985a5d50ec6431981072d9f5857701a1fdc367c3`, `2b9507b060ef9031bd94e97efc6bf876d88583d31bc0a25cb3d16aa8c4bd31c9`, `2cd5ce3501247b4e775ee1b23233156dc1ccb8366269cdc95d8e50aafb442b90`, `85c506ad5256829f873c0eda1b4472c7a27485d9c8f0cc13c9c0c05e51e5138b`.
  - Gravel : `656a20860f5960cc0f442dd425bfe4607052ab1749efdde35d60c5df9639beac`, `7994c91aa29a77692d780b2237ddf209e2d41e687f51c76e2735df8274137029`, `8f9f6104716d3a3af9330dc32905e7f1d5ee3183a7204e2cedecb0b8e3902b42`, `32216ca0d31a15489cb166afff2756ff90c6cac77bb6af0c30bfdebbe2c676bc`, `38e555e168e15eae5416ff279e00f6ab43658bf1d7504b1b9e446bc33edbae36`.
- Tailles physiques retenues : chemin 220 cm ; gravier 290 cm. Passe-haut periodique a 30 cm ; couleur neutre en moyenne, hauteur P1-P99, normales sans pente lente. Le contraste haute frequence du chemin est comprime a exposant 0.45 : pixels ecretes 11.292 % avant correction, 1.262 % apres. Gravier : 0.194 % ecretes.
- `ground-material.ps1 -Rebuild`: `GROUND_MATERIAL::PASS`, 14 textures, 1474 instructions pixel, 164 vertex, 16 samplers. Ce compte atteint le plafond actuel du materiau ; toute nouvelle famille exige un regroupement des lectures.
- `editor-batch.ps1 -Proofs pontic-path-capture,pontic-gravel-capture` : `pontic-path-capture` PASS instrumental (137.2 s). `Saved/PonticPathEvidence/` contient six images et `ground-cover.json`.
- Comparaison au meme `pass_ground` : avant/apres, 41.44 % des pixels RGB >16/255 ; avant/temoin, 0.96 %. En prairie : 0.71 % avant/apres, 1.52 % avant/temoin. Ces mesures localisent l'effet du scan.
- GPU p50 `pass_ground` : 13.166 ms avant, 13.309 ms apres, 13.003 ms temoin. Cette derive ne permet pas de chiffrer un cout propre au scan.
- Premiere capture gravier : `pontic-gravel-capture` PASS instrumental (162.0 s). `bank_ground` avant/apres : 28.38 % de pixels >16/255 ; avant/temoin 1.94 %. Prairie avant/apres : 2.44 %, avant/temoin 2.18 %. Le scan de sol agit bien au site, sans expansion detectee sur la prairie.
- Sonde galets 3D : remplacer leur aplat par `M_AnastasisRock` a compile et la preuve gravier a ete recapturee (`EDITOR_BATCH::PASS 1/1`). Les memes cameras (SHA256 de `cameras.json` identique) montrent quelques fissures de plus, mais les silhouettes anguleuses et l'aspect artificiel persistent. **REJECT** : code revenu a l'aplat initial. Les images de la sonde rejetee restent dans `Saved/PonticGravelRockProbe/`, celles de l'etat livre dans `Saved/PonticGravelEvidence/`.
- Build de l'etat final apres rejet de la sonde galets : `BUILD::PASS`, 4 actions, 42.29 s. Le build et la suite du portail `finish` sont a lire dans son marqueur et ses journaux : le present document est commite avant leur execution pour figer les sources testees.

## PROOFS

PROOFS: pontic-path-capture, pontic-gravel-capture

## SCN

`pass_ground` : KEEP local. Inspection directe des trois images : le sol fissure
et generique devient un melange de terre compacte et petits cailloux lisibles au
premier plan. La teinte de simulation reste, le site revient au temoin et aucune
texture de chemin n'apparait sur la prairie controle. Pas de repétition evidente
dans cette pose. Le trajet complet en marche et la perception a moyenne distance
restent inconnus.

`bank_ground` : KEEP de la **texture au sol**. Le scan de gravier est actif,
revient au temoin et ne gagne pas la prairie. Verdict artistique du site :
PARTIAL, car les galets 3D en aplat gris et l'herbe dense masquent une partie du
grain. Une sonde avec `M_AnastasisRock` ne resout pas leur silhouette et a ete
rejetee. La correction des meshes de galets est un autre chantier, a juger avec
son propre A/B visuel.

## PLY

UNKNOWN. Marche humaine a vitesse normale non realisee.

## INTEGRATION_RISK

- `ground-material.py` et ses deux `.uasset` sont des autorites partagees ; rebase/reconstruction si une autre mission les change sur `main`.
- 16 samplers sur le materiau compile : ne pas ajouter une autre famille par simple duplication.
- Deux preuves image declarees ; `editor-batch` les execute dans deux editeurs sequentiels selon `EDITOR_BATCH_SPLIT_001`.
- Les cartes Unreal doivent rester suivies par Git LFS. Le manifest brut en `Saved/` reste une preuve locale non commitee.

## ECARTS

AUCUN : aucun changement dans `Source/AnastasisSim/`.

## STOP

Cette mission couvre deux contextes visibles de sol. Les autres vignettes de la
planche, dont champ, sable cotier et neige, exigent une autorite spatiale et un
budget d'echantillonnage propres ; elles ne sont pas promises par ces captures.
