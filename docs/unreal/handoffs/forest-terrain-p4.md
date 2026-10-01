# HANDOFF: forest-terrain-p4

## MISSION

Phase P4 de la mission foret/terrain : un sol qui ne soit plus plat, et une seule direction
artistique (realiste, mediterraneenne) du sol a la canopee. Sans toucher au village ni au simulateur.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainSurface.cpp (`SurfaceTypeColor` : Grass, Forest, Scrub)
- Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeography.cpp (couleur du fond de vallee)
- tools/unreal/ground-material.py (echelle regionale, terra rossa)
- tools/unreal/create-ground-cover.py (palette de MeadowTall et MeadowShort)

## Ce qui change

| Defaut | Cause | Correctif |
|---|---|---|
| Fond de vallee plat et trop clair | Human_Geography_V2 peignait tout le fond d'une constante (0.31, 0.40, 0.19), ~2.5x l'albedo de l'herbe calibree | prairie alluviale a l'albedo du sol, alternance pres / chaumes paille par parcelles de 100-200 m |
| Pas de variation au-dela de quelques dizaines de metres | « macro » de 60 m reglee pour un monde de 9 600 uu (la carte en fait 192 000) | bruit regional ~600 m dans `M_AnastasisGround` : pans de colline plus secs ou plus verts (`RegionTiling`, `RegionDryTint`, `RegionGreenTint`, `RegionContrast`) |
| Sol uniforme sous l'herbe et le maquis | aucune terre a nu | terra rossa (0.215, 0.125, 0.075) sur les pentes seches (18-41 deg rendus), par plaques, jamais sur la roche ni au bord de l'eau (`TerraRossa`, `TerraRossaAmount`) |
| Palette pontique humide | `SurfaceTypeColor` | prairie olive-paille (0.140, 0.150, 0.078), litiere de chene et de pin (0.080, 0.078, 0.050), garrigue rousse (0.170, 0.142, 0.088) ; meme niveau d'albedo, bleu toujours canal le plus faible |
| Herbe d'une autre saison que les arbres | prairie vert humide, 38 % d'ocre | prairie de debut d'ete : vert olive, 55 % d'ocre et de paille (haute), 42 % (rase) ; meme luminance |
| Herbe semi-realiste contre arbres low-poly | -- | resolu par P1 (arbres realistes) : une seule direction, realiste |

## COMMIT

BRANCH_HEAD (`claude/anastasis-forest-terrain-yhszr2`)

## MEC

- BUILD: **NOT_RUN** (conteneur Linux sans Unreal).
- Le materiau de sol ne se regenere que sur demande :
  `$env:ANASTASIS_GROUND_REBUILD='1'; tools\unreal\ground-material.ps1` (ecrase toute retouche de
  `MI_AnastasisGround`) ; l'herbe : `tools\unreal\create-ground-cover.ps1`.
- Tests a surveiller : `Anastasis.Terrain.Semantics` (terre jamais bleue), `HydrologyGradient` (crue
  plus sombre : vase 0.073 de luminance contre 0.143 pour la nouvelle prairie), `SlopeShade`,
  `Anastasis.Terrain.Drainage.WaterLook` (critere de repeinte B > R + 0.04 : toute la nouvelle palette
  de terre a B < R).

## SCN

NOT_ATTEMPTED. `capture-terrain-relief.ps1`, `ground-cover-capture.py` (prairie, vallee B),
`capture-horizon.ps1` (vue lointaine : pans regionaux).

## PLY

UNKNOWN.

## INTEGRATION_RISK

- Atmosphere non touchee : profil pontique humide (brume bleutee). Une lumiere plus seche et plus
  chaude serait la suite logique, mais `AnastasisAtmosphereTests` scelle ses valeurs : mission separee.
- La callune (H6b) reste : Erica manipuliflora en tient le role sur les montagnes grecques.
