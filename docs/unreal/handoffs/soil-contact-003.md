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
PROOFS: (aucune)

## SCN
UNKNOWN : pas encore de capture. Aucun KEEP artistique revendique.
Creneau occupe par editeur interactif canonique PID42172 (reverifie avant arret).
Ordre coordonne : anthropic-paths-002 puis soil-contact-003. Aucun editeur lance
par cette mission, aucun processus tiers ferme. Pas de finish ni de main avance.
Commande preparee apres liberation confirmee (MAIN.lock libre, MAX1) :
$env:ANASTASIS_EDITOR_MAX='1'
$env:ANASTASIS_SOIL_CONTACT='1'
tools/soil-crusade/capture.ps1 -Label contact-v1
15 images attendues : contact_walk00..02, contact_side, contact_context, etats
soil_before/soil_after/soil_control. Trois poses fixes ne prouvent pas le deplacement PIE.

## PLY
UNKNOWN. Couche decorative sans collision, aucune marche joueur prouvee.

## ECARTS
AUCUN : aucun fichier Source/AnastasisSim modifie.

## INTEGRATION_RISK
WorldEmbodiment partage : conserver les appels concurrents, hunk limite include + Apply.
HISM transitoires reutilises apres ClearInstances, pas de destruction pendant rebuild async.
Pilote uniquement seed12345 scale5; repli vide si aucun site satisfait pente + eau reelles.
Pas une reconstruction geologique, pas un transport de sediments simule.
Exclusions : assets M/MI Ground, Ecotone, Riverbank, atmosphere et travail concurrent intacts.

## STOP
Ne pas integrer avant observation A/B et verdict. Ne pas generaliser sur toute la carte.
Pas de nouveau systeme de geologie ni de nouveau mesh. Pas de gain FPS garanti.
