# HANDOFF: cosmic-night-v1-001

## MISSION

Faire passer le ciel cosmique V0 à une première composition V1 lisible et réversible :
panorama à coeur galactique, profondeur stellaire, lune mise en scène, météore perceptible
et voile rare distinct. Aucun .umap touché.

## FILES_OWNED

- ArtSource/Celestial/cosmic_river_v1.png, cosmic_sky_v1.hlsl
- Content/Anastasis/Celestial/T_CosmicRiverV1.uasset, M_AnastasisCosmicSkyV1.uasset
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldAtmosphere.h/.cpp, AnastasisCosmicNight.cpp
- tools/unreal/cosmic-sky-material.ps1/.py, cosmic-night-pie.py
- docs/unreal/COSMIC_NIGHT_V1_001.md, cette fiche, AGENTS.md

## COMMIT

Le HEAD marqué par agent-worktree.ps1 finish.

## MEC

- BUILD: PASS — anastasis-unreal.ps1 build, 2026-10-08, Result: Succeeded, 21 actions, 177,98 s.
- MATERIAL: PASS technique — cosmic-sky-material.ps1 -Version 1 -Rebuild, 2026-10-08 ; T_CosmicRiverV1 et M_AnastasisCosmicSkyV1 sauvés, COSMIC_MATERIAL_DONE, sortie 0, log Unreal zéro erreur. Apparence SCN inconnue.
- TESTS: QUEUED — cosmic-night-pie au lot.
- COMMANDS: tools/unreal/anastasis-unreal.ps1 build ; tools/unreal/cosmic-sky-material.ps1 -Version 1 -Rebuild

## PROOFS

PROOFS: cosmic-night-pie

## SCN

UNKNOWN — Alexandre a dispensé les captures automatiques. Image source inspectée, rendu
Unreal à hauteur de joueur non jugé.

## PLY

UNKNOWN — aucune session de joueur libre n'a évalué l'attente et la surprise.

## INTEGRATION_RISK

- AnastasisWorldAtmosphere.cpp est chaud ; rebase et finish sur main le plus récent.
- La nouvelle texture et le nouveau matériau doivent être créés avant finish.
- L'option V1 est active par défaut, mais la V0 demeure sélectionnable par CVar.
- La preuve PIE lit le parent du matériau dynamique pour éviter un faux PASS de paramètres.

## STOP

Ne pas appeler beauté validée un shader compilé ou un test de paramètres.