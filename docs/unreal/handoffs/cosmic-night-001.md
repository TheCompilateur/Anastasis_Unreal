# HANDOFF: cosmic-night-001

## MISSION

Introduire un ciel nocturne galactique et une lune violette dans le système
d'atmosphère actuel, avec météores récurrents et voile rare déterminés par le
calendrier du monde, sans éditer de `.umap` afin de pouvoir reprendre une autre
version de carte.

## FILES_OWNED

- `ArtSource/Celestial/moon_lavender.png`, `cosmic_river.png`
- `Content/Anastasis/Celestial/T_MoonLavender.uasset`, `T_CosmicRiver.uasset`, `M_AnastasisCosmicSky.uasset` (à créer par Unreal)
- `Source/Anastasis_UnrealV2/WorldView/AnastasisCosmicNight.h/.cpp`, `AnastasisCosmicNightTests.cpp`, `AnastasisWorldAtmosphere.h/.cpp`
- `tools/unreal/cosmic-sky-material.ps1/.py`, `capture-sky.ps1/.py`, `cosmic-night-pie.py`, `proofs.txt`
- `docs/unreal/COSMIC_NIGHT_001.md`, `AGENTS.md`, cette fiche

## COMMIT

PENDING

## MEC

- BUILD: UNKNOWN
- TESTS: UNKNOWN
- MATERIAL: UNKNOWN
- COMMANDS: `tools/unreal/anastasis-unreal.ps1 build`, `tools/unreal/cosmic-sky-material.ps1`

## PROOFS

PROOFS: cosmic-night-pie, sky-clock-pie

## SCN

UNKNOWN — captures dispensées par Alexandre le 2026-10-07. La preuve PIE est
instrumentale et ne tranche pas la qualité de l'image.

## PLY

UNKNOWN — aucun joueur libre n'a encore contemplé la nuit dans cette mission.

## INTEGRATION_RISK

- `AnastasisWorldAtmosphere.h/.cpp` sont des fichiers chauds : rebaser sur le `main` le plus récent après le lot en cours.
- `nox-001` travaille en parallèle sur la lumière lunaire dans ce même `.cpp`. Le dôme suit déjà son contrôle optionnel `MoonFraction` quand le profil NOX est actif ; vérifier l'empilement une fois NOX versé. Sans NOX, la lune reste pleine.
- La création des trois `.uasset` et leur compilation de matériau sont requises ; la preuve visuelle est reportée sur demande d'Alexandre.
- `sky-clock-pie` vérifie la traversée du cycle et ses phases ; il ne juge ni la beauté ni la lisibilité des météores.

## STOP

Ne pas présenter un matériau compilé ou une capture fixe comme une preuve de l'expérience du joueur, ni la compatibilité avec toute future carte comme démontrée par une seule autre carte.
