# HANDOFF: horizon-ring-001

## MISSION

Supprimer le « vide noir » autour de la carte : étape 1, anneau de terrain lointain.

Cause mesurée : le maillage forgé s'arrête net au bord des 96 tuiles (1,9 km à
l'échelle 5). Au-delà, rien — sous l'horizon la caméra voit le sol de planète par
défaut du `SkyAtmosphere`, bleu nuit à noir. Vue générale : 59 % de l'image ; au bord
de la carte, regard dehors : la moitié basse de l'image est noire.

Correction : `AnastasisTerrainHorizon`, un anneau de présentation autour du monde
forgé entier, jusqu'à 20 km, plus une jupe à 160 km.

- **Raccord exact** : l'anneau 0 est la copie du bord forgé (positions, normales,
  nappe d'eau, canaux de rive). Test `Seam` : écarts 0.
- **Profil** : le bord se fond sur 600 m dans des collines (7–60 m, l'ordre du relief
  de la carte), puis une chaîne lointaine monte de 2,2 à 7,6 km (100–300 m). Test
  `Closed` : depuis le point le plus haut de la carte, l'horizon est fermé dans toutes
  les directions (pire +0,80°), mesuré sur la surface, pas sur les sommets.
- **Rivière sortante** : la rivière Human_Geography_V2 qui sort par le bord (152
  sommets de bord sous leur nappe) continue dans l'anneau à son propre niveau d'eau,
  puis son lit remonte entre 0,8 et 1,8 km : elle s'achève en lac, pas en mer infinie.
- **Pas de rayures** : première version = aiguilles de 15 m × 300 m dès 2 km, rendues
  en rayures par l'ombrage à facettes (capture H4 intermédiaire). Paliers de détail
  (colonnes ÷2, jusqu'à ÷16) et lissage tangentiel du profil, des couleurs et des UV du
  bord. Test `NoSlivers` : allongement p99 6,87. Maillage : 20 805 sommets, 39 995
  triangles (contre 72 960 / 142 880 sans paliers).
- **Pente** : test `Gentle`. 0 triangle au-delà de 45° hors du raccord ; près du
  raccord, 64 triangles au-dessus de 45°, tous dans le prolongement de la berge de la
  rivière sortante, que le bord forgé porte déjà à 50,1°. Aucun n'est plus raide que
  la forge au bord (50,1° contre 50,2°). Une surface qui contient un segment de bord à
  48° a au moins 48° : l'anneau ne peut pas faire mieux sans toucher la forge.
- Aucune collision, aucune navigation, même matériau de sol et d'eau que la carte.
  N'existe que si la surface incarnée est le monde entier forgé.
- `anastasis.Terrain.Horizon` (1 par défaut, 0 = avant) : A/B dans un seul build.

## FILES_OWNED

- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainHorizon.h/.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainHorizonTests.cpp` (nouveau)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.h/.cpp` : composant
  `HorizonTerrain`, CVar, construction après la forge
- `tools/unreal/capture-horizon.ps1` + `capture-horizon.py` (nouveau), index `AGENTS.md`
- `docs/visual/horizon-ring-001/` : H3, H4, H5 en A (sans anneau) / B (avec)

## COMMIT

PENDING

## MEC

- BUILD: PASS (worktree, `anastasis-unreal.ps1 build`)
- TESTS: `Anastasis.Terrain.Horizon.Seam`, `.Closed`, `.Gentle`, `.NoSlivers` PASS ;
  suite complète : voir `finish`
- COMMANDS:
  - `Automation RunTests Anastasis.Terrain.Horizon`
  - `tools\unreal\capture-horizon.ps1 -Label ring`

Part de pixels « vide » (bleu nuit ou quasi-noir), A → B :

| Vue | A | B |
|---|---|---|
| H1 30 m au-dessus du bassin | 0,2 % | 0,2 % |
| H2 15 m au-dessus du point haut | 14,5 % | 13,6 % |
| H3 au bord, regard dehors | 48,0 % | 0,0 % |
| H4 vue générale | 59,0 % | 1,9 % |
| H5 150 m au-dessus du bassin | 10,1 % | 1,8 % |

H1 : la vue est fermée par le relief et les arbres dans les deux états. H2 : le chiffre
est dominé par les ombres de sous-bois, que le seuil quasi-noir compte aussi ; la
bande de vide visible en A a disparu en B. Deux runs complets donnent les mêmes
valeurs au dixième près.

## SCN

`Lvl_AnastasisSlice`, graine 12345, `EmbodyCanonical`, échelle 5, Human_Geography_V2.

## PLY

NOT_IMPLEMENTED — l'anneau n'a pas de collision ; rien n'empêche aujourd'hui un pion de
sortir de la carte, comme avant.

## INTEGRATION_RISK

- `AnastasisWorldEmbodiment.cpp` est chaud (forge, lieux, dressing) : ajout localisé
  après `ExperimentalSurface->SetVisibility(true)`.
- Les bornes de l'ACTEUR incluent désormais l'anneau (160 km). Les outils qui cadrent
  sur `get_actor_bounds` de l'incarnation (aucun dans `tools/unreal/` à ce jour ;
  `AnastasisWorldProbeSubsystem` les rapporte seulement) doivent cadrer sur le
  composant `ExperimentalTerrain`, comme `capture-horizon.py`.
- Plantage connu à la SORTIE de l'éditeur de capture (`EXCEPTION_ACCESS_VIOLATION` dans
  python311 après `*_COMPLETE`, images déjà écrites) : présent dès le 2026-09-29 sur
  `main`, avant cette mission, avec ou sans désenregistrement du rappel. Non attribué.

## STOP

- Étape 2 non faite : la brume (`AnastasisAtmosphereProfile`, départ 15 m, densité
  0,012) est réglée pour l'ancien monde de 96 m. Sans elle, la limite entre la carte
  et l'anneau (teinte moyenne uniforme) se voit d'en haut, et la chaîne lointaine est
  nette au lieu de bleuir. Domaine atmosphère, mission séparée.
- Pas d'arbres ni de lieux sur l'anneau.
- Le rempart de bord (`ShapeMountainProfile` du simulateur) n'est pas touché : l'anneau
  le prolonge.
