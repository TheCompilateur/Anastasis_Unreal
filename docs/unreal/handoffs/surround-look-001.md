# HANDOFF: surround-look-001

## MISSION

Premier tour du regard autour de la carte, au coût le plus bas. La perspective
aérienne du ciel commence à 0,2 km : le hameau reste net, les crêtes prennent
la brume, et les pixels plus proches sont sautés par l'early depth test.

Brume de thalweg, ombres de nuages, vent de l'herbe et exposition locale
étaient déjà en place. Cette mission ne les réécrit pas.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereProfile.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisAtmosphereTests.cpp
- docs/unreal/handoffs/surround-look-001.md

## COMMIT

Ce commit, branche `agent/surround-look-001`.

## MEC

- BUILD: PASS (`anastasis-unreal.ps1 build`, worktree surround-look-001, 210.88 s)
- TESTS: le filtre `Anastasis.Atmosphere.Realism.Reversible` a été arrêté sur demande d'Alexandre
  pendant `EDITOR_GATE::WAIT` (deux éditeurs, RAM libre sous 3 Go). Il n'a pas lancé l'éditeur.
  La suite `Anastasis` est la preuve du portail `finish`.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build` → BUILD::PASS

## SCN

La couche de réalisme écrit `SkyAerialPerspectiveStartDepthKm` (0,2) sur le
Sky Atmosphere, et rend la valeur moteur (0,1 km) quand le réalisme est coupé.
`Anastasis.Atmosphere.Realism.Reversible` affirme les deux.

## PLY

UNKNOWN. PLAYER reste NOT_IMPLEMENTED. Aucune capture.

## INTEGRATION_RISK

- `AnastasisWorldAtmosphere.cpp` et le profil sont des fichiers chauds (ciel, météo).
- L'asset `DA_AnastasisAtmosphere` n'a pas le champ : Unreal prend le défaut C++ 0,2
  tant que l'asset ne le sérialise pas.
- Preuve visuelle absente. Le seuil 0,2 km n'a pas été jugé sur une image.

## STOP

- Pas de nouveaux volumes de brume, pas de résolution d'ombre de nuage plus haute,
  pas de reconstruction des matériaux d'arbres.
- Pas d'intégration, pas de push.
