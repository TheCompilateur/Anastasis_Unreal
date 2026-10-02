# HANDOFF: riparian-transition-004

## MISSION
Transformer le melange ponctuel de laiches en colonies sur rives douces, sans ajouter de touffes.
Proxy disponible : humidite geometrique RiparianAt et hauteur relative a l'eau, pente rendue.
Echelle 12 m et seuils 6-18 deg : approximation visuelle, pas calibration botanique.

## FILES_OWNED
- Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.h
- Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCover.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisGroundCoverTests.cpp
- Source/Anastasis_UnrealV2/WorldView/AnastasisWorldEmbodiment.cpp (CVar et CoverSettings seulement)
- tools/unreal/ground-cover-capture.py
- tools/unreal/proofs.txt
- AGENTS.md (index capture seulement)
- .claude/skills/anastasis-capture/SKILL.md
- docs/unreal/handoffs/riparian-transition-004.md

## COMMIT
Commit portant cette fiche, branche agent/riparian-transition-004, base 47bcb3e.

## MEC
- BUILD: PASS sur 0b2bed5, UBT 59.87 s ; arbres Unreal identiques apres deplacement de la ligne du registre.
- TESTS: QUEUED, Anastasis.GroundCover.RiparianTransition (isolation sec, pente humide, budget, determinisme, immersion).
- Python AST et git diff --check avant commit.

## PROOFS
PROOFS: riparian-transition-capture

## SCN
UNKNOWN. Reference inspectee : Saved/VisualCrusade/suite-002-final/riviere_eye_on.png (ancien commit 4fef72d).
A/B a rejouer en lot ; neuf images prairie/riviere/aerien. Pas de gain artistique revendique.

## PLY
UNKNOWN. Ni marche ni acces PNJ verifies. Clairieres existantes conservees, aucun nouvel obstacle.

## ECARTS
AUCUN : Source/AnastasisSim non modifie.

## INTEGRATION_RISK
Conflit EOF du registre evite : entree riparian a cote de natural-history ; river-use-pie et soundscape-pie sont laisses a leurs missions.
Conflits possibles avec woodland-sequence-003 sur WorldEmbodiment, script de capture et registre.
Conserver ses ajouts : cette mission ne touche que GroundCover et son nouveau selecteur de famille.
Water-continuity et soil-contact inchanges. Aucun asset, relief, hydrologie ou arbre adulte modifie.
Dressing.RiparianTransition=0 retrouve la reference. Nombre constant ne prouve pas GPU constant.

## STOP
HANDOFF_READY queued seulement. Integration reservee a l'integrateur unique.
