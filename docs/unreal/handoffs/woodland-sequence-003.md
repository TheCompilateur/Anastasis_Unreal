# HANDOFF: woodland-sequence-003

## MISSION

Sequence prairie / lisiere / couvert / ouverture, depuis main 4fef72d. Worktree woodland-sequence-003, branche agent/woodland-sequence-003, port 8829.
Regrouper la regeneration et les ages de la strate secondaire autour des vrais peuplements. Conserver adultes, herbe, grandes ouvertures, terrain, eau et simulation. Aucun asset cree ou ecrit.
CVar anastasis.Dressing.WoodlandSequence : 0 = reference ecotone-002 ; 1 = cohortes et budget forestier borne.

## FILES_OWNED

- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcology.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisMicroEcologyTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (CVar + EcoSettings seulement)
- tools/unreal/ground-cover-capture.py
- tools/unreal/proofs.txt
- AGENTS.md
- .claude/skills/anastasis-capture/SKILL.md
- docs/unreal/handoffs/woodland-sequence-003.md

## COMMIT

Voir le commit contenant cette fiche.

## MEC

- Base BUILD PASS112.94s ; candidat BUILD PASS44.11s ; signal de plafonnement corrige, dernier BUILD PASS44.67s.
- AST capture PASS ; diff --check PASS.
- Test ajoute Anastasis.MicroEcology.WoodlandIsolation : compile, execution QUEUED.
- Le plan ecotone reference conserve integralement sa selection non forestiere (poses comprises). Son nombre forestier retenu borne le nouveau budget. Pas de quota riverain invente.
- Regeneration reservee aux groupes (trois couronnes voisines dans 24m), transition progressive par soutien des couronnes, poches de 18m partageant role et hauteur, exclusions hydriques/pentes/clairieres conservees.
- Debris organiques sous les couronnes ; trouees entre les cohortes. Herbe et materiau du sol inchanges.
- Reutilisation de SM_Tree_Broadleaf_Understory_01 pour les jeunes individus (hauteur cible 1.3-3.6m avec variation bornee). Approximation morphologique, aucune attribution botanique historique.
- HISM transitoires sans collision et sans influence navigation ; aucun Tick.

## PROOFS

PROOFS: woodland-sequence-capture

## SCN

UNKNOWN, pilote en attente du creneau partage. Ne pas assimiler HANDOFF_READY queued a un KEEP artistique. Observer le pilote avant decision artistique ; les sessions externes eau/Sim ont retarde le creneau.
KEEP : groupements et transition perceptibles au sol et en oblique, ouvertures conservees, aucune inflation globale ; berges/prairie positions identiques a la reference.
REJECT : distribution illisible, tapis ou encombrement uniforme, silhouettes incoherentes, regression de cout disproportionnee.
4 poses humaines dont ouverture locale, 1 oblique, prairie temoin ; A/B/reference2 ; deplacement camera de 10m a 1.7m, distinct d'une preuve joueur.

## PLY

UNKNOWN : aucune traversee jouee revendiquee.

## ECARTS

AUCUN : Source/AnastasisSim non modifie, aucune nouvelle revendication de parite.

## INTEGRATION_RISK

- WorldEmbodiment partage : ne touche pas l'insertion SoilContact apres PlaceRiverbank.
- Registre/index : preserver toutes entrees concurrentes lors du rebase.
- Fonctionne avec TreeCanopyEcotone actif ; son retrait conserve le chemin historique.
- La preservation des non-forestiers est relative au meme plan d'entree. Un futur deplacement des adultes changera leur habitat et exigera une nouvelle observation.

## STOP

Aucune nouvelle botanique, hydrologie, topographie ou navigation. Aucun gain artistique avant observation ; aucun gain performance anticipe. Pas d'integration autonome.
