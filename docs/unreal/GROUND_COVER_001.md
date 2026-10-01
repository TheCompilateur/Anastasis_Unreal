# GROUND_COVER_001 — la strate herbacée des espaces ouverts

Première passe d'herbe sur la carte. Jusqu'ici, entre les arbres il n'y avait rien : les tuiles
`Grass` recevaient une couleur, aucune plante. Cette passe pose trois familles d'herbe sur **toute
la carte**, sur le sol et l'eau **réellement rendus** (forge + drainage), après les arbres :
987 627 touffes, dont 690 560 hors de la vallée écrite, pour ~+1,5-2 ms de frame.

## Les familles, lues sur les planches

`docs/visual/reference/` — État Zéro 1, 2, 4, 5 :

| Famille | Planche | Aspect | Où |
|---|---|---|---|
| H1 `MeadowTall` — prairie haute | EZ5 « clairière naturelle », EZ1 « prairies sauvages » | 38-72 cm, vert sourd à pointes dorées, épis, 12 % de paille | pente 0-10°, à découvert |
| H2 `MeadowShort` — prairie basse | EZ5 « prairies basses », EZ2 « terre sèche, tassée » | 12-30 cm, plus verte, la terre perce | pente 10-20°, hameau piétiné, lisière ombragée |
| H3 `Sedge` — prairie humide | EZ5 « prairie humide », EZ4 « prairies saturées », EZ2 « végétation rivulaire » | laîches 45-80 cm de lame retombant à ~35-60 cm, vert bleuté | humidité de rive ≥ 0,4 (drainage) ou sol à < 45 cm au-dessus de la nappe |

| H6a `HeathTussock` — touffe d'éboulis | EZ1 « rochers, éboulis : affleurements » | graminée dure et serrée, 16-42 cm, 30 % de paille | versant 20-45°, partout |
| H6b `Heather` — callune | EZ1 « pente subalpine : landes » | petit buisson raide brun-vert, épis mauves de fin d'été | versant 20-45°, 8 → 28 m au-dessus du fond de vallée |

