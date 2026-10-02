# HANDOFF: geography-concordance-001

## MISSION

Premier chantier de carte scientifique : rendre les desaccords entre eau visible et
Type==Water mesurables avant tout changement du territoire ou des comportements.
Correction de la couverture Map Intelligence (nappe + rubans) et instrumentation
non decisionnelle du releve d'implantation. Base : 4fef72d526ee0be45a923c46c9e626dc3a6e7deb.
Branche agent/geography-concordance-001 ; worktree C:/dev/ANASTASIS_WORKTREES/geography-concordance-001.
Owner : cette mission. Racine canonique et autres worktrees exclus.
Fichiers locaux canoniques exclus : download.png, .claude/settings.local.json.

## FILES_OWNED

- Content/Python/anastasis_map_intelligence/core.py
- Content/Python/anastasis_map_intelligence/editor.py
- Content/Python/anastasis_map_intelligence/concordance.py
- Content/Python/anastasis_map_intelligence/tests/test_adapter.py
- Content/Python/anastasis_map_intelligence/tests/test_concordance.py
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSite.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSiteTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSettlementSurvey.cpp
- tools/unreal/geography-concordance-pie.py
- tools/unreal/proofs.txt (une inscription)
- AGENTS.md (une ligne d'index)
- docs/unreal/MAP_INTELLIGENCE.md
- docs/unreal/handoffs/geography-concordance-001.md

## COMMIT

Commit portant cette fiche : `git log -1 --format=%H -- docs/unreal/handoffs/geography-concordance-001.md`.
Le SHA admissible est celui marque par finish dans .handoff/geography-concordance-001.txt.

## MEC

- BUILD: PASS, UBT Result: Succeeded, 19 actions, 245.88 s (hors attente du build concurrent). Journal : Saved/CanonicalVerification/build.log.
- PYTHON_TESTS: 37 PASS, 0 FAIL (noyau, doublures de l'adaptateur, validation du diagnostic).
- PYTHON_SYNTAX: PASS (AST, sans chargement Unreal).
- TOOLS_INDEX: PASS, Missing=[], Stale=[].
- DIFF_CHECK: PASS.
- CPP_TESTS: QUEUED, dont Anastasis.SettlementSite.WaterConcordance ; compilation ne vaut pas execution.
- COMMANDS:
  - `tools/unreal/anastasis-unreal.ps1 build`
  - `$env:PYTHONPATH=(Join-Path (Get-Location) 'Content/Python')`
  - `& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -B -m unittest discover -s Content/Python/anastasis_map_intelligence/tests -v`
  - `. tools/unreal/tools-index.ps1; Test-AnastasisToolsIndex (Get-Location).Path`
  - `git diff --check`
  - `tools/unreal/agent-worktree.ps1 finish -Mission geography-concordance-001`

## PROOFS

PROOFS: geography-concordance-pie, settlement-site-pie

Premiere preuve : acquisition et integrite seulement. INSTRUMENT_PASS ne signifie pas
agreement : MISMATCH est un resultat scientifique valide. JSON brut + carte SVG dans
Saved/GeographyConcordanceEvidence. Deuxieme preuve existante : selection et ouverture
inchangees, trois captures ; aucun trajet de subsistance demontre par ces captures.
Aucun editeur lance pour cette mission. Respect de la file de preuves commune.

## SCN

UNKNOWN / QUEUED. Aucun chiffre de concordance du monde reel releve par cette mission.
La source prouve seulement que la selection lit deja les sections 1 + 2, tandis que
Map Intelligence ne lisait que la section 1. Les nouvelles mesures restent a acquerir.

## PLY

UNKNOWN. DrinkTarget privilegie un puits, puis eau/berge semantiques ; AtDrinkSpot
accepte aussi Shore. Type==Water n'est pas une carte exhaustive de buvabilite.
Le graphe du selecteur et son water_access ne prouvent pas un trajet PNJ execute.

## ECARTS

AUCUN — aucun fichier Source/AnastasisSim modifie ; aucune evolution de parite introduite.
Pas de revendication de nouvelle execution du harnais de parite.

## INTEGRATION_RISK

- Fichiers partages : AGENTS.md, proofs.txt, SettlementSite/Survey ; integration par lot uniquement.
- Format Map Intelligence v2 : anciens rapports INCOMPARABLES, regenerer une baseline.
- Anciens Settings.json gardent water_section ; additional_water_sections=[2] ajoute les rivieres.
- Centres de tuiles seulement : les chenaux etroits peuvent etre manques ; aucune aire d'eau deduite.
- Absence totale de maillage d'eau observable : concordance UNKNOWN, jamais accord implicite.
- JSON d'ouverture plus volumineux (indices des cinq classes), produit une seule fois au releve.
- La preuve de la slice attend sections 1 et 2 non vides/visibles ; un autre mode de rendu exige un autre protocole.

## STOP

Ne modifie ni relief, hydrologie, placement, navigation, regles PNJ, assets ni simulation.
Aucune integration/push automatique. STOP a HANDOFF_READY selon anastasis-mission.
NEXT apres acquisition du lot : choisir un ecart pres du site et prouver le trajet reel
vers sa source ; ne pas realigner le simulateur avec le rendu avant ce diagnostic.
