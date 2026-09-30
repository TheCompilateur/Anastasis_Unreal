# GROUND_COVER_001 — la strate herbacée des espaces ouverts

Première passe d'herbe sur la carte. Jusqu'ici, entre les arbres il n'y avait rien : les tuiles
`Grass` recevaient une couleur, aucune plante. Cette passe pose trois familles d'herbe sur les
espaces ouverts de la vallée écrite (Human_Geography_V2), sur le sol et l'eau **réellement
rendus** (forge + drainage), après les arbres.

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

## Code

| Fichier | Rôle |
|---|---|
| `WorldView/AnastasisGroundCover.{h,cpp}` | règles pures et déterministes : `Build(FInputs, FSettings, FPlan&)`. Aucune dépendance UObject |
| `WorldView/AnastasisGroundCoverTests.cpp` | `Anastasis.GroundCover.*` |
| `WorldView/AnastasisWorldEmbodiment.{h,cpp}` | `PlaceGroundCover` après la forêt : sol `AnastasisTerrainForge::SampleActive`, eau `SampleActiveWater`, humidité `AnastasisDrainage::RiparianAt`, masque `AnastasisPlaces::ValleyWeightAt`, couronnes réellement posées, hameau en clairière piétinée |
| `tools/unreal/create-ground-cover.{ps1,py}` | source d'autorité des trois touffes et de `M_AnastasisGrass` |
| `tools/unreal/capture-ground-cover.ps1` + `ground-cover-capture.py` | A/B `anastasis.Dressing.GroundCover 1/0` |

CVar : `anastasis.Dressing.GroundCover` (1 par défaut ; 0 = sol nu, pour l'A/B). Journal :
`ANASTASIS_GROUND_COVER tall= short= sedge= placed= candidates= refused_*= plan_ms= total_ms=`.

Rendu : un HISM par famille, `GroundCover_<Famille>`, vidé puis rempli à chaque incarnation
(jamais détruit : cf. assertion `InstanceReorderTable`), sans collision ni navigation, ombres
portées actives, coupé à 112 m. La touffe suit 70 % de la pente et s'enfonce du reste.

`M_AnastasisGrass` : couleur de sommet, feuillage deux faces, normale monde × `TwoSidedSign`
(sans elle, la face arrière d'une lame rendait noire), vent en WPO (hauteur², vagues de position,
déphasé par instance), fondu de distance 70 → 105 m (la touffe s'enfonce vers son pivot).

Touffes : lames opaques en arc, sans texture, groupées en sous-touffes plus un remplissage
uniforme ; LOD écrits à la main (45 % puis 30 % des lames, élargies), pas réduits.

## Limites connues

- **Le sol sous l'herbe** (`MI_AnastasisGround`, autre chantier) est un vert-jaune pâle uniforme :
  entre deux touffes et au-delà du fondu, c'est lui qu'on voit. La lecture lointaine d'une prairie
  (EZ5, vue aérienne) relève du matériau de sol, pas de cette passe.
- **Vue oblique / aérienne** : l'herbe est coupée à 112 m ; de haut elle ne compte pas.
- **Pentes > 20°** : nues jusqu'à H6 (lande).
- **Hameau** : sur la carte réelle, le hameau composé est sur un replat sableux hors du masque
  de vallée : aucune herbe n'y pousse, la clairière piétinée (règle testée par
  `CanopyAndClearing`) n'y a donc aucun effet visible.
- **Coût** : frame p50 +3 à +14 ms selon la vue (lisière en enfilade : 10,7 → 24,7 ms, p95
  103 ms), mesuré sur une machine partagée — voir `docs/visual/ground-cover-001/README.md`.
  Leviers non essayés : ombres portées coupées sur l'herbe, LOD0 plus court, touffes LOD2
  en simple carte, distance de coupe par famille.
- **Crash Python à la fermeture** des éditeurs de capture (`python311.dll`, après
  `GROUND_CAPTURE_COMPLETE` et `QUIT_EDITOR`) : même signature dans les journaux de
  `places-capture.py` (world-dressing-claude-01, env-realism-001). Préexistant au modèle de
  script ; les images sont déjà écrites.
