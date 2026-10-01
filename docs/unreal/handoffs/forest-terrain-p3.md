# HANDOFF: forest-terrain-p3

## MISSION

Phase P3 de la mission foret/terrain : les strates basses. Maquis (lentisque, chene kermes,
genet), ronces en lisiere et sur les berges, rochers disperses selon la pente, herbes hautes en
lisiere, et fin du rang d'arbustes le long des bords de tuile de la vieille foret. Sans toucher au
village ni au simulateur.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisUnderstory.h / .cpp / Tests.cpp (nouveau module)
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h / .cpp (`PlaceUnderstory`, CVars,
  `TreeUnit` corrige)
- Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.cpp (ourlet d'herbes hautes)
- Source/Anastasis_UnrealV2/WorldView/AnastasisPlaces.cpp (arbustes de lisiere de la vieille foret)
- Source/Anastasis_UnrealV2/WorldView/AnastasisEcologicalDressing.cpp (graines des bruits P2)
- Source/Anastasis_UnrealV2/WorldView/AnastasisPresentationResolver.cpp (graine du tirage d'essence P1)
- tools/unreal/create_tree_asset.py (12 arbustes, `M_AnastasisRock`), AGENTS.md (index)

## Ce qui change

| Strate | Regle (aptitudes douces, une candidate par cellule de 4 m) |
|---|---|
| Maquis | bas et moyen versant (rarefie au-dessus de 55-80 % du relief), sec, pentes 0-38 deg, par plaques (bruit 3 tuiles), x2.5 sur les tuiles Scrub du sim, x0.2 sous les couronnes, x1.8 en lisiere, 15 % dans les fonds de vallee pastures |
| Essence | lentisque au bas et au sec ; kermes sur la pente et la roche ; genet dans l'ouvert et en lisiere |
| Ronces | anneau de lisiere (0.8-3 rayons de couronne) et bande riveraine rendue ; jamais au-dessus de 30 deg |
| Rochers | epars sur le pre ; nombreux avec la pente (14-36 deg), la roche du sim et l'altitude ; plus gros avec la pente (0.35-1 m au plat, jusqu'a 2.6 m), famille selon la pente ; enfouis de 15 a 40 % ; collision |
| Ourlet | l'herbe haute monte a mi-ombre au bord des couronnes (et des buissons) au lieu de ceder la place a la rase |
| Vieille foret | arbustes de lisiere a profondeur et position tirees, un tiers des bords vides : plus de rang |

Reserves pour tout : champs, ruines, eau du sim et eau rendue, bassin du village, lit des rivieres
ecrites, route du col. L'herbe s'ecarte des buissons et ne traverse pas les rochers (emprises
ajoutees aux couronnes). Teinte par instance des arbustes comme les arbres (secheresse, ecart).

**Correction de hachage (P1, P2, P3).** Le sel n'entrait qu'au dernier pas : deux tirages d'un
meme site ne differaient que d'une constante. Effets trouves sur la replique : aucun genet
(l'essence suivait le tirage de presence) ; hauteur, couronne et teinte d'un arbre correlees ;
essence d'arbre correlee a la densite locale (meme fonction et meme graine que le tirage de
presence ecologique) ; bruit de clairiere correle au bruit de masse. Corrige par finaliseur
(`TreeUnit`, `AnastasisUnderstory::Hash`) ou graine perturbee avant melange (essence, bruits P2).
Les tirages pre-existants ne sont pas touches.

**Rendu.** Un HISM par mesh, coupe par instance : maquis 300 m, ronces 150 m, rochers 400 m.
`M_AnastasisRock` : calcaire gris chaud, lichens, fissures, grain en normale, procedural en espace
monde (aucune texture) ; repli sur l'aplat de pierre des lieux composes s'il manque.

## COMMIT

BRANCH_HEAD (`claude/anastasis-forest-terrain-yhszr2`)

## MEC

- BUILD: **NOT_RUN** (conteneur Linux sans Unreal). Sur le poste :
  1. `tools\unreal\anastasis-unreal.ps1 build`
  2. editeur : `py tools/unreal/create_tree_asset.py` -> `RESULT::PASS ... shrub_meshes=12` et `ROCK_MATERIAL saved`
  3. `tools\unreal\report-tests.ps1 -Filter "Anastasis"`
- Nouveaux tests : `Anastasis.Understory.SlopeAltitudeAndReserves`, `Anastasis.Understory.EdgesRiversAndSpecies`.
  Seuils tires d'une replique Python du plan, avec marge : rochers 33 au plat / 820 a 30 deg (> x5),
  taille 0.65 -> 1.15 m a 40 deg (> x1.3), maquis 702 -> 125 au sommet (< 0.4), ronces 431 en lisiere
  et 0 au loin, 658 sur la berge et 0 au sec, lentisque 505 > kermes+genet 197 au bas, kermes 1070 >
  lentisque 92 sur la roche en pente ; 0 sur champ, ruine, eau, bassin.
- Log : `ANASTASIS_UNDERSTORY lentisk= kermes_oak= broom= bramble= rock= ... missing_meshes= plan_ms=`.
- CVars : `anastasis.Dressing.Understory 0/1` (A/B), `anastasis.Understory.InAutomation` (0 : coupe
  sous automatisation, comme l'herbe).

## SCN

NOT_ATTEMPTED. Captures conseillees : `ground-cover-capture.py` (lisiere, prairie, vallee B) et
`capture-forest-walk.py`, `anastasis.Dressing.Understory 0` puis `1`.

## PLY

UNKNOWN (collision des rochers a eprouver en marchant).

## INTEGRATION_RISK

- Nombre d'instances inconnu sur la carte reelle (lire `ANASTASIS_UNDERSTORY`, garde-fou 400 000).
- La teinte des rochers depend des meshes `SM_Rock_*` : leur materiau d'import est WorldGrid,
  remplace sur tous les slots par `M_AnastasisRock`.
- La correction de graine du tirage d'essence (P1) change la repartition par rapport au commit P1.
