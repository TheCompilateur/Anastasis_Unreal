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

Pas encore faits, prévus : joncs de rive (H4, les roseaux existants tiennent l'eau), herbacées
de sous-bois (H5), lande et touffes d'éboulis (H6). Au-delà de 20° et sous une couronne, cette
passe ne pose donc rien.

Ce que toutes les planches montrent, et que les règles portent :

- **jamais un tapis** : densité modulée par un bruit à deux octaves (taches d'~16 m), plancher
  à 40 % ; test `PatchesAndMask` (carrés de 10 m : p90 ≥ 1,5 × p10) ;
- **hauteurs mêlées** sur le plat : une seconde tache décide haute ou basse ;
- **fondu, pas de coupure** : la densité baisse avec la pente (12→20°) et à l'approche des
  couronnes (0,8 → 1,6 rayon), l'herbe haute y cède la place à la rase.

## Où l'herbe a le droit d'exister

Masque = max(poids de vallée Human_Geography_V2, ouverture de la tuile de simulation interpolée
entre centres de tuiles). Ouverture : prairie / champ 1 ; broussaille, forêt 0,8 (les couronnes
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
l'A/B), `anastasis.GroundCover.Shadows` (1 ; ombres des touffes proches). Journal :
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

## Limites connues

- **Le sol sous l'herbe** (`MI_AnastasisGround`, autre chantier) perce entre les touffes : vert
  pâle en vallée, sableux ailleurs. La lecture lointaine d'une prairie relève du matériau de sol.
- **Vue oblique / aérienne** : l'herbe est coupée à 108 m ; de haut elle ne compte pas.
- **Pentes > 20°** : nues jusqu'à H6 (lande).
- **Hameau** : sur un replat sableux où l'herbe est rare ; la clairière piétinée (règle testée
  par `CanopyAndClearing`) n'y a pas d'effet visible.
- **Incarnation** : +1,4 à 2 s (plan 0,35-0,55 s en parallèle, le reste pour remplir les HISM) ;
  ~1 M d'instances en mémoire dans tout éditeur ouvert sur `Lvl_AnastasisSlice`.
- **Temps render thread** non mesuré (`GRenderThreadTime` vaut 0 dans l'éditeur).
- **Crash Python à la fermeture** des éditeurs de capture (`python311.dll`, après
  `GROUND_CAPTURE_COMPLETE` et `QUIT_EDITOR`) : même signature dans les journaux de
  `places-capture.py`. Préexistant au modèle de script ; les images sont déjà écrites.
