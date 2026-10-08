# HANDOFF: cosmic-comet-001

## MISSION

Ajouter à la V1 du ciel une comète reconnaissable qui dure quatre soirées et change
de position par rapport à la rivière d'étoiles. Calendrier déterministe, occultation
par jour et météo, aucune modification de la simulation ni de la carte.

## FILES_OWNED

- ArtSource/Celestial/cosmic_sky_v1.hlsl
- Content/Anastasis/Celestial/M_AnastasisCosmicSkyV1.uasset
- Source/Anastasis_UnrealV2/WorldView/AnastasisCosmicNight.h/.cpp, AnastasisCosmicNightTests.cpp, AnastasisWorldAtmosphere.cpp
- tools/unreal/cosmic-sky-material.py, cosmic-night-pie.py
- docs/unreal/COSMIC_NIGHT_V1_001.md, cette fiche, AGENTS.md

## COMMIT

Le HEAD marqué par agent-worktree.ps1 finish.

## MEC

- BUILD: PASS — anastasis-unreal.ps1 build dans cosmic-comet-001, 2026-10-08, 22 actions, 185,53 s, Result: Succeeded.
- MATERIAL: PASS technique — cosmic-sky-material.ps1 -Version 1 -Rebuild dans le worktree V1 déjà compilé ; M_AnastasisCosmicSkyV1 sauvegardé, COSMIC_MATERIAL_DONE, sortie 0. Copie identique SHA-256 dans cosmic-comet-001 ; la texture V1 et le matériau V0 ne changent pas.
- SUITE: PASS — finish -Mission cosmic-comet-001, sans rendu, 288 s : 374 PASS, 4 KNOWN_EXPECTED_FAILURE, 0 FAIL sur 378 cas. Anastasis.Sky.Cosmic.Comet : Result={Success}.
- PIE LOCAL: PASS — editor-batch.ps1 -Proofs cosmic-night-pie dans cosmic-comet-001, 2026-10-08, 78,5 s. Parent MID = M_AnastasisCosmicSkyV1 ; MeteorStrength = 0,215 ; CometStrength = 0,55 à 23 h clair, puis 0 sous couverture totale. La preuve devra être rejouée au lot.

## PROOFS

PROOFS: cosmic-night-pie

## SCN

UNKNOWN — aucune inspection du rendu à hauteur de joueur ; les captures automatiques
ont été dispensées par Alexandre.

## PLY

UNKNOWN — la curiosité du joueur sur plusieurs nuits n'a pas été observée.

## INTEGRATION_RISK

- La V1 doit être dans main avant cette mission ; branche créée sur ad18bb99 qui la contient.
- Régénérer le seul M_AnastasisCosmicSkyV1, sans toucher au matériau V0.
- L'effet utilise les mêmes coordonnées tournantes que la rivière ; vérifier son coût GPU
  et sa silhouette avant toute revendication artistique.

## STOP

Un calendrier et un shader compilés ne prouvent pas l'émerveillement ni la lisibilité en jeu.
