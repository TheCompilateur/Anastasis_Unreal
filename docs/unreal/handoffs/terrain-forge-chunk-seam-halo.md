# HANDOFF: terrain-forge-chunk-seam-halo

## MISSION

Corriger les pics d'escarpement degenerés observés au bord des emprises (chunks)
de terrain, sur les pentes raides : `AnastasisTerrainForge::Apply` calculait
pente/Laplacien au bord fin de `Crop` en clampant le voisin manquant sur le
sommet lui-meme, lisant une convexite fictive que l'escarpement amplifiait
sans borne (`L * 8.0`).

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForge.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisTerrainForgeTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (construction
  du HaloCrop, uniquement dans la branche SurfaceMode==2 -- le Mode 1 scelle
  WORLD_SLICE_006 n'est pas touche)

## COMMIT

BRANCH_HEAD (d598c55 sur `terrain-forge-chunk-seam-halo`)

## MEC

- BUILD: PASS (`Anastasis_UnrealV2Editor Win64 Development`)
- TESTS: PASS 4/4, filtre `Anastasis.Terrain.Forge` (Contract, SampleHeight,
  CarriesMorphology, ChunkSeam -- nouveau)
- PREUVE CHIFFREE (`Anastasis.Terrain.Forge.ChunkSeam`) : sur une emprise
  20x20 loin des bords du monde, le Laplacien au bord fin colle exactement a
  celui du monde entier avec le halo (`lap_error_halo=0.000000000`) contre
  `lap_error_no_halo=0.003500000` sans lui.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1 -Filter "Anastasis.Terrain.Forge"`

## SCN

NOT_ATTEMPTED. Aucune capture visuelle : le defaut et la preuve du fix sont
numeriques (comparaison du Laplacien a la verite du monde entier), pas une
difference visible a l'oeil sur une seule capture statique.

## PLY

UNKNOWN. Jamais verifie depuis une camera joueur a un bord de chunk reel en
PIE.

## INTEGRATION_RISK

- **`AnastasisWorldEmbodiment.cpp` est un fichier tres chaud.** Plusieurs
  branches vivantes le touchent (terrain-forge-001, hydrology-surface,
  ecotone-forge-001, atmosphere-mist-002, world-dressing-v0,
  village-interaction-001, sim-tick-day, entre autres observees au moment de
  cette integration). Cette mission n'y ajoute que la construction du
  `HaloCrop` autour de l'appel `AnastasisTerrainForge::Apply` existant --
  verifier apres toute fusion supplementaire que cet appel garde sa forme a 4
  arguments.
- **L'ampleur mesuree (0.0035) est celle d'UNE seule emprise/seed (24,24 20x20,
  seed 12345).** L'artefact clampe-sur-soi est proportionnel a la vraie pente
  du terrain juste au-dela du bord ; une falaise franche coupee par un bord de
  chunk pourrait produire une erreur bien plus grande. Non teste sur plusieurs
  seeds/emprises.
- **Le Mode 1 (WORLD_SLICE_006) est protege par un simple `if (SurfaceMode ==
  2)`**, verifie par lecture de code, pas par un test dedie qui detecterait
  une suppression accidentelle de cette garde.
- **Trouvaille separee, non corrigee ici** : le bassin habitable, le point
  haut et l'accumulation de drainage (D8) cherchent leur candidat sur TOUTE
  l'emprise de `Crop`, pas a position absolue -- deux emprises differentes du
  meme point du monde peuvent y placer un bassin/sommet completement
  different (ecart mesure ~350uu, cent fois le pic corrige ici). Signale en
  tache de fond separee (voir suivi hors-branche), pas dans le perimetre de
  cette mission.

## STOP

Cette mission ne revendique PAS :

- la stabilite du bassin/point-haut/drainage entre deux emprises (trouvaille
  separee, non corrigee)
- une preuve visuelle ou joueur
- une validation multi-seed de l'ampleur de l'artefact corrige
- tout changement au Mode 1 / WORLD_SLICE_006
