# HANDOFF: horizon-blend-001

## MISSION

Mesurer l'anneau d'horizon (horizon-ring-001) sous l'atmosphère réaliste
d'env-realism-001, désormais sur `main`, et corriger ce qui relève de l'anneau.

Mesure (`capture-horizon.ps1 -Label realism -Atmosphere -PreCmds 'anastasis.Sky.Clock 0'`,
soleil fixe du rig, perspective aérienne ×3, nuages, brume volumétrique) : part de pixels
« vide », anneau coupé (A) puis actif (B) :

| Vue | A | B |
|---|---|---|
| H3 au bord, regard dehors | 25,3 % | 1,8 % |
| H4 vue générale | 60,1 % | 2,6 % |
| H5 150 m au-dessus du bassin | 5,5 % | 0,8 % |

La chaîne lointaine bleuit (perspective aérienne). Défaut restant, imputable à l'anneau :
vu d'en haut, l'anneau est un **désert taupe** uniforme et la limite de la carte se lit.
Cause : la teinte lointaine était la moyenne de toute la terre forgée, sol de forêt
compris (famille Litter, brune).

Correction : `AnastasisTerrainHorizon` emprunte maintenant ses teintes et canaux lointains
aux **prairies** de la carte (herbe = 1 − Rock − Litter − Worked ≥ 0,6, Wetness < 0,35),
coupées en deux palettes par luminance (prairie verte / prairie sèche), mélangées en
mosaïque à ~900 m de longueur d'onde. Worked tombe à 0 hors de la carte.

Corrigé en passant : `capture-horizon.ps1 -States B` aplatissait le plan d'états (sortie
d'un `if` passée au pipeline) — états « 1 » puis « B », suffixe d'image vide.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainHorizon.h/.cpp`
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainHorizonTests.cpp` (test `Palette`)
- `tools/unreal/capture-horizon.ps1`

## COMMIT

PENDING

## MEC

- BUILD: PASS (worktree)
- TESTS: `Anastasis.Terrain.Horizon.{Seam,Closed,Gentle,NoSlivers,Palette}` PASS ;
  `HORIZON_PALETTE donors=38664 lush=(0.125,0.162,0.084) dry=(0.267,0.341,0.167)
  far_mean=(0.198,0.255,0.127) old_all_land=(0.178,0.201,0.124)`. Suite complète : `finish`.
- Captures : `Saved/HorizonEvidence/realism` (avant) et `realism-meadow` (après, état B,
  vide H4 2,6 %, H3 1,7 %, H5 0,9 %).

## SCN

`Lvl_AnastasisSlice`, graine 12345, atmosphère du profil appliquée comme en PIE,
`anastasis.Sky.Clock 0`.

## PLY

NOT_IMPLEMENTED

## INTEGRATION_RISK

Faible : seules les couleurs et canaux de l'anneau changent, sa géométrie est identique
(tests Seam/Closed/Gentle/NoSlivers inchangés au chiffre près).

## STOP

- Au bord même (H3), le sol reste beige sur les ~600 m du fondu : c'est le rempart
  rocheux du bord de carte (`ShapeMountainProfile` du simulateur) que l'anneau prolonge,
  beige aussi sur la carte. Non touché.
- L'anneau n'a ni arbres ni herbe : la limite entre la forêt de la carte et la prairie nue
  de l'anneau reste lisible d'en haut.
- Tout autre banc de capture posé au sol hors de la carte est sous l'anneau : masquer le
  composant `HorizonTerrain` comme ci-dessous, ou monter le banc (villager-png-001 :
  z = 30000).

## SUITE (même branche, second commit)

Signalé par villager-png-001 et vérifié : `capture-tree-lineup.py` (STAGE x = −14000,
z = 0) sortait sol noir et arbres à moitié enterrés sous l'anneau
(`Saved/SliceEvidence/lineup_ring_check.png`). Le script masque désormais
`HorizonTerrain` (rendu et ombre) après le chargement, pour la capture seulement :
`LINEUP HORIZON_HIDDEN`, `LINEUP::PASS`, planche lisible
(`lineup_ring_fixed.png`).
