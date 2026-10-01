# HANDOFF: forest-terrain-p0

> **Mise a jour a la fusion de main (WATER_LOOK_001).** Le correctif 1 ci-dessous (tache bleue) et
> son test `NoWaterPaintOnDryLand` sont RETIRES : WATER_LOOK_001, integre et prouve sur main, corrige
> le meme defaut (`Params.bWaterLook`, critere B > R + 0.04, repeinte depuis la terre seche la plus
> proche, test `Anastasis.Terrain.Drainage.WaterLook`). Sa version est gardee telle quelle. Reste de
> cette phase : l'exutoire sinueux (correctif 2).

## MISSION

Phase P0 de la mission foret/terrain : retirer les deux artefacts signales au sol --
la tache bleue dans le pre et le troncon de riviere droit -- sans toucher au village
ni au simulateur.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisDrainage.cpp (bloc « Couleurs et canaux
  du sol sur la terre reparee », seul)
- Source/Anastasis_UnrealV2/WorldView/AnastasisDrainageTests.cpp (nouveau test)
- Source/Anastasis_UnrealV2/WorldView/AnastasisHumanGeography.cpp (noeuds de `LakeOutlet`)
- docs/unreal/handoffs/forest-terrain-p0.md

## Diagnostic

1. **Tache bleue.** `AnastasisTerrainSurface::TileColor` peint toute tuile d'eau de la
   simulation en bleu (alpha 1). Human_Geography_V2 puis le drainage redessinent l'eau
   APRES cette peinture. Le drainage ne repeignait que les sommets qu'il assechait lui-meme
   (`Repair`, issus de `Wet` = sous la nappe d'origine). Une tuile d'eau que le relief
   corrige laisse hors d'eau sans qu'elle ait jamais ete sous la nappe gardait donc sa
   peinture : une plaque bleue en pleine terre seche.
2. **Riviere droite.** `LakeOutlet` : les noeuds (69,70) -> (74,70) -> (80,69) etaient
   alignes et au meme niveau d'eau (2.75). Human_Geography_V2 y creusait un canal droit
   de ~220 m sous le lac, que le drainage reprend comme riviere ecrite.

## Correctifs

1. Drainage : toute terre seche dont la peinture d'eau depasse 3 % est repeinte
   (couleur, UV0, UV1) par le meme remplissage harmonique que la terre reparee, et aucune
   peinture d'eau (seche ou noyee) ne sert plus de bord au remplissage. Le seuil de 3 %
   est justifie dans le code : toute teinte de sol a le bleu pour canal le plus faible, la
   plus grise (Stone) ne bascule qu'a ~4 % de `DeepWater`.
2. Human_Geography_V2 : un noeud intermediaire decale en alternance entre chaque paire de
   noeuds de `LakeOutlet` (1.5 -> 0.4 tuile), niveau d'eau moyen des voisins. Les noeuds
   d'origine, qui sont les points de controle de `HumanGeography.RiverAndOutlet`, sont
   conserves. Simulation hors Unreal de la spline : distance aux points de controle 0
   (0.001 tuile pour (95.5,76)), profil d'eau monotone, sinuosite 1.04 -> 1.13.

## COMMIT

BRANCH_HEAD (`claude/anastasis-forest-terrain-yhszr2`)

## MEC

- BUILD: **NOT_RUN**. Ecrit depuis un conteneur Linux sans Unreal : aucun build, aucun
  test n'a tourne. A faire sur le poste :
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter "Anastasis.Terrain"`
- Nouveau test : `Anastasis.Terrain.Drainage.NoWaterPaintOnDryLand` -- aucun sommet sec a
  dominante bleue apres drainage. Sa ligne `DRY_WATER_PAINT before_drainage=N` chiffre le
  defaut avant correction.
- Tests a surveiller : `Anastasis.Terrain.HumanGeography.RiverAndOutlet`,
  `Anastasis.Terrain.Drainage.*` (`KeepsAuthoredRivers`, `Network` : nouvelle geometrie
  de l'exutoire), `Anastasis.Terrain.Horizon.*` (la riviere sort au bord est par le meme
  point).

## SCN

NOT_ATTEMPTED. Capture conseillee : `tools\unreal\hydro-network-capture.ps1` (vue
zenithale et gros plans), avant/apres.

## PLY

UNKNOWN.

## INTEGRATION_RISK

- Non traite, volontairement :
  - **Anneau d'horizon** (`AnastasisTerrainHorizon.cpp`) : la riviere qui sort au bord est
    y est extrudee tout droit sur 0.8-1.8 km. Ses tests de pente (`Horizon.Gentle`) sont
    stricts ; une sinuosite la cisaille et doit etre validee par build + tests, pas a
    l'aveugle.
  - **Debug du village** : `anastasis.Village.Debug` vaut 1 par defaut et dessine en PIE
    des spheres bleues (habitants a l'interieur) et des cylindres cyan. Hors perimetre de
    cette mission ; autre candidat pour « l'objet bleu ».
- Le repeint touche aussi la frange seche des lacs gardes (halo bleute de rive) : voulu,
  c'est le meme defaut.
