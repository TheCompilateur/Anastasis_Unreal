# HANDOFF: river-use-001

## MISSION

Preparer l'experience « une riviere utile aux habitants » : un PNJ autonome assoiffe,
sans puits, depuis un depart sec jusqu'a une berge choisie physiquement. Observer
reconnaissance, trajet, consommation et baisse de soif, sans imposer but ou cible.
Aucun correctif de comportement tant que le premier maillon en echec n'est pas observe.

Root : C:/dev/ANASTASIS_WORKTREES/river-use-001 ; branche agent/river-use-001.
Base : 73248bc (main, avant les integrations en cours). Owner : cette mission.
Canonique et tous les autres worktrees exclus. Fichiers locaux canoniques exclus :
.claude/settings.local.json et download.png.
Procedure : .claude/skills/anastasis-mission/SKILL.md ; aucune integration/push ici.

## FILES_OWNED

- Source/Anastasis_UnrealV2/Sim/AnastasisRiverUseProbe.h
- Source/Anastasis_UnrealV2/Sim/AnastasisRiverUseProbe.cpp
- Content/Python/anastasis_river_use.py
- Content/Python/tests/test_river_use.py
- tools/unreal/river-use-pie.py
- tools/unreal/proofs.txt (une inscription)
- AGENTS.md (une ligne d'index)
- docs/unreal/RIVER_USE_001.md
- docs/unreal/handoffs/river-use-001.md

## COMMIT

Commit portant cette fiche : `git log -1 --format=%H -- docs/unreal/handoffs/river-use-001.md`.
Le commit admis est marque par finish dans .handoff/river-use-001.txt.

## MEC

- BUILD: PASS, Result: Succeeded, recompilation 4 actions / 112.93 s hors attente. Journal Saved/CanonicalVerification/build.log. Premier essai en echec sur collision du nom Error avec Unreal, corrigee en RiverProbeError.
- TESTS_PYTHON: 17 PASS, 0 FAIL. Geometrie et refus des faux succes : lac seul,
  ruban enfoui, sol inconnu/inonde, teletransport, puits/joueur, compteur sans effet,
  autre rive, cible absente, lacune temporelle. Ce sont des tests synthetiques.
- AST: PASS pour noyau, tests et script PIE.
- TOOLS_INDEX: PASS, Missing=[], Stale=[].
- BATCH_DRYRUN: PASS, une preuve river-use-pie, sans editeur.
- CPP_AUTOMATION / LIVE_PROOF: QUEUED, aucune execution revendiquee.
- COMMANDS:
  - `tools/unreal/anastasis-unreal.ps1 build`
  - `$env:PYTHONPATH=(Join-Path (Get-Location) 'Content/Python')`
  - `& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -B -m unittest discover -s Content/Python/tests -p test_river_use.py -v`
  - `tools/unreal/editor-batch.ps1 -Proofs river-use-pie -DryRun`
  - `. tools/unreal/tools-index.ps1; Test-AnastasisToolsIndex (Get-Location).Path`
  - `git diff --check`
  - `tools/unreal/agent-worktree.ps1 finish -Mission river-use-001`

## PROOFS

PROOFS: river-use-pie

PASS uniquement si trajet autonome >=20 m, intention/cible shore observees,
consommation accrue et baisse de soif >=10, sol sec/pente acceptable, boisson proche
de l'eau exposee choisie, geometrie stable, scenario sans puits/autre habitant/joueur.
UNKNOWN ne passe pas le registre. Echec conserve brut dans Saved/RiverUseEvidence/<time_ns>/.
Seuils complets et limites : docs/unreal/RIVER_USE_001.md.

## SCN

UNKNOWN. Aucune mesure sur la carte vivante, aucun trajet execute dans cette mission.
Le futur verdict est local a une berge et a ce protocole. Les maillages CPU ne prouvent
pas la visibilite pixel, et les segments inter-observations sont echantillonnes.

## PLY

UNKNOWN. Pas d'incarnation ni d'essai joueur. Le PNJ reste autonome.

## ECARTS

AUCUN — Source/AnastasisSim inchange ; utilisation des API existantes SpawnNpc et
lectures uniquement. Besoins initiaux scenario dans l'hote Unreal ; aucun but impose.
Aucun nouveau claim de parite validee.

## INTEGRATION_RISK

- Faire preceder cette preuve par geography-concordance-pie de geography-concordance-001
  (f5e6fe6, encore queued lors de la preparation). Pas de resultat suppose ni recopie.
  Aucun besoin de cherry-pick de cette mission pour compiler river-use-001 ; le rapport
  de site contient automatiquement sa concordance quand les deux missions sont versees.
- Conflits attendus possibles sur les deux lignes ajoutees AGENTS.md/proofs.txt.
- Cette preuve peut echouer LEGITIMEMENT sur le monde courant ; elle teste une hypothese,
  elle ne garantit pas un resultat. Un UNKNOWN de precondition ou de frequence
  d'observation bloque aussi le lot. Lire journey.json avant tout patch.
- StartVillagers est restaure des que le monde PIE existe (son lancement a deja lu 0).
  Le rythme est restaure a la fin normale/exception et par le runner entre travaux.
  Un timeout avant meme le monde PIE doit rester un lot en echec. Aucun editeur lance.
- Begin refuse hors PIE et village non vide ; pas de reset ni de suppression d'habitants.
- Le pilote choisit une berge geometrique pres du site de village ; il ne selectionne
  pas une berge deja prouvee utilisable. L'eau sous un lac ou enfouie est rejetee.
- Tous les exports sont generes non commites. Cible de performance : temps borne a
  60 s simulees / 360 s murales ; la preparation geometrie et la frequence necessitent
  encore une mesure dans l'editeur commun.

## STOP

STOP apres finish / HANDOFF_READY. L'integrateur designe rejoue la preuve.
Pas de correctif speculatif de l'hydrologie, de navigation ou de decision PNJ.
NEXT : lire le verdict et la trace, identifier le premier maillon effectivement
manquant ; corriger celui-la seul, ou cloturer sans patch si le trajet passe.