Pas encore faits, prévus : joncs de rive (H4, les roseaux existants tiennent l'eau), herbacées
de sous-bois (H5). Au-delà de 45° (falaise) et sous une couronne, rien n'est posé.

## Lande (H6)

- **Bandes** : prairie 0-20°, lande 20-45°, fondu par tirage entre 18 et 22° (pas de courbe de
  niveau). Densité 0,9 au pied → 0,3 à 45°, plancher de tache 35 %, ombre des couronnes à 50 %
  (prairie : 75 %), touffes ×1,3.
- **Habitat propre** (`LandeMask`) : roche, prairie, broussaille 1 ; forêt 0,7 ; eau de
  simulation et ruine 0,6 ; champ 0,5. La roche, refusée à la prairie (0,2), est chez elle ici.
- **Callune en haut** : hauteur au-dessus du fond de vallée habitable (bassin de la forge), pas de
  la nappe — hors rivières le drainage pose la nappe à 1 m sous le sol partout.
- **Mesuré, pas supposé** : `ANASTASIS_GROUND_SLOPES` journalise l'histogramme des pentes des
  candidates sèches et les hauteurs de lande posées (p10/p50/p90) ; les seuils ci-dessus en
  viennent. Preuves : `docs/visual/ground-cover-lande-001/`.
- Lande nue au-delà de 45° ; versant ouest de la vallée A encore nu (non diagnostiqué).

Ce que toutes les planches montrent, et que les règles portent :

- **jamais un tapis** : densité modulée par un bruit à deux octaves (taches d'~16 m), plancher
  à 40 % ; test `PatchesAndMask` (carrés de 10 m : p90 ≥ 1,5 × p10) ;
- **hauteurs mêlées** sur le plat : une seconde tache décide haute ou basse ;
- **fondu, pas de coupure** : la densité baisse avec la pente (12→20°) et à l'approche des
  couronnes (0,8 → 1,6 rayon), l'herbe haute y cède la place à la rase.

## Où l'herbe a le droit d'exister

Masque = max(poids de vallée Human_Geography_V2, ouverture de la tuile de simulation interpolée
entre centres de tuiles). Ouverture (prairie) : prairie / champ 1 ; broussaille, forêt 0,8 (les couronnes
posées excluent le sous-bois, une trouée de forêt est une clairière) ; eau de simulation 0,8
(le drainage en a rendu l'essentiel à la terre, la vraie nappe est refusée par la règle d'eau) ;
ruine 0,4 ; roche 0,2 (lande = H6). Les v1-v3 ne lisaient que la vallée : hors vallées, rien.

## Code

| Fichier | Rôle |
|---|---|
| `WorldView/AnastasisGroundCover.{h,cpp}` | règles pures et déterministes : `Build(FInputs, FSettings, FPlan&)`, parallèle par rangées de blocs, fusion dans l'ordre. Aucune dépendance UObject |
| `WorldView/AnastasisGroundCoverTests.cpp` | `Anastasis.GroundCover.*` |
| `WorldView/AnastasisWorldEmbodiment.{h,cpp}` | `PlaceGroundCover` après la forêt : sol `AnastasisTerrainForge::SampleActive`, eau `SampleActiveWater`, humidité `AnastasisDrainage::RiparianAt`, masque ci-dessus, couronnes réellement posées, hameau en clairière piétinée ; `GetFrameTimingsMs` (métrologie) |
| `tools/unreal/create-ground-cover.{ps1,py}` | source d'autorité des trois touffes et de `M_AnastasisGrass` |
| `tools/unreal/capture-ground-cover.ps1` + `ground-cover-capture.py` | A/B `-States on,off,on2,noshadow`, frame et temps GPU par vue |

CVars (appliquées à l'incarnation) : `anastasis.Dressing.GroundCover` (1 ; 0 = sol nu, pour
l'A/B), `anastasis.GroundCover.Shadows` (1 ; ombres des touffes proches),
`anastasis.GroundCover.InAutomation` (0 ; pas d'herbe pendant les tests d'automatisation : la
suite incarne le monde ~16 fois, et 16 × 1 M de touffes non rendues entre deux tests ont tué la
suite par manque de mémoire le 2026-10-01). Journal :
`ANASTASIS_GROUND_COVER tall= short= sedge= placed= near= far= chunks= outside_valley= ... plan_ms= total_ms=`.

## Rendu et coût

- **Tuiles de 160 m.** Un HISM par (famille, tier, tuile) — 683 sur la carte de référence —
  avec une distance d'affichage de primitive (coupe + demi-diagonale) : le moteur écarte d'un
  bloc les tuiles loin de l'œil. En v4, six HISM couvrant toute la carte coûtaient +12 ms dans
  chaque vue, même vide : le coût suivait le nombre total d'instances, pas ce qui est visible.
- **Éclaircie de distance.** Deux tiers par famille : proche (66 %, ombres portées, fondu
  40 → 55 m) et lointain (34 %, ×1,25, sans ombres, fondu 70 → 105 m). Bornes passées au
  matériau par MID (`FadeStart` / `FadeEnd`).
- Vidé puis rempli à chaque incarnation, jamais détruit (assertion `InstanceReorderTable`),
  sans collision ni navigation. La touffe suit 70 % de la pente et s'enfonce du reste.
- Mesures : `docs/visual/ground-cover-001/README.md`. Ombres coupées = aucun gain mesurable.

`M_AnastasisGrass` : couleur de sommet, feuillage deux faces, normale monde × `TwoSidedSign`
(sans elle, la face arrière d'une lame rendait noire), vent en WPO (hauteur², vagues de position,
déphasé par instance), fondu de distance paramétré (la touffe s'enfonce vers son pivot).

Touffes : lames opaques en arc, sans texture, groupées en sous-touffes plus un remplissage
uniforme ; LOD écrits à la main (45 % puis 30 % des lames, élargies), pas réduits. Le générateur
ouvre l'éditeur sur `/Engine/Maps/Entry` : sur la carte de démarrage, les touffes posées
verrouillaient les assets à régénérer.

## Sol sous l'herbe

Le matériau de sol (`M_AnastasisGround`, `tools/unreal/ground-material.py`) multiplie toutes ses
couches par la couleur de sommet. Après la pose de l'herbe, `PlaceGroundCover` construit un champ de
couverture (`BuildCoverField` : part des candidates réellement posées par cellule de 4 m, par groupe
prairie / laîches / lande, deux flous 3×3) et teinte la couleur de sommet de la section de sol
(`TintSoil`) : sous la prairie elle fonce et verdit, sous les laîches elle fonce davantage, sous la
lande elle passe en terre brune. Facteurs relatifs (la tuile garde sa sémantique : un sable reste
plus clair qu'une herbe) tirés à moitié vers un absolu (un sable sous prairie verdit).

C'est un champ basse fréquence : il va dans la couleur de sommet, comme le veut
`GROUND_HYDROLOGY_ARBITRATION.md` ; le matériau garde le détail et n'est pas modifié. Le canal alpha
(drapeau « sous l'eau » du drainage) est intact. CVar : `anastasis.GroundCover.SoilTint` (1 ; 0 pour
l'A/B, mêmes touffes). Journal : `ANASTASIS_SOIL_TINT tinted_vertices= mean_amount=`. Preuves :
`docs/visual/ground-soil-tint-001/`.

## Limites connues

- **Le sol sous l'herbe** est teinté par la couverture (voir ci-dessus) ; hors herbe, il garde la
  teinte de tuile du matériau de sol (sable, roche).
- **Vue oblique / aérienne** : l'herbe est coupée à 108 m ; de haut elle ne compte pas.
- **Pentes > 45°** : nues (falaise). Entre 20 et 45° : lande clairsemée, voir Lande (H6).
- **Hameau** : sur un replat sableux où l'herbe est rare ; la clairière piétinée (règle testée
  par `CanopyAndClearing`) n'y a pas d'effet visible.
- **Incarnation** : +1,4 à 2 s (plan 0,35-0,55 s en parallèle, le reste pour remplir les HISM) ;
  ~1 M d'instances en mémoire dans tout éditeur ouvert sur `Lvl_AnastasisSlice`.
- **Temps render thread** non mesuré (`GRenderThreadTime` vaut 0 dans l'éditeur).
- **Crash Python à la fermeture** des éditeurs de capture (`python311.dll`, après
  `GROUND_CAPTURE_COMPLETE` et `QUIT_EDITOR`) : même signature dans les journaux de
  `places-capture.py`. Préexistant au modèle de script ; les images sont déjà écrites.
