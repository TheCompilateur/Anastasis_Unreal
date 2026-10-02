# HANDOFF: soil-contact-003

## MISSION
Pilot raccord affleurement / debris / rive a hauteur humaine, sans changer relief,
collision, vegetation, materiaux existants ou simulation. Base 4fef72d526ee0be45a923c46c9e626dc3a6e7deb.
Worktree C:/dev/ANASTASIS_WORKTREES/soil-contact-003, branche agent/soil-contact-003, port8551.

## FILES_OWNED
- Source/Anastasis_UnrealV2/WorldView/AnastasisSoilContact.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisSoilContact.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisSoilContactTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (include + Apply apres PlaceRiverbank uniquement)
- tools/soil-crusade/capture.py
- tools/soil-crusade/README.md
- tools/unreal/proofs.txt (soil-contact-capture)
- docs/unreal/handoffs/soil-contact-003.md

## COMMIT
HEAD de agent/soil-contact-003 (fiche dans le commit).

## KEEP_REJECT
KEEP seulement si la famille rocheuse semble emerger de la pente, avec debris plus
petits vers l'eau, sans blocs flottants, cordon regulier ni barrage visuel. Examiner
les cinq triplets, dont trois poses successives a1.7m. Materiaux et lumiere identiques.
Mesurer l'effet contre derive avant/temoin; deltaGPU cible<=0.5ms hors derive.
REJECT si amas decoratif, vegetation masquant tout gain, ou raccord roche/sol plus artificiel.

## MEC
Premier BUILD::PASS (141.06s). Second BUILD::PASS (33.94s), test Support compile; execution non faite.
PROOFS: soil-contact-capture

## SCN
UNKNOWN : pas encore de capture. Aucun KEEP artistique revendique.
Reprise : preuve enregistree pour execution groupee, pilote DEFAULT OFF.
La capture active temporairement SoilContact puis restaure sa CVar et la visibilite.
Commande integrateur : editor-batch.ps1 -Proofs soil-contact-capture
Sorties Saved/SoilEvidence/contact-batch; aucune sauvegarde d'asset.
15 images attendues : contact_walk00..02, contact_side, contact_context, etats
soil_before/soil_after/soil_control. Trois poses fixes ne prouvent pas le deplacement PIE.

## PLY
UNKNOWN. Couche decorative sans collision, aucune marche joueur prouvee.

## ECARTS
AUCUN : aucun fichier Source/AnastasisSim modifie.

## INTEGRATION_RISK
Rebase main47bcb3e; conflit du lot identifie uniquement sur proofs.txt (ajouts en fin).
Entree sol placee en tete du registre, sans modifier les entrees concurrentes.
WorldEmbodiment partage : conserver les appels concurrents, hunk limite include + Apply.
HISM transitoires reutilises apres ClearInstances, pas de destruction pendant rebuild async.
Pilote uniquement seed12345 scale5; repli vide si aucun site satisfait pente + eau reelles.
Pas une reconstruction geologique, pas un transport de sediments simule.
Exclusions : assets M/MI Ground, Ecotone, Riverbank, atmosphere et travail concurrent intacts.

## STOP
Seul le candidat DEFAULT OFF peut etre assemble pour produire la preuve.
Ne pas activer ni generaliser avant observation A/B et verdict artistique.
COMPLETE prouve les images produites, pas leur qualite ni la fermeture saine de l editeur.
Pas de nouveau systeme de geologie ni de nouveau mesh. Pas de gain FPS garanti.
